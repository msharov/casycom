// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"

//{{{ Memory management ------------------------------------------------

#ifdef __cplusplus
extern "C" {
    template <typename T, size_t N> constexpr inline size_t ArraySize (T(&a)[N]) { return N; }
#else
    #define ArraySize(a)	(sizeof(a)/sizeof(a[0]))
#endif

static inline constexpr size_t Floor (size_t n, size_t grain)	{ return n - n % grain; }
static inline constexpr size_t Align (size_t n, size_t grain)	{ return Floor (n+grain-1, grain); }
static inline constexpr size_t DivRU (size_t n1, size_t n2)	{ return (n1 + n2-1) / n2; }

void*	xalloc (size_t sz) noexcept MALLOCLIKE;
void*	xrealloc (void* p, size_t sz) noexcept MALLOCLIKE;
#define xfree(p)	do { if (p) { free(p); p = NULL; } } while (false)

//}}}-------------------------------------------------------------------
//{{{ Debugging

#ifndef NDEBUG
void	casycom_log (int type, const char* fmt, ...) PRINTFARGS(2,3);
void	casycom_backtrace (void);
#else
#define casycom_log(type,...)	syslog(type,__VA_ARGS__)
#endif

//}}}-------------------------------------------------------------------
//{{{ vector

typedef struct _vector {
    char*		d;
    size_t		size;
    size_t		allocated;
    const size_t	elsize;
} vector;

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

#define VECTOR(vtype,name)	vtype name = { NULL, 0, 0, sizeof(*(((vtype*)NULL)->d)) }

void	vector_reserve (void* v, size_t sz) noexcept;
void	vector_deallocate (void* v) noexcept;
void	vector_insert (void* v, size_t ip, const void* e) noexcept NONNULL();
void*	vector_insert_empty (void* v, size_t ip) noexcept;
void	vector_erase_n (void* v, size_t ep, size_t n) noexcept;
void	vector_swap (void* v1, void* v2) noexcept NONNULL();

#ifdef __cplusplus
namespace {
#endif

static inline void vector_erase (void* v, size_t ep)
    { vector_erase_n (v, ep, 1); }
static inline void vector_push_back (void* v, const void* e)
    { vector_insert (v, ((vector*)v)->size, e); }
static inline void* vector_push_back_empty (void* v)
    { return vector_insert_empty (v, ((vector*)v)->size); }
static inline void vector_pop_back (void* v)
    { vector_erase (v, ((vector*)v)->size-1); }
static inline void vector_clear (void* v)
    { vector_erase_n (v, 0, ((vector*)v)->size); }
static inline void vector_detach (void* vv)
    { vector* v = (vector*) vv; v->d = NULL; v->size = v->allocated = 0; }
static inline NONNULL() void vector_attach (void* vv, void* e, size_t n) {
    vector* v = (vector*) vv;
    assert (!v->d && "This vector is already attached to something. Detach or deallocate first.");
    assert (e && "Attaching requires a non-null pointer");
    v->d = e; v->size = v->allocated = n;
}

#ifdef __cplusplus
} // namespace
#endif

//}}}-------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
