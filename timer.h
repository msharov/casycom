// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

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

typedef void (*MFN_Timer_Watch)(void* vo, enum ETimerWatchCmd cmd, int fd, casytimer_t timer);
typedef struct _DTimer {
    iid_t		interface;
    MFN_Timer_Watch	Timer_Watch;
} DTimer;

extern const Interface i_Timer;

//----------------------------------------------------------------------

void PTimer_Watch (const Proxy* pp, enum ETimerWatchCmd cmd, int fd, casytimer_t timeoutms);

//----------------------------------------------------------------------
// PTimerR

typedef void (*MFN_TimerR_Timer)(void* vo, int fd, const Msg* msg);
typedef struct _DTimerR {
    iid_t		interface;
    MFN_TimerR_Timer	TimerR_Timer;
} DTimerR;

extern const Interface i_TimerR;

//----------------------------------------------------------------------

void PTimerR_Timer (const Proxy* pp, int fd);

//----------------------------------------------------------------------

extern const Factory f_Timer;

bool		Timer_RunTimer (int toWait) noexcept;
casytimer_t	Timer_NowMS (void) noexcept;
size_t		Timer_WatchListSize (void) noexcept;
size_t		Timer_WatchListForPoll (struct pollfd* fds, size_t fdslen, int* timeout) noexcept NONNULL(1);

//----------------------------------------------------------------------
// PTimer inlines

#ifdef __cplusplus
namespace {
#endif

static inline void PTimer_Stop (const Proxy* pp)
    { PTimer_Watch (pp, WATCH_STOP, -1, TIMER_NONE); }
static inline void PTimer_Timer (const Proxy* pp, casytimer_t timeoutms)
    { PTimer_Watch (pp, WATCH_TIMER, -1, timeoutms); }
static inline void PTimer_WaitRead (const Proxy* pp, int fd)
    { PTimer_Watch (pp, WATCH_READ, fd, TIMER_NONE); }
static inline void PTimer_WaitWrite (const Proxy* pp, int fd)
    { PTimer_Watch (pp, WATCH_WRITE, fd, TIMER_NONE); }
static inline void PTimer_WaitRdWr (const Proxy* pp, int fd)
    { PTimer_Watch (pp, WATCH_RDWR, fd, TIMER_NONE); }
static inline void PTimer_WaitReadWithTimeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_Watch (pp, WATCH_READ_TIMER, fd, timeoutms); }
static inline void PTimer_WaitWriteWithTimeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_Watch (pp, WATCH_WRITE_TIMER, fd, timeoutms); }
static inline void PTimer_WaitRdWrWithTimeout (const Proxy* pp, int fd, casytimer_t timeoutms)
    { PTimer_Watch (pp, WATCH_RDWR_TIMER, fd, timeoutms); }

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
