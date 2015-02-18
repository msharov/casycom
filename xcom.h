// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"

//----------------------------------------------------------------------
// PCOM

enum ECOMMethod {
    method_COM_Error,
    method_COM_Export,
    method_COM_Delete,
    method_COM_N
};
typedef void (*MFN_COM_Error)(void* vo, const char* error, const SMsg* msg);
typedef void (*MFN_COM_Export)(void* vo, const char* elist, const SMsg* msg);
typedef void (*MFN_COM_Delete)(void* vo, const SMsg* msg);
typedef struct _DCOM {
    iid_t		interface;
    MFN_COM_Error	COM_Error;
    MFN_COM_Export	COM_Export;
    MFN_COM_Delete	COM_Delete;
} DCOM;

extern const SInterface i_COM;

void PCOM_Error (const PProxy* pp, const char* error);
void PCOM_Export (const PProxy* pp, const char* elist);
void PCOM_Delete (const PProxy* pp);

//----------------------------------------------------------------------
// COM object

typedef struct _SCOM {
    PProxy	reply;
    PProxy	forwd;
} SCOM;

void* COM_Create (const SMsg* msg) noexcept NONNULL();
void COM_Destroy (void* o) noexcept NONNULL();
void COM_ObjectDestroyed (void* o, oid_t oid) noexcept;
bool COM_Error (void* o, oid_t eoid, const char* msg) noexcept NONNULL();

void COM_COM_Error (SCOM* o, const char* error, SMsg* msg) noexcept NONNULL();
void COM_COM_Export (SCOM* o, const char* elist, const SMsg* msg) noexcept NONNULL();
void COM_COM_Delete (SCOM* o, const SMsg* msg) noexcept NONNULL();
void COM_COM_Message (SCOM* o, SMsg* msg) noexcept NONNULL();

extern const SObject o_COM;

#define DEFINE_EXTERN_DTABLE(name,iface)	\
const DCOM name = {				\
    .interface = iface;				\
    DMETHOD (COM, COM_Error);			\
    DMETHOD (COM, COM_Export);			\
    DMETHOD (COM, COM_Delete);			\
}

#define DEFINE_EXTERNS_OBJECT(name, ...)	\
const SObject name = {				\
    .Create		= COM_Create,		\
    .Destroy		= COM_Destroy,		\
    .ObjectDestroyed	= COM_ObjectDestroyed,	\
    .Error		= COM_Error,		\
    .interface		= { __VA_ARGS__, NULL }	\
}

//----------------------------------------------------------------------
// PExtern

enum EExternMethod {
    method_Extern_Attach,
    method_Extern_Detach,
    method_Extern_N
};
enum EExternAttachType {
    ATTACH_AS_SERVER,
    ATTACH_AS_CLIENT
};
typedef void (*MFN_Extern_Attach)(void* vo, int fd, enum EExternAttachType atype, const SMsg* msg);
typedef void (*MFN_Extern_Detach)(void* vo, const SMsg* msg);
typedef struct _DExtern {
    iid_t		interface;
    MFN_Extern_Attach	Extern_Attach;
    MFN_Extern_Detach	Extern_Detach;
} DExtern;

extern const SInterface i_Extern;

void PExtern_Attach (const PProxy* pp, int fd, enum EExternAttachType atype);
void PExtern_Detach (const PProxy* pp);

//----------------------------------------------------------------------
// Extern object
