// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"

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

SMsg*	casycom_create_message (void) noexcept;

#ifdef __cplusplus
} // extern "C"
#endif
