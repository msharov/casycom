// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "util.h"
#include <stdarg.h>
#if HAVE_EXECINFO_H
    #include <execinfo.h>
#endif

//{{{ Memory management ------------------------------------------------

void* xalloc (size_t sz)
{
    void* p = xrealloc (NULL, sz);
    memset (p, 0, sz);
    return p;
}

void* xrealloc (void* p, size_t sz)
{
    p = realloc (p, sz);
    if (!p) {
	casycom_log (LOG_ERR, "out of memory");
	exit (EXIT_FAILURE);
    }
    return p;
}

//}}}-------------------------------------------------------------------
//{{{ Debugging

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
    int nFrames = backtrace (ArrayBlock(frames));
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

#ifndef UC_VERSION
static inline char _num_to_digit (uint8_t b)
{
    char d = (b & 0xF) + '0';
    return d <= '9' ? d : d+('A'-'0'-10);
}
static inline bool _printable (char c)
{
    return c >= 32 && c < 127;
}
void hexdump (const void* vp, size_t n)
{
    const uint8_t* p = vp;
    char line[65]; line[64] = 0;
    for (size_t i = 0; i < n; i += 16) {
	memset (line, ' ', sizeof(line)-1);
	for (size_t h = 0; h < 16; ++h) {
	    if (i+h < n) {
		uint8_t b = p[i+h];
		line[h*3] = _num_to_digit(b>>4);
		line[h*3+1] = _num_to_digit(b);
		line[h+3*16] = _printable(b) ? b : '.';
	    }
	}
	puts (line);
    }
}
#endif

//}}}-------------------------------------------------------------------
//{{{ Miscellaneous

unsigned sd_listen_fds (void)
{
    const char* e = getenv("LISTEN_PID");
    if (!e || getpid() != (pid_t) strtoul(e, NULL, 10))
	return 0;
    e = getenv("LISTEN_FDS");
    return e ? strtoul (e, NULL, 10) : 0;
}

#ifndef UC_VERSION
const char* executable_in_path (const char* efn, char* exe, size_t exesz)
{
    if (efn[0] == '/' || (efn[0] == '.' && (efn[1] == '/' || efn[1] == '.'))) {
	if (0 != access (efn, X_OK))
	    return NULL;
	return efn;
    }

    const char* penv = getenv("PATH");
    if (!penv)
	penv = "/bin:/usr/bin:.";
    char path [PATH_MAX];
    snprintf (ArrayBlock(path), "%s/%s"+3, penv);

    for (char *pf = path, *pl = pf; *pf; pf = pl) {
	while (*pl && *pl != ':') ++pl;
	*pl++ = 0;
	snprintf (exe, exesz, "%s/%s", pf, efn);
	if (0 == access (exe, X_OK))
	    return exe;
    }
    return NULL;
}
#endif

//}}}-------------------------------------------------------------------
