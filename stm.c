// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "stm.h"

const char* casystm_read_string (RStm* s)
{
    uint32_t vlen = casystm_read_uint32 (s);
    const char* v = s->_p;
    if (!vlen)
	--v;
    casystm_read_skip (s, vlen);
    casystm_read_align (s, sizeof(vlen));
    assert (strnlen(v,s->_p-v) == vlen-1 && "unterminated string in stream");
    return v;
}

void casystm_write_string (WStm* s, const char* v)
{
    uint32_t vlen = 0;
    if (v)
	vlen = strlen(v)+1;
    casystm_write_uint32 (s, vlen);
    if (v) {
	casystm_write_data (s, v, vlen);
	casystm_write_align (s, sizeof(vlen));
    }
}
