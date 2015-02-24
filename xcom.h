// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"

//{{{
#ifdef __cplusplus
extern "C" {
#endif
//}}}-------------------------------------------------------------------
//{{{ COM interface and object

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

extern const SFactory f_COM;

extern const SInterface* const eil_None[1];

void casycom_register_externs (const SInterface* const* eo) noexcept NONNULL();

//}}}-------------------------------------------------------------------
//{{{ Extern interface and object

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

void PExtern_Attach (const PProxy* pp, int fd, enum EExternAttachType atype) noexcept NONNULL();
void PExtern_Detach (const PProxy* pp) noexcept NONNULL();

extern const SInterface i_Extern;

//----------------------------------------------------------------------

enum EExternRMethod {
    method_ExternR_Connected,
    method_ExternR_N
};
typedef void (*MFN_ExternR_Connected)(void* vo, const SMsg* msg);
typedef struct _DExternR {
    iid_t			interface;
    MFN_ExternR_Connected	ExternR_Connected;
} DExternR;

void PExternR_Connected (const PProxy* pp) noexcept NONNULL();

extern const SInterface i_ExternR;

//----------------------------------------------------------------------

extern const SFactory f_Extern;

//}}}-------------------------------------------------------------------
//{{{
#ifdef __cplusplus
} // extern "C"
#endif
//}}}
