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
    _casymsg_Msg = (SMsg*) xalloc (sizeof(SMsg));
    if (sz)
	_casymsg_Msg->body = xalloc (sz);
    _casymsg_Msg->interface = pp->interface;
    assert (imethod < casyiface_count_methods (_casymsg_Msg->interface) && "invalid method index for this interface");
    _casymsg_Msg->imethod = imethod;
    _casymsg_Msg->size = sz;
    _casymsg_Msg->src = pp->src;
    _casymsg_Msg->dest = pp->dest;
    _casymsg_Msg->fdoffset = NoFdInMessage;
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

//----------------------------------------------------------------------

uint32_t casyiface_count_methods (iid_t iid)
{
    uint32_t n = 0;
    if (!iid) return n;
    for (const char* const* i = iid->method; *i; ++i)
	++n;
    return n;
}
