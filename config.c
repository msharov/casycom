#include "config.h"
#include <stdarg.h>
#if HAVE_EXECINFO_H
    #include <execinfo.h>
#endif

#ifndef NDEBUG

void casycom_log (int type, const char* fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    if (isatty (STDOUT_FILENO))
	vprintf (fmt, args);
    else
	vsyslog (type, fmt, args);
    va_end (args);
}

void casycom_backtrace (void)
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
	casycom_log (LOG_ERR, "\t%s\n", syms[i]);
    free (syms);
#endif
}

#endif
