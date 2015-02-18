// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "msg.h"

typedef struct _SObject {
    void*		(*Create)(const SMsg* msg);
    void		(*Destroy)(void* o);
    void		(*ObjectDestroyed)(void* o, oid_t oid);
    bool		(*Error)(void* o, oid_t eoid, const char* msg);
    const void* const	interface[];
} SObject;

#ifdef __cplusplus
extern "C" {
#endif

void	casycom_init (void) noexcept;
void	casycom_reset (void) noexcept;
void	casycom_framework_init (void) noexcept;
int	casycom_main (void) noexcept;
void	casycom_quit (int exitCode) noexcept;

typedef void* (pfn_object_init)(const SMsg* msg);

void	casycom_register (const SObject* o) noexcept NONNULL();
PProxy	casycom_create_proxy (iid_t iid, oid_t src) noexcept;
void	casycom_destroy_proxy (PProxy* pp) noexcept NONNULL();
void	casycom_error (const char* fmt, ...) noexcept PRINTFARGS(1,2);
bool	casycom_forward_error (oid_t oid, oid_t eoid) noexcept;
void	casycom_mark_unused (const void* o) noexcept NONNULL();
oid_t	casycom_oid_of_object (const void* o) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline PProxy casycom_create_proxy_to (iid_t iid, oid_t src, oid_t dest)
    { return (PProxy) { iid, src, dest }; }
static inline PProxy casycom_create_reply_proxy (iid_t iid, const SMsg* msg)
    { return casycom_create_proxy_to (iid, msg->h.dest, msg->h.src); }

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
