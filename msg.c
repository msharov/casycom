// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "msg.h"

SMsg* casymsg_create (uint32_t sz)
{
    SMsg* msg = (SMsg*) calloc (1, sizeof(SMsg));
    if (!msg)
	return NULL;
    msg->body = calloc (1, sz);
    if (!msg->body) {
	free (msg);
	return NULL;
    }
    msg->size = sz;
    return msg;
}
