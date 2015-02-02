// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "main.h"

//----------------------------------------------------------------------

static SMsg* _casymsg_Msg = NULL;

// Private import from main.c
void casycom_queue_message (SMsg* msg);

//----------------------------------------------------------------------

WStm casymsg_begin (const PProxy* pp, uint32_t imethod, uint32_t sz)
{
    assert (pp && pp->interface);
    assert (!_casymsg_Msg && "already writing a message");
    assert ((imethod == method_CreateObject || imethod < casyiface_count_methods (pp->interface)) && "invalid method index for this interface");
    _casymsg_Msg = (SMsg*) xalloc (sizeof(SMsg));
    _casymsg_Msg->interface = pp->interface;
    _casymsg_Msg->imethod = imethod;
    _casymsg_Msg->src = pp->src;
    _casymsg_Msg->dest = pp->dest;
    _casymsg_Msg->fdoffset = NO_FD_IN_MESSAGE;
    _casymsg_Msg->body = NULL;
    if (sz) {
	size_t asz = Align (sz, MESSAGE_BODY_ALIGNMENT);
	_casymsg_Msg->body = xalloc (asz);
    }
    _casymsg_Msg->size = sz;
    return (WStm) {
	(char*) _casymsg_Msg->body,
	(char*) _casymsg_Msg->body + _casymsg_Msg->size
    };
}

void casymsg_end (const WStm* stm UNUSED)
{
    assert (_casymsg_Msg && "no message being written");
    assert (stm->_p == stm->_end && "message marshalling buffer overrun");
    assert (stm->_p ==_casymsg_Msg->body + _casymsg_Msg->size && "message marshalling buffer overrun");
    casycom_queue_message (_casymsg_Msg);
    _casymsg_Msg = NULL;
}

void casymsg_from_vector (const PProxy* pp, uint32_t imethod, void* body)
{
    WStm os = casymsg_begin (pp, imethod, 0);
    vector* vbody = (vector*) body;
    size_t asz = Align (vbody->size * vbody->elsize, MESSAGE_BODY_ALIGNMENT);
    vector_reserve (vbody, DivRU(asz,vbody->elsize));
    _casymsg_Msg->body = vbody->d;
    _casymsg_Msg->size = vbody->size * vbody->elsize;
    vector_detach (vbody);
    casystm_write_skip_to_end (&os);
    casymsg_end (&os);
}

void casymsg_forward (const PProxy* pp, SMsg* msg)
{
    assert (pp->interface == msg->interface && "messages can not be forwarded to a different interface");
    WStm os = casymsg_begin (pp, msg->imethod, 0);
    _casymsg_Msg->body = msg->body;
    _casymsg_Msg->size = msg->size;
    msg->size = 0;
    msg->body = NULL;
    casystm_write_skip_to_end (&os);
    casymsg_end (&os);
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
