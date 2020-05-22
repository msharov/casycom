// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#include "vector.h"
#include "util.h"

void vector_reserve (void* vv, size_t sz)
{
    CharVector* v = vv;
    if (v->allocated >= sz)
	return;
    size_t nsz = v->allocated + !v->allocated;
    while (nsz < sz)
	nsz *= 2;
    assert (v->elsize && "uninitialized vector detected");
    v->d = xrealloc (v->d, nsz * v->elsize);
    memset (v->d + v->allocated * v->elsize, 0, (nsz - v->allocated) * v->elsize);
    v->allocated = nsz;
}

void vector_deallocate (void* vv)
{
    CharVector* v = vv;
    xfree (v->d);
    v->size = 0;
    v->allocated = 0;
}

void* vector_emplace (void* vv, size_t ip)
{
    CharVector* v = vv;
    assert (ip <= v->size && "out of bounds insert");
    vector_reserve (vv, v->size+1);
    char* ii = v->d + ip * v->elsize;
    memmove (ii + v->elsize, ii, (v->size-ip)*v->elsize);
    memset (ii, 0, v->elsize);
    ++v->size;
    return ii;
}

void* vector_emplace_n (void* vv, size_t ip, size_t n)
{
    CharVector* v = vv;
    assert (ip <= v->size && "out of bounds insert");
    vector_reserve (vv, v->size+n);
    char* ii = v->d + ip * v->elsize;
    memmove (ii + n*v->elsize, ii, (v->size-ip)*v->elsize);
    memset (ii, 0, n*v->elsize);
    v->size += n;
    return ii;
}

void vector_insert (void* vv, size_t ip, const void* e)
{
    CharVector* v = vv;
    void* h = vector_emplace (v, ip);
    memcpy (h, e, v->elsize);
}

void vector_insert_n (void* vv, size_t ip, const void* e, size_t esz)
{
    CharVector* v = vv;
    void* h = vector_emplace_n (v, ip, esz);
    memcpy (h, e, esz*v->elsize);
}

void vector_erase_n (void* vv, size_t ep, size_t n)
{
    CharVector* v = vv;
    char *ei = v->d + ep*v->elsize, *eei = v->d + (ep+n)*v->elsize;
    assert (ep <= v->size && ep+n <= v->size && "out of bounds erase");
    memmove (ei, eei, (v->size - (ep+n)) * v->elsize);
    v->size -= n;
}

void vector_swap (void* vv1, void* vv2)
{
    CharVector *v1 = vv1, *v2 = vv2;
    assert (v1->elsize == v2->elsize && "can only swap identical vectors");
    CharVector t;
    memcpy (&t, v1, sizeof(t));
    memcpy (v1, v2, sizeof(*v1));
    memcpy (v2, &t, sizeof(*v2));
}

void vector_copy (void* vv1, const void* vv2)
{
    CharVector* v1 = vv1;
    const CharVector* v2 = vv2;
    vector_resize (v1, v2->size);
    memcpy (v1->d, v2->d, v2->size);
}

static size_t _vector_bound (const void* vv, vector_compare_fn_t cmp, const int cmpv, const void* e)
{
    const CharVector* v = vv;
    size_t f = 0, l = v->size;
    while (f < l) {
	size_t m = (f+l)/2;
	if (cmpv > cmp (v->d + m*v->elsize, e))
	    f = m + 1;
	else
	    l = m;
    }
    return f;
}
size_t vector_lower_bound (const void* vv, vector_compare_fn_t cmp, const void* e)
    { return _vector_bound (vv, cmp, 0, e); }
size_t vector_upper_bound (const void* vv, vector_compare_fn_t cmp, const void* e)
    { return _vector_bound (vv, cmp, 1, e); }
