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

enum { method_Timer_Watch };

void PTimer_Watch (const Proxy* pp, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    assert (pp->interface == &i_Timer && "the given proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_Timer_Watch, 16);
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, cmd);
    casystm_write_int32 (&os, fd);
    casystm_write_uint64 (&os, timeoutms);
    casymsg_end (msg);
}

static void Timer_Dispatch (const DTimer* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_Timer_Watch) {
	RStm is = casymsg_read (msg);
	enum ETimerWatchCmd cmd = casystm_read_uint32 (&is);
	int fd = casystm_read_int32 (&is);
	casytimer_t timeoutms = casystm_read_uint64 (&is);
	dtable->Timer_Watch (o, cmd, fd, timeoutms);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_Timer = {
    .name	= "Timer",
    .dispatch	= Timer_Dispatch,
    .method	= { "Watch\0uix", NULL }
};

//----------------------------------------------------------------------
// TimerR interface

enum { method_TimerR_Timer };

void PTimerR_Timer (const Proxy* pp, int fd)
{
    Msg* msg = casymsg_begin (pp, method_TimerR_Timer, 4);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

static void TimerR_Dispatch (const DTimerR* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_TimerR_Timer) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	dtable->TimerR_Timer (o, fd, msg);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_TimerR = {
    .name	= "TimerR",
    .dispatch	= TimerR_Dispatch,
    .method	= { "Timer\0i", NULL }
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
static VECTOR(WatchList, _timer_WatchList);

//----------------------------------------------------------------------

void* Timer_Create (const Msg* msg)
{
    Timer* o = xalloc (sizeof(Timer));
    o->reply = casycom_create_reply_proxy (&i_TimerR, msg);
    o->nextfire = TIMER_NONE;
    o->cmd = WATCH_STOP;
    o->fd = -1;
    vector_push_back (&_timer_WatchList, &o);
    return o;
}

void Timer_Destroy (void* vo)
{
    Timer* o = vo;
    for (int i = _timer_WatchList.size; --i >= 0;)
	if (_timer_WatchList.d[i] == o)
	    vector_erase (&_timer_WatchList, i);
    xfree (o);
    if (!_timer_WatchList.size)
	vector_deallocate (&_timer_WatchList);
}

void Timer_Timer_Watch (Timer* o, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    o->cmd = cmd;
    o->fd = fd;
    if (timeoutms <= TIMER_MAX)
	timeoutms += Timer_NowMS();
    o->nextfire = timeoutms;
}

#ifndef NDEBUG
static const char* timestring (casytimer_t t)
{
    t /= 1000;
    static char tbuf [32];
    memset (tbuf, 0, sizeof(tbuf));
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
bool Timer_RunTimer (int toWait)
{
    if (!_timer_WatchList.size)
	return false;
    // Populate the fd list and find the nearest timer
    struct pollfd fds [_timer_WatchList.size];
    size_t nFds = 0;
    casytimer_t nearest = TIMER_MAX;
    for (size_t i = 0; i < _timer_WatchList.size; ++i) {
	const Timer* we = _timer_WatchList.d[i];
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
	toWait = nearest - Timer_NowMS();
    // And wait
    if (DEBUG_MSG_TRACE) {
	DEBUG_PRINTF ("[I] Waiting for %zu file descriptors from %zu timers", nFds, _timer_WatchList.size);
	if (toWait > 0)
	    DEBUG_PRINTF (" with %d ms timeout", toWait);
	DEBUG_PRINTF (". %s\n", timestring(Timer_NowMS()));
    }
    // And poll
    poll (fds, nFds, toWait);
    // Poll errors are checked for each fd with POLLERR. Other errors are ignored.
    // poll will exit when there are fds available or when the timer expires
    const casytimer_t now = Timer_NowMS();
    for (size_t i = 0, fdi = 0; i < _timer_WatchList.size; ++i) {
	const Timer* we = _timer_WatchList.d[i];
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
	    PTimerR_Timer (&we->reply, we->fd);
	    casycom_mark_unused (we);	// ... in the next idle
	}
    }
    return _timer_WatchList.size;
}

size_t Timer_WatchListSize (void)
{
    return _timer_WatchList.size;
}

size_t Timer_WatchListForPoll (struct pollfd* fds, size_t fdslen, int* timeout)
{
    size_t nFds = 0;
    casytimer_t nearest = TIMER_MAX;
    for (size_t i = 0; i < _timer_WatchList.size; ++i) {
	const Timer* we = _timer_WatchList.d[i];
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
	casytimer_t now = Timer_NowMS();
        *timeout = (now < nearest ? (int)(nearest - now) : -1);
    }
    return nFds;
}

/// Returns current time in milliseconds
casytimer_t Timer_NowMS (void)
{
    struct timespec t;
    if (0 > clock_gettime (CLOCK_REALTIME, &t))
	return 0;
    return (casytimer_t) t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

//----------------------------------------------------------------------

static const DTimer d_Timer_Timer = {
    .interface = &i_Timer,
    DMETHOD (Timer, Timer_Watch)
};
const Factory f_Timer = {
    .Create	= Timer_Create,
    .Destroy	= Timer_Destroy,
    .dtable	= { &d_Timer_Timer, NULL }
};
