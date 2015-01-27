// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "../casycom.h"

//----------------------------------------------------------------------

enum EPingMethod {
    method_Ping_Ping,
    method_Ping_N
};

typedef struct _DPing {
    const void*	Ping_Ping;	// void Ping_Ping (void* vo, uint32_t u, const SMsg* msg)
} DPing;

typedef void (*MFN_Ping_Ping)(void* o, uint32_t v, const SMsg* msg);

static void Ping_Dispatch (const DPing* dtable, void* o, const SMsg* msg);

static const SInterface i_Ping = {
    .name	= "Ping",
    .dispatch	= Ping_Dispatch,
    .method	= { "Ping\0u", NULL }
};

static void Ping_Dispatch (const DPing* dtable, void* o, const SMsg* msg)
{
    assert (dtable && msg);
    assert (msg->interface == &i_Ping);
    if (msg->imethod == method_Ping_Ping) {
	RStm is = casymsg_read (msg);
	uint32_t v = casystm_read_uint32 (&is);
	MDCALL (Ping_Ping, v);
    } else
	assert (!"Invalid method index in message");
}

static void PPing_Ping (const PProxy* pp, uint32_t v)
{
    WStm msg = casymsg_begin (pp, method_Ping_Ping, sizeof(v));
    casystm_write_uint32 (&msg, v);
    casymsg_end (&msg);
}

//----------------------------------------------------------------------

typedef struct _SPingObject {
    unsigned	nPings;
} SPingObject;

static void* PingObject_Create (const SMsg* msg UNUSED)
{
    return xalloc (sizeof(SPingObject));
}

static void PingObject_Ping_Ping (SPingObject* o, uint32_t u)
{
    printf ("Ping: %u, %u total\n", u, ++o->nPings);
}

static const DPing s_DPingObject = {
    .Ping_Ping = PingObject_Ping_Ping
};

static const SObject o_PingObject = {
    .Create = PingObject_Create,
    .interface = {
	{ &i_Ping, &s_DPingObject },
	{ NULL, NULL }
    }
};

//----------------------------------------------------------------------

int main (void)
{
    casycom_framework_init();
    casycom_register (&o_PingObject);
    PProxy pp = casycom_create_proxy (&i_Ping, oid_Broadcast);
    PPing_Ping (&pp, 1);
    return casycom_main();
}
