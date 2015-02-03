// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "util.h"

typedef struct _RStm {
    const char*	_p;
    const char*	_end;
} RStm;

typedef struct _WStm {
    char*	_p;
    char*	_end;
} WStm;

#ifdef __cplusplus
extern "C" {
namespace {
#endif

static inline size_t casystm_read_available (const RStm* s)
    { return s->_end - s->_p; }
static inline bool casystm_can_read (const RStm* s, size_t sz)
    { return s->_p + sz <= s->_end; }
static inline void casystm_read_skip (RStm* s, size_t sz)
    { assert (casystm_can_read(s,sz)); s->_p += sz; }
static inline void casystm_unread (RStm* s, size_t sz)
    { s->_p -= sz; }
static inline void casystm_read_skip_to_end (RStm* s)
    { s->_p = s->_end; }
static inline void casystm_read_data (RStm* s, void* buf, size_t sz)
    { const void* p = s->_p; casystm_read_skip (s, sz); memcpy (buf, p, sz); }
static inline bool casystm_is_read_aligned (const RStm* s, size_t grain)
    { return !(((uintptr_t)s->_p) % grain); }
static inline void casystm_read_align (RStm* s, size_t grain)
    { s->_p = (const char*) Align ((uintptr_t) s->_p, grain); }

static inline size_t casystm_write_available (const RStm* s)
    { return s->_end - s->_p; }
static inline bool casystm_can_write (const WStm* s, size_t sz)
    { return s->_p + sz <= s->_end; }
static inline void casystm_write_skip (WStm* s, size_t sz)
    { assert (casystm_can_write(s,sz)); s->_p += sz; }
static inline void casystm_write_skip_to_end (WStm* s)
    { s->_p = s->_end; }
static inline void casystm_write_data (WStm* s, const void* buf, size_t sz)
    { void* p = s->_p; casystm_write_skip (s, sz); memcpy (p, buf, sz); }
static inline bool casystm_is_write_aligned (const WStm* s, size_t grain)
    { return !(((uintptr_t)s->_p) % grain); }

#define CASYSTM_READ_POD(name,type)\
static inline type casystm_read_##name (RStm* s) {\
    assert (casystm_is_read_aligned (s, _Alignof(type)));\
    casystm_read_skip (s, sizeof(type));\
    return ((type*)s->_p)[-1];\
}
CASYSTM_READ_POD (uint8,	uint8_t)
CASYSTM_READ_POD (int8,		int8_t)
CASYSTM_READ_POD (uint16,	uint16_t)
CASYSTM_READ_POD (int16,	int16_t)
CASYSTM_READ_POD (uint32,	uint32_t)
CASYSTM_READ_POD (int32,	int32_t)
CASYSTM_READ_POD (uint64,	uint64_t)
CASYSTM_READ_POD (int64,	int64_t)
CASYSTM_READ_POD (float,	float)
CASYSTM_READ_POD (double,	double)
CASYSTM_READ_POD (bool,		uint8_t)

#define CASYSTM_WRITE_POD(name,type)\
static inline void casystm_write_##name (WStm* s, type v) {\
    assert (casystm_is_write_aligned (s, _Alignof(type)));\
    casystm_write_skip (s, sizeof(type));\
    ((type*)s->_p)[-1] = v;\
}
CASYSTM_WRITE_POD (uint8,	uint8_t)
CASYSTM_WRITE_POD (int8,	int8_t)
CASYSTM_WRITE_POD (uint16,	uint16_t)
CASYSTM_WRITE_POD (int16,	int16_t)
CASYSTM_WRITE_POD (uint32,	uint32_t)
CASYSTM_WRITE_POD (int32,	int32_t)
CASYSTM_WRITE_POD (uint64,	uint64_t)
CASYSTM_WRITE_POD (int64,	int64_t)
CASYSTM_WRITE_POD (float,	float)
CASYSTM_WRITE_POD (double,	double)
CASYSTM_WRITE_POD (bool,	uint8_t)

static inline void casystm_write_align (WStm* s, size_t grain) {
    while (!casystm_is_write_aligned (s, grain))
	casystm_write_uint8 (s, 0);
}

static inline size_t casystm_size_string (const char* v)
    { return Align (sizeof(uint32_t)+strlen(v)+1, 4); }

#ifdef __cplusplus
} // namespace
#endif

const char* casystm_read_string (RStm* s) noexcept;
void casystm_write_string (WStm* s, const char* v) noexcept;

#ifdef __cplusplus
} // extern "C"
#endif
