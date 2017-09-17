// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"
#include <syslog.h>

//{{{ Memory management ------------------------------------------------

// Types for main args
typedef unsigned	argc_t;
#ifdef UC_VERSION
typedef const char**	argv_t;
#else
typedef char* const*	argv_t;
#endif

#ifdef __cplusplus
extern "C" {
    template <typename T, size_t N> constexpr inline size_t ArraySize (T(&a)[N]) { return N; }
#else
    #define ArraySize(a)	(sizeof(a)/sizeof(a[0]))
#endif
#define ArrayBlock(a)		a, ArraySize(a)

static inline constexpr size_t Floor (size_t n, size_t grain)	{ return n - n % grain; }
static inline constexpr size_t Align (size_t n, size_t grain)	{ return Floor (n+grain-1, grain); }
static inline constexpr size_t DivRU (size_t n1, size_t n2)	{ return (n1 + n2-1) / n2; }

#ifndef UC_VERSION
static inline const char* strnext (const char* s)		{ return s+strlen(s)+1; }
#endif

void*	xalloc (size_t sz) noexcept MALLOCLIKE;
void*	xrealloc (void* p, size_t sz) noexcept MALLOCLIKE;
#define xfree(p)	do { if (p) { free(p); p = NULL; } } while (false)

// Define explicit aliasing cast to work around strict aliasing rules
#define DEFINE_ALIAS_CAST(name,type)	\
    type* name (void* p) { union { void* p; type* t; } __attribute__((may_alias)) cs = { .p = p }; return cs.t; }

//}}}-------------------------------------------------------------------
//{{{ Debugging

#ifndef NDEBUG
void	casycom_log (int type, const char* fmt, ...) noexcept PRINTFARGS(2,3);
void	casycom_backtrace (void) noexcept;
#else
#define casycom_log(type,...)	syslog(type,__VA_ARGS__)
#endif

#ifndef UC_VERSION
void hexdump (const void* pv, size_t n) noexcept;
#endif

//}}}-------------------------------------------------------------------
//{{{ Miscellaneous

// Systemd socket activation support
enum { SD_LISTEN_FDS_START = 3 };
unsigned sd_listen_fds (void) noexcept;

const char* find_exe_in_path (const char* exe, char* exepath, size_t exepathlen) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

// Use in lock wait loops to relax the CPU load
static inline void tight_loop_pause (void)
{
    #if __i386__ || __x86_64__
	#if __clang__
	    __asm__ volatile ("rep nop");
	#else
	    __builtin_ia32_pause();
	#endif
    #else
	usleep (1);
    #endif
}

static inline void acquire_lock (_Atomic(bool)* l)
    { do { tight_loop_pause(); } while (atomic_exchange (l, true)); }
static inline void release_lock (_Atomic(bool)* l)
    { *l = false; }

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
//}}}-------------------------------------------------------------------
