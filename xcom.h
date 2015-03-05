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
typedef void (*MFN_Extern_Open)(void* vo, int fd, enum EExternType atype, const iid_t* importedInterfaces, const iid_t* exportedInterfaces);
typedef void (*MFN_Extern_Close)(void* vo);
typedef struct _DExtern {
    iid_t		interface;
    MFN_Extern_Open	Extern_Open;
    MFN_Extern_Close	Extern_Close;
} DExtern;

void PExtern_Open (const PProxy* pp, int fd, enum EExternType atype, const iid_t* importInterfaces, const iid_t* exportInterfaces) noexcept NONNULL(1);
void PExtern_Close (const PProxy* pp) noexcept NONNULL();

extern const SInterface i_Extern;

void casycom_enable_externs (void) noexcept;

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

//}}}-------------------------------------------------------------------
//{{{
#ifdef __cplusplus
} // extern "C"
#endif
//}}}
