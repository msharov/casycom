// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"
#include <syslog.h>

//{{{ Memory management ------------------------------------------------

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

static inline const char* strnext (const char* s)		{ return s+strlen(s)+1; }

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

void hexdump (const void* pv, size_t n) noexcept;

//}}}-------------------------------------------------------------------
//{{{ vector

typedef struct _CharVector {
    char*		d;
    size_t		size;
    size_t		allocated;
    const size_t	elsize;
} CharVector;

// Declares a vector for the given type.
// Use the VECTOR macro to instantiate it. Fields are marked const to
// ensure only the vector_ functions modify it.
#define DECLARE_VECTOR_TYPE(name,type)	\
typedef struct _##name {		\
    type* const		d;		\
    const size_t	size;		\
    const size_t	allocated;	\
    const size_t	elsize;		\
} name

#define VECTOR_INIT(vtype)	{ NULL, 0, 0, sizeof(*(((vtype*)NULL)->d)) }
#define VECTOR(vtype,name)	vtype name = VECTOR_INIT(vtype)
#define VECTOR_MEMBER_INIT(vtype,name)	*(size_t*)&(name).elsize = sizeof(*(((vtype*)NULL)->d));

void	vector_reserve (void* v, size_t sz) noexcept;
void	vector_deallocate (void* v) noexcept;
void	vector_insert (void* v, size_t ip, const void* e) noexcept NONNULL();
void*	vector_emplace (void* v, size_t ip) noexcept;
void	vector_erase_n (void* v, size_t ep, size_t n) noexcept;
void	vector_swap (void* v1, void* v2) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline void vector_erase (void* v, size_t ep)
    { vector_erase_n (v, ep, 1); }
static inline void vector_push_back (void* v, const void* e)
    { vector_insert (v, ((CharVector*)v)->size, e); }
static inline void* vector_emplace_back (void* v)
    { return vector_emplace (v, ((CharVector*)v)->size); }
static inline void vector_pop_back (void* v)
    { vector_erase (v, ((CharVector*)v)->size-1); }
static inline void vector_clear (void* vv)
    { CharVector* v = (CharVector*) vv; v->size = 0; }
static inline void vector_detach (void* vv)
    { CharVector* v = (CharVector*) vv; v->d = NULL; v->size = v->allocated = 0; }
static inline NONNULL() void vector_attach (void* vv, void* e, size_t n) {
    CharVector* v = (CharVector*) vv;
    assert (!v->d && "This vector is already attached to something. Detach or deallocate first.");
    assert (e && "Attaching requires a non-null pointer");
    v->d = e; v->size = v->allocated = n;
}

#ifdef __cplusplus
} // namespace
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
