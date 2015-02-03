// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "../casycom.h"

//----------------------------------------------------------------------
// Ping interface

// Indexes for Ping methods, for the imethod field in the message
enum EPingMethod {
    method_Ping_Ping,
    method_Ping_N
};
// Method types for the dispatch table
typedef void (*MFN_Ping_Ping)(void* o, uint32_t v, const SMsg* msg);
// The dispatch table for the dispatch method
typedef struct _DPing {
    const SInterface*	interface;	// Pointer to the interface this DTable implements
    MFN_Ping_Ping	Ping_Ping;	// void Ping_Ping (void* vo, uint32_t u, const SMsg* msg)
} DPing;
// The dispatch method that will unmarshal the arguments and call the
// appropriate method through the dtable.
static void Ping_Dispatch (const DPing* dtable, void* o, const SMsg* msg);

// The interface definition, containing the name, method list, and
// dispatch function pointer.
static const SInterface i_Ping = {
    .name	= "Ping",
    .dispatch	= Ping_Dispatch,
    .method	= { "Ping\0u", NULL }
};

// Dispatch function for the Ping interface
// The arguments are the destination object, its dispatch table, and the message
static void Ping_Dispatch (const DPing* dtable, void* o, const SMsg* msg)
{
    // The message stores the method as an index into the interface.method array
    if (msg->imethod == method_Ping_Ping) {	// Use constant defined above
	RStm is = casymsg_read (msg);
	uint32_t v = casystm_read_uint32 (&is);
	dtable->Ping_Ping (o, v, msg);
    } else	// To handle errors, call the default dispatch for unknown methods
	casymsg_default_dispatch (dtable, o, msg);
}

// Each interface method has a corresponding proxy method, called
// as if the proxy was the remote object. The proxy methods marshal
// the arguments into a message object and put it in the queue.
static void PPing_Ping (const PProxy* pp, uint32_t v)
{
    // casymsg_begin will create a message of the given size (here sizeof(v))
    SMsg* msg = casymsg_begin (pp, method_Ping_Ping, sizeof(v));
    // casystm functions are defined in stm.h
    WStm os = casymsg_write (msg);
    casystm_write_uint32 (&os, v);
    // When done, call casymsg_end. It will check for errors and queue the message.
    casymsg_end (msg);
}

//----------------------------------------------------------------------
// Ping server object

// The ping server object data
typedef struct _SPingObject {
    unsigned	nPings;
} SPingObject;

// The constructor for the object, called when the first message sent
// to it arrives. It must return a valid object pointer. Here, a new
// object is allocated and returned. (xalloc will zero the memory)
// msg->dest is the new object's oid, which may be saved if needed.
static void* PingObject_Create (const SMsg* msg)
{
    printf ("Created PingObject %u\n", msg->dest);
    return xalloc (sizeof(SPingObject));
}

// Object destructor. If defined, must free the object.
static void PingObject_Destroy (void* o)
{
    printf ("Destroy PingObject\n");
    xfree (o);
}

// Method implementing the Ping.Ping interface method
static void PingObject_Ping_Ping (SPingObject* o, uint32_t u)
{
    printf ("Ping: %u, %u total\n", u, ++o->nPings);
}

// The PingObject DTable for the Ping interface
static const DPing d_PingObject_Ping = {
    .interface = &i_Ping,
    // There are two ways to define the methods. First, you could
    // use the same signature as defined in DPing, which requires
    // the passed in object to be a void* (because when the interface
    // is declared, this server object is not and can not be unknown)
    // .Ping_Ping = PingObject_Ping_Ping
    // You then have to cast that to the object type in each method.
    // The second way is to define the method with the typed object
    // pointer and use the DMETHOD macro to cast it in the DTable.
    // The advantage is not having to cast in the method, the
    // disadvantage is that the method's signature is not checked.
    DMETHOD (PingObject, Ping_Ping)
};

// SObject defines the methods required to create objects of this
// type. The framework will do so when receiving a  message addressed
// to an interface listed in the .interface array.
static const SObject o_PingObject = {
    .Create = PingObject_Create,
    .Destroy = PingObject_Destroy,
    // Lists each interface implemented by this object
    .interface = { &d_PingObject_Ping, NULL }
};

//----------------------------------------------------------------------

int main (void)
{
    // Use casycom_init if you are using another top-level framework
    casycom_framework_init();
    // Each addressable object must be registered
    casycom_register (&o_PingObject);
    // To create a remote object, create a proxy object for it
    PProxy pp = casycom_create_proxy (&i_Ping, oid_Broadcast);
    // ... which can then be accessed through the proxy methods.
    PPing_Ping (&pp, 1);
    // Finally, run the main loop.
    // The main loop runs until casycom_quit is called or the queues empty.
    return casycom_main();
}
