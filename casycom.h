// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "casycom/config.h"
#include <syslog.h>

//----------------------------------------------------------------------

typedef unsigned short	oid_t;
typedef const char*	methodid_t;
typedef const char*	interfaceid_t;

typedef struct _SMsg {
    oid_t	_src;
    oid_t	_dest;
    unsigned	_size;
    methodid_t	_method;
    void*	_body;
} SMsg;

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//{{{ Utility functions ------------------------------------------------

#define ArraySize(a)	(sizeof(a)/sizeof(a[0]))

//}}}-------------------------------------------------------------------
// Main API

void	casycom_init (void);
void	casycom_framework_init (void);
int	casycom_main (void);
void	casycom_quit (int exitCode);

SMsg*	casycom_create_message (void);

//{{{ Debugging functions ----------------------------------------------

#ifndef NDEBUG
void	LogMessage (int type, const char* fmt, ...) __attribute__((__format__(__printf__,2,3)));
void	PrintBacktrace (void);
#else
#define LogMessage(type,fmt,...)	syslog(type,fmt,__VA_ARGS__)
#endif

//}}}-------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
