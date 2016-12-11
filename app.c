// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "app.h"

//----------------------------------------------------------------------
// PApp

enum {
    method_App_Init,
    method_App_Signal
};

void PApp_Init (const Proxy* pp, unsigned argc, const char* const* argv)
{
    Msg* msg = casymsg_begin (pp, method_App_Init, 12);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, argv);
    casystm_write_uint32 (&os, argc);
    casymsg_end (msg);
}

void PApp_Signal (const Proxy* pp, unsigned sig, pid_t childPid, int childStatus)
{
    Msg* msg = casymsg_begin (pp, method_App_Signal, 12);
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, sig);
    casystm_write_uint32 (&os, childPid);
    casystm_write_int32 (&os, childStatus);
    casymsg_end (msg);
}

static void PApp_Dispatch (const DApp* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_App_Init) {
	RStm is = casymsg_read (msg);
	const char* const* argv = casystm_read_ptr (&is);
	unsigned argc = casystm_read_uint32 (&is);
	if (dtable->App_Init)
	    dtable->App_Init (o, argc, argv);
    } else if (msg->imethod == method_App_Signal) {
	RStm is = casymsg_read (msg);
	unsigned sig = casystm_read_uint32 (&is);
	pid_t childPid = casystm_read_uint32 (&is);
	int childStatus = casystm_read_int32 (&is);
	if (dtable->App_Signal)
	    dtable->App_Signal (o, sig, childPid, childStatus);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_App = {
    .dispatch = PApp_Dispatch,
    .name = "App",
    .method = { "Init\0xu", "Signal\0uui", NULL }
};
