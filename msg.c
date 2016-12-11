// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "main.h"

Msg* casymsg_begin (const Proxy* pp, uint32_t imethod, uint32_t sz)
{
    Msg* msg = xalloc (sizeof(Msg));
    msg->h = *pp;
    msg->imethod = imethod;
    msg->fdoffset = NO_FD_IN_MESSAGE;
    if ((msg->size = sz))
	msg->body = xalloc (Align (sz, MESSAGE_BODY_ALIGNMENT));
    return msg;
}

void casymsg_from_vector (const Proxy* pp, uint32_t imethod, void* body)
{
    Msg* msg = casymsg_begin (pp, imethod, 0);
    WStm os = casymsg_write (msg);
    CharVector* vbody = body;
    size_t asz = Align (vbody->size * vbody->elsize, MESSAGE_BODY_ALIGNMENT);
    vector_reserve (vbody, DivRU(asz,vbody->elsize));
    msg->body = vbody->d;
    msg->size = vbody->size * vbody->elsize;
    vector_detach (vbody);
    casystm_write_skip_to_end (&os);
    casymsg_end (msg);
}

void casymsg_forward (const Proxy* pp, Msg* msg)
{
    Msg* fwm = casymsg_begin (pp, msg->imethod, 0);
    fwm->h.interface = msg->h.interface;
    fwm->body = msg->body;
    fwm->size = msg->size;
    fwm->extid = msg->extid;
    fwm->fdoffset = msg->fdoffset;
    msg->size = 0;
    msg->body = NULL;
    casymsg_end (fwm);
}

//----------------------------------------------------------------------

uint32_t casyiface_count_methods (iid_t iid)
{
    uint32_t n = 0;
    if (!iid) return n;
    for (const char* const* i = iid->method; *i; ++i)
	++n;
    return n;
}

static size_t casymsg_sigelement_size (char c)
{
    static const struct { char sym; uint8_t sz; } syms[] =
	{{'y',1},{'b',1},{'n',2},{'q',2},{'i',4},{'u',4},{'h',4},{'x',8},{'t',8}};
    for (unsigned i = 0; i < ArraySize(syms); ++i)
	if (syms[i].sym == c)
	    return syms[i].sz;
    return 0;
}

static void casymsg_skip_one_sigelement (const char** sig)
{
    unsigned parens = 0;
    do {
	if (**sig == '(')
	    ++parens;
	else if (**sig == ')')
	    --parens;
    } while (*++*sig && parens);
}

static size_t casymsg_validate_sigelement (const char** sig, RStm* buf)
{
    size_t sz = casymsg_sigelement_size (**sig);
    assert ((sz || **sig == '(' || **sig == 'a' || **sig == 's') && "invalid character in method signature");
    if (sz) {		// Zero size is returned for compound elements, which are structs, arrays, or strings.
	++*sig;		// single char element
	if (!casystm_can_read (buf, sz) || !casystm_is_read_aligned (buf, sz))
	    return 0;	// invalid data in buf
	casystm_read_skip (buf, sz);
    } else if (**sig == '(') {				// Structs. Scan forward until ')'.
	++*sig;
	for (size_t ssz; **sig && **sig != ')'; sz += ssz)
	    if (!(ssz = casymsg_validate_sigelement (sig, buf)))		// invalid data in buf, return 0 as error
		return 0;
    } else if (**sig == 'a' || **sig == 's') {		// Arrays and strings
	if (!casystm_can_read (buf, 4))
	    return 0;
	uint32_t nel = casystm_read_uint32 (buf);	// number of elements in the array
	sz += 4;
	size_t elsz = 1;	// strings are equivalent to "ac"
	if (*++*sig == 'a')	// arrays are followed by an element sig "a(uqq)"
	    elsz = casymsg_sigelement_size (**sig);
	if (elsz) {		// optimization for the common case of fixed-element array
	    elsz *= nel;
	    if (!casystm_can_read (buf, elsz))
		return 0;
	    casystm_read_skip (buf, elsz);
	    sz += elsz;
	} else {
	    for (uint32_t i = 0; i < nel; ++i, sz += elsz) {	// read each element
		const char* elsig = *sig;			// for each element, pass in the same element sig
		if (!(elsz = casymsg_validate_sigelement (&elsig, buf)))	// invalid data in buf, return 0 as error
		    return 0;
	    }
	}
	if ((*sig)[-1] == 'a')	// skip the array element sig for arrays; strings do not have one
	    casymsg_skip_one_sigelement (sig);
	else {			// for strings, verify zero-termination
	    casystm_unread (buf, 1);
	    if (casystm_read_uint8 (buf))
		return 0;
	}
	size_t alignsz = Align(sz,4)-sz;	// at the end, both arrays and strings are aligned to 4-grain
	if (!casystm_can_read (buf, alignsz))
	    return 0;
	casystm_read_skip (buf, alignsz);
	sz += alignsz;
    }
    return sz;
}

size_t casymsg_validate_signature (const Msg* msg)
{
    RStm is = casymsg_read(msg);
    size_t sz = 0;
    for (const char* sig = casymsg_signature(msg); *sig;) {
	size_t elsz = casymsg_validate_sigelement (&sig, &is);
	if (!elsz)
	    return 0;
	sz += elsz;
    }
    return sz;
}
