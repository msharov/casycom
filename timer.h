// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#pragma once
#include "main.h"
#include <sys/poll.h>
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// Timer protocol constants

enum ETimerWatchCmd {
    WATCH_STOP              = 0,
    WATCH_READ              = POLLIN,
    WATCH_WRITE             = POLLOUT, 
    WATCH_RDWR              = WATCH_READ| WATCH_WRITE,
    WATCH_TIMER             = POLLMSG,
    WATCH_READ_TIMER        = WATCH_READ| WATCH_TIMER,
    WATCH_WRITE_TIMER       = WATCH_WRITE| WATCH_TIMER,
    WATCH_RDWR_TIMER        = WATCH_RDWR| WATCH_TIMER
};
typedef uint64_t casytimer_t;
enum {
    TIMER_MAX = INT64_MAX,
    TIMER_NONE = UINT64_MAX
};

//----------------------------------------------------------------------
// PTimer

typedef void (*MFN_Timer_watch)(void* vo, enum ETimerWatchCmd cmd, int fd, casytimer_t timer);
typedef struct _DTimer {
    iid_t		interface;
    MFN_Timer_watch	Timer_watch;
} DTimer;

extern const Interface i_Timer;

//----------------------------------------------------------------------

void PTimer_watch (const Proxy* pp, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms);

//----------------------------------------------------------------------
// PTimerR

typedef void (*MFN_TimerR_timer)(void* vo, int fd, const Msg* msg);
typedef struct _DTimerR {
    iid_t		interface;
    MFN_TimerR_timer	TimerR_timer;
} DTimerR;

extern const Interface i_TimerR;

//----------------------------------------------------------------------

void PTimerR_timer (const Proxy* pp, int fd);

//----------------------------------------------------------------------

extern const Factory f_Timer;

bool		Timer_run_timer (int toWait) noexcept;
casytimer_t	Timer_now (void) noexcept;
size_t		Timer_watch_list_size (void) noexcept;
size_t		Timer_watch_list_for_poll (struct pollfd* fds, size_t fdslen, int* timeout) noexcept NONNULL(1);

//----------------------------------------------------------------------
// PTimer inlines

#ifdef __cplusplus
namespace {
#endif

static inline void PTimer_stop (const Proxy* pp)
    { PTimer_watch (pp, WATCH_STOP, -1, TIMER_NONE); }
static inline void PTimer_timer (const Proxy* pp, casytimer_t timeoutms)
    { PTimer_watch (pp, WATCH_TIMER, -1, timeoutms); }
static inline void PTimer_wait_read (const Proxy* pp, int fd)
    { PTimer_watch (pp, WATCH_READ, fd, TIMER_NONE); }
static inline void PTimer_wait_write (const Proxy* pp, int fd)
    { PTimer_watch (pp, WATCH_WRITE, fd, TIMER_NONE); }
static inline void PTimer_wait_rdwr (const Proxy* pp, int fd)
    { PTimer_watch (pp, WATCH_RDWR, fd, TIMER_NONE); }
static inline void PTimer_wait_read_with_timeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_watch (pp, WATCH_READ_TIMER, fd, timeoutms); }
static inline void PTimer_wait_write_with_timeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_watch (pp, WATCH_WRITE_TIMER, fd, timeoutms); }
static inline void PTimer_wait_rdwr_with_timeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_watch (pp, WATCH_RDWR_TIMER, fd, timeoutms); }

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
