// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "timer.h"
#include "main.h"
#include "vector.h"
#include <time.h>

//----------------------------------------------------------------------
// Timer interface

enum { method_Timer_watch };

void PTimer_watch (const Proxy* pp, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    assert (pp->interface == &i_Timer && "the given proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_Timer_watch, 16);
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, cmd);
    casystm_write_int32 (&os, fd);
    casystm_write_uint64 (&os, timeoutms);
    casymsg_end (msg);
}

static void Timer_dispatch (const DTimer* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_Timer_watch) {
	RStm is = casymsg_read (msg);
	enum ETimerWatchCmd cmd = casystm_read_uint32 (&is);
	int fd = casystm_read_int32 (&is);
	casytimer_t timeoutms = casystm_read_uint64 (&is);
	dtable->Timer_watch (o, cmd, fd, timeoutms);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_Timer = {
    .name	= "Timer",
    .dispatch	= Timer_dispatch,
    .method	= { "watch\0uix", NULL }
};

//----------------------------------------------------------------------
// TimerR interface

enum { method_TimerR_timer };

void PTimerR_timer (const Proxy* pp, int fd)
{
    Msg* msg = casymsg_begin (pp, method_TimerR_timer, 4);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

static void TimerR_dispatch (const DTimerR* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_TimerR_timer) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	dtable->TimerR_timer (o, fd, msg);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_TimerR = {
    .name	= "TimerR",
    .dispatch	= TimerR_dispatch,
    .method	= { "timer\0i", NULL }
};

//----------------------------------------------------------------------
// The Timer server object

typedef struct _Timer {
    Proxy		reply;
    casytimer_t		nextfire;
    enum ETimerWatchCmd	cmd;
    int			fd;
} Timer;

// Global list of pointers to active timer objects
DECLARE_VECTOR_TYPE (WatchList, Timer*);
static VECTOR(WatchList, _timer_watch_list);

//----------------------------------------------------------------------

void* Timer_create (const Msg* msg)
{
    Timer* o = xalloc (sizeof(Timer));
    o->reply = casycom_create_reply_proxy (&i_TimerR, msg);
    o->nextfire = TIMER_NONE;
    o->cmd = WATCH_STOP;
    o->fd = -1;
    vector_push_back (&_timer_watch_list, &o);
    return o;
}

void Timer_destroy (void* vo)
{
    Timer* o = vo;
    for (int i = _timer_watch_list.size; --i >= 0;)
	if (_timer_watch_list.d[i] == o)
	    vector_erase (&_timer_watch_list, i);
    xfree (o);
    if (!_timer_watch_list.size)
	vector_deallocate (&_timer_watch_list);
}

void Timer_Timer_watch (Timer* o, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    o->cmd = cmd;
    o->fd = fd;
    if (timeoutms <= TIMER_MAX)
	timeoutms += Timer_now();
    o->nextfire = timeoutms;
}

#ifndef NDEBUG
static const char* timestring (casytimer_t t)
{
    t /= 1000;
    static char tbuf[32] = {};
    #if UC_VERSION
	ctime_r (t, tbuf);
    #else
	ctime_r ((const time_t*) &t, tbuf);
    #endif
    tbuf[strlen(tbuf)-1] = 0;	// remove newline
    return tbuf;
}
#endif

/// Waits for timer or fd events.
/// toWait specifies the minimum timeout in milliseconds.
bool Timer_run_timer (int toWait)
{
    if (!_timer_watch_list.size)
	return false;
    // Populate the fd list and find the nearest timer
    struct pollfd fds [_timer_watch_list.size];
    size_t nFds = 0;
    casytimer_t nearest = TIMER_MAX;
    for (size_t i = 0; i < _timer_watch_list.size; ++i) {
	const Timer* we = _timer_watch_list.d[i];
	if (we->nextfire < nearest)
	    nearest = we->nextfire;
	if (we->fd >= 0 && we->cmd != WATCH_STOP) {
	    fds[nFds].fd = we->fd;
	    fds[nFds].events = we->cmd;
	    fds[nFds].revents = 0;
	    ++nFds;
	}
    }
    // Calculate how long to wait
    if (toWait && nearest < TIMER_MAX)	// toWait could be zero, in which case don't
	toWait = nearest - Timer_now();
    // And wait
    if (DEBUG_MSG_TRACE) {
	DEBUG_PRINTF ("[I] Waiting for %zu file descriptors from %zu timers", nFds, _timer_watch_list.size);
	if (toWait > 0)
	    DEBUG_PRINTF (" with %d ms timeout", toWait);
	DEBUG_PRINTF (". %s\n", timestring(Timer_now()));
    }
    // And poll
    poll (fds, nFds, toWait);
    // Poll errors are checked for each fd with POLLERR. Other errors are ignored.
    // poll will exit when there are fds available or when the timer expires
    const casytimer_t now = Timer_now();
    for (size_t i = 0, fdi = 0; i < _timer_watch_list.size; ++i) {
	const Timer* we = _timer_watch_list.d[i];
	bool bFired = we->nextfire <= now;	// Check timer expiration
	if (DEBUG_MSG_TRACE && bFired) {
	    DEBUG_PRINTF("[T]\tTimer %s", timestring(we->nextfire));
	    DEBUG_PRINTF(" fired at %s\n", timestring(now));
	}
	if (we->fd >= 0 && we->cmd != WATCH_STOP) {
	    // Check if fd fired, or has errors. Errors will be detected when the client tries to use the fd.
	    if (fds[fdi].revents & (POLLERR| we->cmd))
		bFired = true;
	    if (DEBUG_MSG_TRACE) {
		if (fds[fdi].revents & POLLIN)
		    DEBUG_PRINTF("[T]\tFile descriptor %d can be read\n", fds[fdi].fd);
		if (fds[fdi].revents & POLLOUT)
		    DEBUG_PRINTF("[T]\tFile descriptor %d can be written\n", fds[fdi].fd);
		if (fds[fdi].revents & POLLMSG)
		    DEBUG_PRINTF("[T]\tFile descriptor %d has extra data\n", fds[fdi].fd);
		if (fds[fdi].revents & POLLERR)
		    DEBUG_PRINTF("[T]\tFile descriptor %d has errors\n", fds[fdi].fd);
	    }
	    ++fdi;
	}
	if (bFired) {	// Once the timer or the fd fires, it is removed
	    PTimerR_timer (&we->reply, we->fd);
	    casycom_mark_unused (we);	// ... in the next idle
	}
    }
    return _timer_watch_list.size;
}

size_t Timer_watch_list_size (void)
{
    return _timer_watch_list.size;
}

size_t Timer_watch_list_for_poll (struct pollfd* fds, size_t fdslen, int* timeout)
{
    size_t nFds = 0;
    casytimer_t nearest = TIMER_MAX;
    for (size_t i = 0; i < _timer_watch_list.size; ++i) {
	const Timer* we = _timer_watch_list.d[i];
	if (we->nextfire < nearest)
	    nearest = we->nextfire;
	if (we->fd >= 0 && we->cmd != WATCH_STOP && nFds < fdslen) {
	    fds[nFds].fd = we->fd;
	    fds[nFds].events = we->cmd;
	    fds[nFds].revents = 0;
	    ++nFds;
	}
    }
    if (timeout) {
	casytimer_t now = Timer_now();
        *timeout = (now < nearest ? (int)(nearest - now) : -1);
    }
    return nFds;
}

/// Returns current time in milliseconds
casytimer_t Timer_now (void)
{
    struct timespec t;
    if (0 > clock_gettime (CLOCK_REALTIME, &t))
	return 0;
    return (casytimer_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

//----------------------------------------------------------------------

static const DTimer d_Timer_Timer = {
    .interface = &i_Timer,
    DMETHOD (Timer, Timer_watch)
};
const Factory f_Timer = {
    .create	= Timer_create,
    .destroy	= Timer_destroy,
    .dtable	= { &d_Timer_Timer, NULL }
};
