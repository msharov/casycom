// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "config.h"
#include "casycom.h"
#include <signal.h>

//{{{ Module globals ---------------------------------------------------

/// Last non-fatal signal. Set by signal handler, read by main loop.
static int _casycom_LastSignal = 0;
/// Loop exit code
static int _casycom_ExitCode = EXIT_SUCCESS;
/// Loop status
static bool _casycom_Quitting = false;

//}}}-------------------------------------------------------------------
//{{{ Signal handling

#define S(s) (1<<(s))
enum {
    sigset_Quit	= S(SIGINT)|S(SIGQUIT)|S(SIGTERM)|S(SIGPWR),
    sigset_Die	= S(SIGILL)|S(SIGABRT)|S(SIGBUS)|S(SIGFPE)|S(SIGSYS)|S(SIGSEGV)|S(SIGALRM)|S(SIGXCPU),
    sigset_Msg	= sigset_Quit|S(SIGHUP)|S(SIGCHLD)|S(SIGWINCH)|S(SIGURG)|S(SIGXFSZ)|S(SIGUSR1)|S(SIGUSR2)|S(SIGPIPE)
};

static void casycom_on_fatal_signal (int sig)
{
    enum { qc_ShellSignalQuitOffset = 128 };
    static volatile _Atomic(bool) doubleSignal = false;
    if (false == atomic_exchange (&doubleSignal, true)) {
	casycom_log (LOG_CRIT, "[S] Error: %s\n", strsignal(sig));
	#ifndef NDEBUG
	    casycom_backtrace();
	#endif
	exit (qc_ShellSignalQuitOffset+sig);
    }
    _Exit (qc_ShellSignalQuitOffset+sig);
}

static void casycom_on_msg_signal (int sig)
{
    _casycom_LastSignal = sig;
    alarm (1);	// Reset in main loop after clearing _casycom_LastSignal. Ensures that hangs can be broken.
}

static void casycom_install_signal_handlers (void)
{
    for (unsigned sig = 0; sig < sizeof(int)*8; ++sig) {
	if (sigset_Msg & S(sig))
	    signal (sig, casycom_on_msg_signal);
	else if (sigset_Die & S(sig))
	    signal (sig, casycom_on_fatal_signal);
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
    casycom_install_signal_handlers();
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
