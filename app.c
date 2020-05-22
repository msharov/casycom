// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#include "app.h"

//----------------------------------------------------------------------
// PApp

enum {
    method_App_init,
    method_App_signal
};

void PApp_init (const Proxy* pp, argc_t argc, argv_t argv)
{
    Msg* msg = casymsg_begin (pp, method_App_init, 12);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, argv);
    casystm_write_uint32 (&os, argc);
    casymsg_end (msg);
}

void PApp_signal (const Proxy* pp, unsigned sig, pid_t child_pid, int child_status)
{
    Msg* msg = casymsg_begin (pp, method_App_signal, 12);
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, sig);
    casystm_write_uint32 (&os, child_pid);
    casystm_write_int32 (&os, child_status);
    casymsg_end (msg);
}

static void PApp_dispatch (const DApp* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_App_init) {
	RStm is = casymsg_read (msg);
	argv_t argv = casystm_read_ptr (&is);
	argc_t argc = casystm_read_uint32 (&is);
	if (dtable->App_init)
	    dtable->App_init (o, argc, argv);
    } else if (msg->imethod == method_App_signal) {
	RStm is = casymsg_read (msg);
	unsigned sig = casystm_read_uint32 (&is);
	pid_t child_pid = casystm_read_uint32 (&is);
	int child_status = casystm_read_int32 (&is);
	if (dtable->App_signal)
	    dtable->App_signal (o, sig, child_pid, child_status);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_App = {
    .dispatch = PApp_dispatch,
    .name = "App",
    .method = { "init\0xu", "signal\0uui", NULL }
};
