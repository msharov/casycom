// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"

//{{{ Ping interface ---------------------------------------------------

// Indexes for Ping methods, for the imethod field in the message.
// This is used only when marshalling the message and during its
// dispatch. Remember to keep this synchronized with the dtable.
enum { method_Ping_ping };

// Each interface method has a corresponding proxy method, called
// as if the proxy was the remote object. The proxy methods marshal
// the arguments into a message object and put it in the queue.
void PPing_ping (const Proxy* pp, uint32_t v)
{
    assert (pp->interface == &i_Ping && "the given proxy is for a different interface");
    // casymsg_begin will create a message of the given size (here sizeof(v))
    Msg* msg = casymsg_begin (pp, method_Ping_ping, sizeof(v));
    // casystm functions are defined in stm.h
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, v);
    // When done, call casymsg_end. It will check for errors and queue the message.
    casymsg_end (msg);
}

// dispatch function for the Ping interface
// The arguments are the destination object, its dispatch table, and the message
static void Ping_dispatch (const DPing* dtable, void* o, const Msg* msg)
{
    // The message stores the method as an index into the interface.method array
    if (msg->imethod == method_Ping_ping) {	// Use constant defined above
	RStm is = casymsg_read (msg);
	uint32_t v = casystm_read_uint32 (&is);
	dtable->Ping_ping (o, v);
    } else	// To handle errors, call the default dispatch for unknown methods
	casymsg_default_dispatch (dtable, o, msg);
}

// The interface definition, containing the name, method list, and
// dispatch function pointer.
const Interface i_Ping = {
    .name	= "Ping",
    .dispatch	= Ping_dispatch,
    .method	= { "ping\0u", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ PingR interface

enum { method_PingR_ping };

void PPingR_ping (const Proxy* pp, uint32_t v)
{
    assert (pp->interface == &i_PingR && "the given proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_PingR_ping, sizeof(v));
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, v);
    casymsg_end (msg);
}

static void PingR_dispatch (const DPingR* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_PingR_ping) {
	RStm is = casymsg_read (msg);
	uint32_t v = casystm_read_uint32 (&is);
	dtable->PingR_ping (o, v);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

// The interface definition, containing the name, method list, and
// dispatch function pointer.
const Interface i_PingR = {
    .name	= "PingR",
    .dispatch	= PingR_dispatch,
    .method	= { "ping\0u", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ Ping server object

// The ping server object data
typedef struct _Ping {
    Proxy	reply;
    unsigned	npings;
} Ping;

// The constructor for the object, called when the first message sent
// to it arrives. It must return a valid object pointer. Here, a new
// object is allocated and returned. (xalloc will zero the memory)
// msg->dest is the new object's oid, which may be saved if needed.
static void* Ping_create (const Msg* msg)
{
    LOG ("Created Ping %u\n", msg->h.dest);
    Ping* po = xalloc (sizeof(Ping));
    po->reply = casycom_create_reply_proxy (&i_PingR, msg);
    return po;
}

// Object destructor. If defined, must free the object.
static void Ping_destroy (void* o)
{
    LOG ("Destroy Ping\n");
    xfree (o);
}

// Method implementing the Ping.ping interface method
static void Ping_Ping_ping (Ping* o, uint32_t u)
{
    LOG ("Ping: %u, %u total\n", u, ++o->npings);
    PPingR_ping (&o->reply, u);
}

// The Ping DTable for the Ping interface
static const DPing d_Ping_Ping = {
    .interface = &i_Ping,
    // There are two ways to define the methods. First, you could
    // use the same signature as defined in DPing, which requires
    // the passed in object to be a void* (because when the interface
    // is declared, this server object is not and can not be unknown)
    // .Ping_ping = Ping_Ping_ping
    // You then have to cast that to the object type in each method.
    // The second way is to define the method with the typed object
    // pointer and use the DMETHOD macro to cast it in the DTable.
    // The advantage is not having to cast in the method, the
    // disadvantage is that the method's signature is not checked.
    DMETHOD (Ping, Ping_ping)
};

// Factory defines the methods required to create objects of this
// type. The framework will do so when receiving a  message addressed
// to an interface listed in the .dtable array.
const Factory f_Ping = {
    .create = Ping_create,
    .destroy = Ping_destroy,
    // Lists each interface implemented by this object
    .dtable = { &d_Ping_Ping, NULL }
};

//}}}-------------------------------------------------------------------
