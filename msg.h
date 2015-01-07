// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "stm.h"

//----------------------------------------------------------------------

typedef uint16_t	oid_t;
typedef const char*	methodid_t;

typedef struct _SInterface {
    const char*	name;
    size_t	nMethods;
    const char*	methods[];
} SInterface;
typedef const SInterface*	iid_t;

typedef struct _SMsg {
    iid_t	interface;
    void*	body;
    uint32_t	imethod;
    uint32_t	size;
    oid_t	src;
    oid_t	dest;
    oid_t	extid;
    uint8_t	fdoffset;
    uint8_t	reserved;
} SMsg;

enum { NoFdInMessage = UINT8_MAX };

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

SMsg*	casymsg_create (uint32_t sz) noexcept;

#ifdef __cplusplus
} // extern "C"
#endif
