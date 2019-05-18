// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "msg.h"

typedef struct _Factory {
    void*		(*create)(const Msg* msg);
    void		(*destroy)(void* o);
    void		(*object_destroyed)(void* o, oid_t oid);
    bool		(*error)(void* o, oid_t eoid, const char* msg);
    const void* const	dtable[];
} Factory;

#ifdef __cplusplus
extern "C" {
#endif

void	casycom_init (void) noexcept;
void	casycom_reset (void) noexcept;
void	casycom_framework_init (const Factory* oapp, argc_t argc, argv_t argv) noexcept NONNULL(1);
int	casycom_main (void) noexcept;
void	casycom_quit (int exitCode) noexcept;
bool	casycom_is_quitting (void) noexcept;
bool	casycom_is_failed (void) noexcept;
int	casycom_exit_code (void) noexcept;
bool	casycom_loop_once (void) noexcept;

typedef void* (pfn_object_init)(const Msg* msg);

void	casycom_register (const Factory* o) noexcept NONNULL();
void	casycom_register_default (const Factory* o) noexcept;
void*	casycom_create_object (const iid_t iid) noexcept NONNULL();
iid_t	casycom_interface_by_name (const char* iname) noexcept NONNULL();
Proxy	casycom_create_proxy (iid_t iid, oid_t src) noexcept;
Proxy	casycom_create_proxy_to (iid_t iid, oid_t src, oid_t dest) noexcept;
void	casycom_destroy_proxy (Proxy* pp) noexcept NONNULL();
void	casycom_error (const char* fmt, ...) noexcept PRINTFARGS(1,2);
bool	casycom_forward_error (oid_t oid, oid_t eoid) noexcept;
void	casycom_mark_unused (const void* o) noexcept NONNULL();
oid_t	casycom_oid_of_object (const void* o) noexcept NONNULL();

#ifndef NDEBUG
    extern bool casycom_debug_msg_trace;
    #define DEBUG_MSG_TRACE     casycom_debug_msg_trace
    #define DEBUG_PRINTF(...)   do { if (DEBUG_MSG_TRACE) { fflush (stderr); printf (__VA_ARGS__); fflush (stdout); } } while (false)
#else
    #define DEBUG_MSG_TRACE     false
    #define DEBUG_PRINTF(...)   do {} while (false)
#endif
void casycom_debug_message_dump (const Msg* msg) noexcept;
void casycom_debug_dump_link_table (void) noexcept;

#ifdef __cplusplus
namespace {
#endif

static inline Proxy casycom_create_reply_proxy (iid_t iid, const Msg* msg)
    { return casycom_create_proxy_to (iid, msg->h.dest, msg->h.src); }

#ifndef NDEBUG
static inline void casycom_enable_debug_output (void)
    { casycom_debug_msg_trace = true; }
static inline void casycom_disable_debug_output (void)
    { casycom_debug_msg_trace = false; }
#else
static inline void casycom_enable_debug_output (void) {}
static inline void casycom_disable_debug_output (void) {}
#endif

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
