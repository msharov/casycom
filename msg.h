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

typedef struct _SInterface {
    const void*	dispatch;
    methodid_t	name;
    methodid_t	method[];
} SInterface;

typedef const SInterface*	iid_t;

typedef struct _PProxy {
    iid_t	interface;
    oid_t	src;
    oid_t	dest;
} PProxy;

typedef struct _SMsg {
    PProxy	h;
    uint32_t	imethod;
    uint32_t	size;
    oid_t	extid;
    uint8_t	fdoffset;
    uint8_t	reserved;
    void*	body;
} SMsg;

enum {
    NO_FD_IN_MESSAGE = UINT8_MAX,
    MESSAGE_BODY_ALIGNMENT = 8,
    method_CreateObject = UINT32_MAX
};

typedef struct _DTable {
    const iid_t	interface;
} DTable;

#define DMETHOD(o,m)	.m = (MFN_##m) o##_##m

typedef void (*pfn_dispatch)(const void* dtable, void* o, const SMsg* msg);

//----------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

SMsg*	casymsg_begin (const PProxy* pp, uint32_t imethod, uint32_t sz) noexcept NONNULL() MALLOCLIKE;
void	casymsg_from_vector (const PProxy* pp, uint32_t imethod, void* body) noexcept NONNULL();
void	casymsg_forward (const PProxy* pp, SMsg* msg) noexcept NONNULL();
void	casycom_queue_message (SMsg* msg) noexcept NONNULL(); ///< In main.c
uint32_t casyiface_count_methods (iid_t iid) noexcept;
size_t	casymsg_validate_signature (const char* sig, RStm* buf) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline const char* casymsg_method_name (const SMsg* msg) {
    assert (casyiface_count_methods(msg->h.interface) > msg->imethod && "invalid method index in message");
    return msg->h.interface->method[msg->imethod];
}
static inline const char* casymsg_signature (const SMsg* msg) {
    const char* mname = casymsg_method_name (msg);
    assert (mname && "invalid method in message");
    return mname + strlen(mname) + 1;
}

static inline RStm casymsg_read (const SMsg* msg)
    { return (RStm) { (const char*) msg->body, (const char*) msg->body + msg->size }; }
static inline WStm casymsg_write (SMsg* msg)
    { return (WStm) { (char*) msg->body, (char*) msg->body + msg->size }; }
static inline void casymsg_end (SMsg* msg)
    { casycom_queue_message (msg); }
static inline void casymsg_write_fd (SMsg* msg, WStm* os, int fd) {
    size_t fdoffset = os->_p - (char*) msg->body;
    assert (fdoffset < UINT8_MAX && "file descriptors must be passed in the first 252 bytes of the message");
    msg->fdoffset = fdoffset;
    casystm_write_int32 (os, fd);
}

#define casymsg_free(msg)	\
    do { if (msg) xfree (msg->body); xfree (msg); } while (false)

static inline void casymsg_default_dispatch (const void* dtable UNUSED, void* o UNUSED, const SMsg* msg)
{
    if (msg->imethod != method_CreateObject)
	assert (!"Invalid method index in message");
}

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
