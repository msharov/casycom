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
//{{{ vector

typedef int (*vector_compare_fn_t)(const void*, const void*);

typedef struct _CharVector {
    char*	d;
    size_t	size;
    size_t	allocated;
    size_t	elsize;
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

#define VECTOR_INIT(vtype)	{ .elsize = sizeof(*(((vtype*)NULL)->d)) }
#define VECTOR(vtype,name)	vtype name = VECTOR_INIT(vtype)
#define VECTOR_MEMBER_INIT(vtype,name)	vector_init(&(name), sizeof(*(((vtype*)NULL)->d)))

#define vector_begin(v)			(v)->d
#define vector_end(v)			((v)->d+(v)->size)
#define vector_foreach(vtype,p,v)	for (vtype *p = vector_begin(&(v)), *p##end = vector_end(&(v)); p < p##end; ++p)

void	vector_reserve (void* v, size_t sz) noexcept NONNULL();
void	vector_deallocate (void* v) noexcept NONNULL();
void	vector_insert (void* vv, size_t ip, const void* e) noexcept NONNULL();
void	vector_insert_n (void* v, size_t ip, const void* e, size_t esz) noexcept NONNULL();
void*	vector_emplace (void* vv, size_t ip) noexcept NONNULL();
void*	vector_emplace_n (void* vv, size_t ip, size_t n) noexcept NONNULL();
void	vector_erase_n (void* v, size_t ep, size_t n) noexcept NONNULL();
void	vector_swap (void* v1, void* v2) noexcept NONNULL();
size_t	vector_lower_bound (const void* vv, vector_compare_fn_t cmp, const void* e) noexcept NONNULL();
size_t	vector_upper_bound (const void* vv, vector_compare_fn_t cmp, const void* e) noexcept NONNULL();
void	vector_copy (void* vv1, const void* vv2) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline void vector_init (void* vv, size_t elsz) {
    CharVector* v = vv;
    v->d = NULL;
    v->size = 0;
    v->allocated = 0;
    v->elsize = elsz;
}
static inline void vector_erase (void* v, size_t ep)
    { vector_erase_n (v, ep, 1); }
static inline void vector_push_back (void* vv, const void* e)
    { CharVector* v = vv; vector_insert (vv, v->size, e); }
static inline void vector_append_n (void* vv, const void* e, size_t n)
    { CharVector* v = vv; vector_insert_n (vv, v->size, e, n); }
static inline void* vector_emplace_back (void* vv)
    { CharVector* v = vv; return vector_emplace (vv, v->size); }
static inline void vector_pop_back (void* vv)
    { CharVector* v = vv; vector_erase (vv, v->size-1); }
static inline void vector_clear (void* vv)
    { CharVector* v = vv; v->size = 0; }
static inline void vector_detach (void* vv)
    { CharVector* v = vv; v->d = NULL; v->size = v->allocated = 0; }
static inline void vector_attach (void* vv, void* e, size_t n) {
    CharVector* v = vv;
    assert (!v->d && "This vector is already attached to something. Detach or deallocate first.");
    assert (e && "Attaching requires a non-null pointer");
    v->d = e; v->size = v->allocated = n;
}
static inline NONNULL() void vector_resize (void* vv, size_t sz) {
    CharVector* v = vv;
    vector_reserve (v, sz);
    v->size = sz;
}
static inline size_t NONNULL() vector_insert_sorted (void* vv, vector_compare_fn_t cmp, const void* e) {
    size_t ip = vector_upper_bound (vv, cmp, e);
    vector_insert (vv, ip, e);
    return ip;
}
static inline void NONNULL() vector_sort (void* vv, vector_compare_fn_t cmp)
    { CharVector* v = vv; qsort (v->d, v->size, v->elsize, cmp); }

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
