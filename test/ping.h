// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "../casycom.h"

//----------------------------------------------------------------------
// For communication between objects, both sides of the link must be
// defined as well as the interface. The interface object, by convention
// named i_Name, is of type SInterface and contains its name, method
// names and signatures, and the dispatch method pointer. The proxy
// object is used to send messages to the remote object, and contains
// methods mirroring those on the remote object, that are implemented
// by marshalling the arguments into a message. The remote object is
// created by the framework, using the SFactory f_Name, when the message
// is to be delivered.

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

// Ping proxy methods, matching the list in EPingMethod and DPing
void PPing_Ping (const PProxy* pp, uint32_t v);

// This is the interface object; iid_t is the pointer to it.
extern const SInterface i_Ping;

//----------------------------------------------------------------------
// Replies are reply interfaces, conventionally named NameR
// Implementation is the same as above

enum EPingRMethod {
    method_PingR_Ping,
    method_PingR_N
};
typedef void (*MFN_PingR_Ping)(void* o, uint32_t v, const SMsg* msg);
typedef struct _DPingR {
    const SInterface*	interface;
    MFN_PingR_Ping	PingR_Ping;
} DPingR;
void PPingR_Ping (const PProxy* pp, uint32_t v);

extern const SInterface i_PingR;

//----------------------------------------------------------------------
// This is the server object, to pass to casycom_register
extern const SFactory f_PingObject;
