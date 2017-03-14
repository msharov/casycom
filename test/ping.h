// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "../casycom.h"

//----------------------------------------------------------------------
// For communication between objects, both sides of the link must be
// defined as well as the interface. The interface object, by convention
// named i_Name, is of type Interface and contains its name, method
// names and signatures, and the dispatch method pointer. The proxy
// "object" is used to send messages to the remote object, and contains
// methods mirroring those on the remote object, that are implemented
// by marshalling the arguments into a message. The remote object is
// created by the framework, using the Factory f_Name, when the message
// is to be delivered.

// Method types for the dispatch table
typedef void (*MFN_Ping_Ping)(void* o, uint32_t v);
// The dispatch table for the dispatch method
typedef struct _DPing {
    const Interface*	interface;	// Pointer to the interface this DTable implements
    MFN_Ping_Ping	Ping_Ping;	// void Ping_Ping (void* vo, uint32_t u, const Msg* msg)
} DPing;

// Ping proxy methods, matching the list in DPing
void PPing_Ping (const Proxy* pp, uint32_t v);

// This is the interface object; iid_t is the pointer to it.
extern const Interface i_Ping;

//----------------------------------------------------------------------
// Replies are reply interfaces, conventionally named NameR
// Implementation is the same as above

typedef void (*MFN_PingR_Ping)(void* o, uint32_t v);
typedef struct _DPingR {
    const Interface*	interface;
    MFN_PingR_Ping	PingR_Ping;
} DPingR;
void PPingR_Ping (const Proxy* pp, uint32_t v);

extern const Interface i_PingR;

//----------------------------------------------------------------------
// This is the server object, to pass to casycom_register
extern const Factory f_Ping;

//----------------------------------------------------------------------
// Define a synchronous write to avoid race conditions between processes
#define LOG(...)	{printf(__VA_ARGS__);fflush(stdout);}
