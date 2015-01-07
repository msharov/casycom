// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "config.h"
#include "casycom.h"
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#if HAVE_EXECINFO_H
    #include <execinfo.h>
#endif

//{{{ Module globals ---------------------------------------------------

/// Last non-fatal signal. Set by signal handler, read by main loop.
static int _casycom_LastSignal = 0;
/// Loop exit code
static int _casycom_ExitCode = EXIT_SUCCESS;
/// Loop status
static bool _casycom_Quitting = false;

//}}}-------------------------------------------------------------------
//{{{ Debugging
#ifndef NDEBUG

void LogMessage (int type, const char* fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    if (isatty (STDOUT_FILENO))
	vprintf (fmt, args);
    else
	vsyslog (type, fmt, args);
    va_end (args);
}

void PrintBacktrace (void)
{
#if HAVE_EXECINFO_H
    void* frames [64];
    int nFrames = backtrace (frames, ArraySize(frames));
    if (nFrames <= 1)
	return;	// Can happen if there is no debugging information
    char** syms = backtrace_symbols (frames, nFrames);
    if (!syms)
	return;
    for (int i = 1; i < nFrames; ++i)
	LogMessage (LOG_ERR, "\t%s\n", syms[i]);
    free (syms);
#endif
}

#endif
//}}}-------------------------------------------------------------------
//{{{ Signal handling

#define S(s) (1<<(s))
enum {
    sigset_Quit	= S(SIGINT)|S(SIGQUIT)|S(SIGTERM)|S(SIGPWR),
    sigset_Die	= S(SIGILL)|S(SIGABRT)|S(SIGBUS)|S(SIGFPE)|S(SIGSYS)|S(SIGSEGV)|S(SIGALRM)|S(SIGXCPU),
    sigset_Msg	= sigset_Quit|S(SIGHUP)|S(SIGCHLD)|S(SIGWINCH)|S(SIGURG)|S(SIGXFSZ)|S(SIGUSR1)|S(SIGUSR2)|S(SIGPIPE)
};

static void OnFatalSignal (int sig)
{
    enum { qc_ShellSignalQuitOffset = 128 };
    static volatile _Atomic(bool) doubleSignal = false;
    if (false == atomic_exchange (&doubleSignal, true)) {
	LogMessage (LOG_CRIT, "[S] Error: %s\n", strsignal(sig));
	#ifndef NDEBUG
	    PrintBacktrace();
	#endif
	exit (qc_ShellSignalQuitOffset+sig);
    }
    _Exit (qc_ShellSignalQuitOffset+sig);
}

static void OnMsgSignal (int sig)
{
    _casycom_LastSignal = sig;
    alarm (1);	// Reset in main loop after clearing _casycom_LastSignal. Ensures that hangs can be broken.
}

static void InstallCleanupHandlers (void)
{
    for (unsigned sig = 0; sig < sizeof(int)*8; ++sig) {
	if (sigset_Msg & S(sig))
	    signal (sig, OnMsgSignal);
	else if (sigset_Die & S(sig))
	    signal (sig, OnFatalSignal);
    }
}
#undef S

//}}}-------------------------------------------------------------------
//--- Main API

/// Initializes the library and the event loop
void casycom_init (void)
{
}

/// Replaces casycom_init if casycom is the top-level framework in your process
void casycom_framework_init (void)
{
    InstallCleanupHandlers();
    casycom_init();
}

/// Sets the quit flag, causing the event loop to quit once all queued events are processed
void casycom_quit (int exitCode)
{
    _casycom_ExitCode = exitCode;
    _casycom_Quitting = true;
}

/// The main event loop. Returns exit code.
int casycom_main (void)
{
    while (!_casycom_Quitting) {
	casycom_quit (EXIT_SUCCESS);
    }
    return _casycom_ExitCode;
}
