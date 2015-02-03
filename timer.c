// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "timer.h"
#include "main.h"
#include <time.h>

//----------------------------------------------------------------------
// Timer interface

void PTimer_Watch (const PProxy* pp, enum EWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    SMsg* msg = casymsg_begin (pp, method_Timer_Watch, 16);
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, cmd);
    casystm_write_int32 (&os, fd);
    casystm_write_uint64 (&os, timeoutms);
    casymsg_end (msg);
}

static void Timer_Dispatch (const DTimer* dtable, void* o, const SMsg* msg)
{
    if (msg->imethod == method_Timer_Watch) {
	RStm is = casymsg_read (msg);
	enum EWatchCmd cmd = casystm_read_uint32 (&is);
	int fd = casystm_read_int32 (&is);
	casytimer_t timeoutms = casystm_read_uint64 (&is);
	dtable->Timer_Watch (o, cmd, fd, timeoutms, msg);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_Timer = {
    .name	= "Timer",
    .dispatch	= Timer_Dispatch,
    .method	= { "Watch\0uix", NULL }
};

//----------------------------------------------------------------------
// TimerR interface

void PTimerR_Timer (const PProxy* pp, int fd)
{
    SMsg* msg = casymsg_begin (pp, method_TimerR_Timer, 4);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

static void TimerR_Dispatch (const DTimerR* dtable, void* o, const SMsg* msg)
{
    if (msg->imethod == method_TimerR_Timer) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	dtable->TimerR_Timer (o, fd, msg);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_TimerR = {
    .name	= "TimerR",
    .dispatch	= TimerR_Dispatch,
    .method	= { "Timer\0i", NULL }
};

//----------------------------------------------------------------------
// The Timer server object

typedef struct _STimer {
    PProxy		reply;
    casytimer_t		nextfire;
    enum EWatchCmd	cmd;
    int			fd;
} STimer;

// Global list of pointers to active timer objects
DECLARE_VECTOR_TYPE (WatchList, STimer*);
static VECTOR(WatchList, _timer_WatchList);

//----------------------------------------------------------------------

void* Timer_Create (const SMsg* msg)
{
    STimer* o = (STimer*) xalloc (sizeof(STimer));
    o->reply = casycom_create_reply_proxy (&i_TimerR, msg);
    o->nextfire = TIMER_NONE;
    o->cmd = WATCH_STOP;
    o->fd = -1;
    vector_push_back (&_timer_WatchList, o);
    return o;
}

void Timer_Destroy (void* vo)
{
    STimer* o = (STimer*) vo;
    for (int i = _timer_WatchList.size; --i >= 0;)
	if (_timer_WatchList.d[i] == o)
	    vector_erase (&_timer_WatchList, i);
    xfree (o);
    if (!_timer_WatchList.size)
	vector_deallocate (&_timer_WatchList);
}

void Timer_Timer_Watch (STimer* o, enum EWatchCmd cmd, int fd, casytimer_t timeoutms)
{
    o->cmd = cmd;
    o->fd = fd;
    o->nextfire = timeoutms + Timer_NowMS();
}

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
	const STimer* we = _timer_WatchList.d[i];
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
    if (0 > poll (fds, nFds, toWait))
	casycom_error ("poll: %s", strerror(errno));
    // poll will exit when there are fds available or when the timer expires
    const casytimer_t now = Timer_NowMS();
    for (size_t i = 0, fdi = 0; i < _timer_WatchList.size; ++i) {
	const STimer* we = _timer_WatchList.d[i];
	bool bFired = we->nextfire <= now;	// First, check timer expiration
	if (we->fd >= 0 && we->cmd != WATCH_STOP) {
	    if (fds[fdi].revents & POLLERR) {	// Second, check for fd errors
		casycom_error ("poll: invalid file descriptor");
		casycom_forward_error (we->reply.dest, we->reply.src);
	    } else if (fds[fdi].revents & we->cmd)	// Third, check the fd
		bFired = true;
	    ++fdi;
	}
	if (bFired)	// Once the timer or the fd fires, it is removed
	    casycom_mark_unused (we);	// ... in the next idle
    }
    return _timer_WatchList.size;
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

static const DTimer d_TimerObject_Timer = {
    .interface = &i_Timer,
    DMETHOD (Timer, Timer_Watch)
};
const SObject o_TimerObject = {
    .Create	= Timer_Create,
    .Destroy	= Timer_Destroy,
    .interface	= { &d_TimerObject_Timer, NULL }
};
