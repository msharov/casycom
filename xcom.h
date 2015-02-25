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
    method_Extern_Open,
    method_Extern_Close,
    method_Extern_N
};
enum EExternType {
    EXTERN_CLIENT,
    EXTERN_SERVER
};
typedef void (*MFN_Extern_Open)(void* vo, int fd, enum EExternType atype, const SMsg* msg);
typedef void (*MFN_Extern_Close)(void* vo, const SMsg* msg);
typedef struct _DExtern {
    iid_t		interface;
    MFN_Extern_Open	Extern_Open;
    MFN_Extern_Close	Extern_Close;
} DExtern;

void PExtern_Open (const PProxy* pp, int fd, enum EExternType atype) noexcept NONNULL();
void PExtern_Close (const PProxy* pp) noexcept NONNULL();

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
