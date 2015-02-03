// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "main.h"

SMsg* casymsg_begin (const PProxy* pp, uint32_t imethod, uint32_t sz)
{
    assert (pp && pp->interface);
    assert (!msg && "already writing a message");
    assert ((imethod == method_CreateObject || imethod < casyiface_count_methods (pp->interface)) && "invalid method index for this interface");
    SMsg* msg = (SMsg*) xalloc (sizeof(SMsg));
    msg->interface = pp->interface;
    msg->imethod = imethod;
    msg->src = pp->src;
    msg->dest = pp->dest;
    msg->fdoffset = NO_FD_IN_MESSAGE;
    msg->body = NULL;
    if (sz) {
	size_t asz = Align (sz, MESSAGE_BODY_ALIGNMENT);
	msg->body = xalloc (asz);
    }
    msg->size = sz;
    return msg;
}

void casymsg_from_vector (const PProxy* pp, uint32_t imethod, void* body)
{
    SMsg* msg = casymsg_begin (pp, imethod, 0);
    WStm os = casymsg_write (msg);
    vector* vbody = (vector*) body;
    size_t asz = Align (vbody->size * vbody->elsize, MESSAGE_BODY_ALIGNMENT);
    vector_reserve (vbody, DivRU(asz,vbody->elsize));
    msg->body = vbody->d;
    msg->size = vbody->size * vbody->elsize;
    vector_detach (vbody);
    casystm_write_skip_to_end (&os);
    casymsg_end (msg);
}

void casymsg_forward (const PProxy* pp, SMsg* msg)
{
    assert (pp->interface == msg->interface && "messages can not be forwarded to a different interface");
    SMsg* fwm = casymsg_begin (pp, msg->imethod, 0);
    fwm->body = msg->body;
    fwm->size = msg->size;
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
