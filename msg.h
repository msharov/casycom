// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "stm.h"

//----------------------------------------------------------------------

typedef uint16_t	oid_t;

enum { oid_Broadcast, oid_App, oid_First };

typedef const char*	methodid_t;

typedef struct _Interface {
    const void*	dispatch;
    methodid_t	name;
    methodid_t	method[];
} Interface;

typedef const Interface*	iid_t;

typedef struct _Proxy {
    iid_t	interface;
    oid_t	src;
    oid_t	dest;
} Proxy;

#define PROXY_INIT	{NULL,0,0}

typedef struct _Msg {
    Proxy	h;
    uint32_t	imethod;
    uint32_t	size;
    oid_t	extid;
    uint8_t	fdoffset;
    uint8_t	reserved;
    void*	body;
} Msg;

enum {
    NO_FD_IN_MESSAGE = UINT8_MAX,
    MESSAGE_HEADER_ALIGNMENT = 8,
    MESSAGE_BODY_ALIGNMENT = MESSAGE_HEADER_ALIGNMENT,
    method_Invalid = ((uint32_t)-2),
    method_CreateObject = ((uint32_t)-1)
};

typedef struct _DTable {
    const iid_t	interface;
} DTable;

#define DMETHOD(o,m)	.m = (MFN_##m) o##_##m

typedef void (*pfn_dispatch)(const void* dtable, void* o, const Msg* msg);

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

Msg*	casymsg_begin (const Proxy* pp, uint32_t imethod, uint32_t sz) noexcept NONNULL() MALLOCLIKE;
void	casymsg_from_vector (const Proxy* pp, uint32_t imethod, void* body) noexcept NONNULL();
void	casymsg_forward (const Proxy* pp, Msg* msg) noexcept NONNULL();
void	casycom_queue_message (Msg* msg) noexcept NONNULL(); ///< In main.c
uint32_t casyiface_count_methods (iid_t iid) noexcept;
size_t	casymsg_validate_signature (const Msg* msg) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline const char* casymsg_interface_name (const Msg* msg) { return msg->h.interface->name; }
static inline const char* casymsg_method_name (const Msg* msg) {
    assert ((msg->imethod == method_CreateObject || casyiface_count_methods(msg->h.interface) > msg->imethod) && "invalid method index in message");
    return msg->h.interface->method[(int32_t)msg->imethod];
}
static inline const char* casymsg_signature (const Msg* msg) {
    const char* mname = casymsg_method_name (msg);
    assert (mname && "invalid method in message");
    return mname + strlen(mname) + 1;
}

static inline RStm casymsg_read (const Msg* msg)
    { return (RStm) { (const char*) msg->body, (const char*) msg->body + msg->size }; }
static inline WStm casymsg_write (Msg* msg)
    { return (WStm) { (char*) msg->body, (char*) msg->body + msg->size }; }
static inline void casymsg_end (Msg* msg)
    { casycom_queue_message (msg); }
static inline void casymsg_write_fd (Msg* msg, WStm* os, int fd) {
    size_t fdoffset = os->_p - (char*) msg->body;
    assert (fdoffset != NO_FD_IN_MESSAGE && "file descriptors must be passed in the first 252 bytes of the message");
    assert (msg->fdoffset == NO_FD_IN_MESSAGE && "only one file descriptor can be passed per message");
    msg->fdoffset = fdoffset;
    casystm_write_int32 (os, fd);
}
static inline int casymsg_read_fd (const Msg* msg, RStm* is) {
    assert ((size_t)(is->_p - (char*) msg->body) == msg->fdoffset && "there is no file descriptor at this offset");
    return casystm_read_int32 (is);
}

#define casymsg_free(msg)	\
    do { if (msg) xfree (msg->body); xfree (msg); } while (false)

static inline void casymsg_default_dispatch (const void* dtable UNUSED, void* o UNUSED, const Msg* msg)
{
    if (msg->imethod != method_CreateObject)
	assert (!"Invalid method index in message");
}

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
