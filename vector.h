// This file is part of the casycom project
//
// Copyright (c) 2017 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "config.h"

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
#define vector_p2i(v,p)			((p)-vector_begin(v))

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

static inline NONNULL() void vector_init (void* vv, size_t elsz) {
    CharVector* v = vv;
    v->d = NULL;
    v->size = 0;
    v->allocated = 0;
    v->elsize = elsz;
}
static inline NONNULL() void vector_erase (void* v, size_t ep)
    { vector_erase_n (v, ep, 1); }
static inline NONNULL() void vector_push_back (void* vv, const void* e)
    { CharVector* v = vv; vector_insert (vv, v->size, e); }
static inline NONNULL() void vector_append_n (void* vv, const void* e, size_t n)
    { CharVector* v = vv; vector_insert_n (vv, v->size, e, n); }
static inline NONNULL() void* vector_emplace_back (void* vv)
    { CharVector* v = vv; return vector_emplace (vv, v->size); }
static inline NONNULL() void vector_pop_back (void* vv)
    { CharVector* v = vv; vector_erase (vv, v->size-1); }
static inline NONNULL() void vector_clear (void* vv)
    { CharVector* v = vv; v->size = 0; }
static inline NONNULL() void vector_link (void* vv, const void* e, size_t n) {
    CharVector* v = vv;
    assert (!v->d && "This vector is already linked to something. Unlink or deallocate first.");
    v->d = (void*) e; v->size = n;
}
static inline NONNULL() void vector_unlink (void* vv)
    { CharVector* v = vv; v->d = NULL; v->size = v->allocated = 0; }
static inline NONNULL() void vector_detach (void* vv)
    { vector_unlink (vv); }
static inline NONNULL() void vector_attach (void* vv, void* e, size_t n) {
    vector_link (vv, e, n);
    CharVector* v = vv;
    v->allocated = v->size;
}
static inline NONNULL() void vector_resize (void* vv, size_t sz) {
    CharVector* v = vv;
    vector_reserve (v, sz);
    v->size = sz;
}
static inline NONNULL() size_t vector_insert_sorted (void* vv, vector_compare_fn_t cmp, const void* e) {
    size_t ip = vector_upper_bound (vv, cmp, e);
    vector_insert (vv, ip, e);
    return ip;
}
static inline NONNULL() void vector_sort (void* vv, vector_compare_fn_t cmp)
    { CharVector* v = vv; qsort (v->d, v->size, v->elsize, cmp); }

#ifdef __cplusplus
} // namespace
#endif
