// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "msg.h"

typedef struct _SFactory {
    void*		(*Create)(const SMsg* msg);
    void		(*Destroy)(void* o);
    void		(*ObjectDestroyed)(void* o, oid_t oid);
    bool		(*Error)(void* o, oid_t eoid, const char* msg);
    const void* const	dtable[];
} SFactory;

#ifdef __cplusplus
extern "C" {
#endif

void	casycom_init (void) noexcept;
void	casycom_reset (void) noexcept;
void	casycom_framework_init (const SFactory* oapp, unsigned argc, const char* const* argv) noexcept NONNULL(1);
int	casycom_main (void) noexcept;
void	casycom_quit (int exitCode) noexcept;

typedef void* (pfn_object_init)(const SMsg* msg);

void	casycom_register (const SFactory* o) noexcept NONNULL();
void	casycom_register_default (const SFactory* o) noexcept;
PProxy	casycom_create_proxy (iid_t iid, oid_t src) noexcept;
PProxy	casycom_create_proxy_to (iid_t iid, oid_t src, oid_t dest) noexcept;
void	casycom_destroy_proxy (PProxy* pp) noexcept NONNULL();
void	casycom_error (const char* fmt, ...) noexcept PRINTFARGS(1,2);
bool	casycom_forward_error (oid_t oid, oid_t eoid) noexcept;
void	casycom_mark_unused (const void* o) noexcept NONNULL();
oid_t	casycom_oid_of_object (const void* o) noexcept NONNULL();

#ifndef NDEBUG
    extern bool casycom_DebugMsgTrace;
    #define DEBUG_MSG_TRACE     casycom_DebugMsgTrace
    #define DEBUG_PRINTF(...)   do { if (DEBUG_MSG_TRACE) { fflush (stderr); printf (__VA_ARGS__); fflush (stdout); } } while (false)
#else
    #define DEBUG_MSG_TRACE     false
    #define DEBUG_PRINTF(...)   do {} while (false)
#endif
void casycom_debug_message_dump (const SMsg* msg) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline PProxy casycom_create_reply_proxy (iid_t iid, const SMsg* msg)
    { return casycom_create_proxy_to (iid, msg->h.dest, msg->h.src); }

#ifndef NDEBUG
static inline void casycom_enable_debug_output (void)
    { DEBUG_MSG_TRACE = true; }
static inline void casycom_disable_debug_output (void)
    { DEBUG_MSG_TRACE = false; }
#else
static inline void casycom_enable_debug_output (void) {}
static inline void casycom_disable_debug_output (void) {}
#endif

#define CASYCOM_DEFINE_SINGLETON(name)			\
static void* name##_Create (const SMsg* msg UNUSED)	\
    { static S##name o; memset (&o,0,sizeof(o)); return &o; }\
static void name##_Destroy (void* p UNUSED) {}

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
