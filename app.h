// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "msg.h"
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// PApp

typedef void (*MFN_App_Init)(void* o, argc_t argc, argv_t argv);
typedef void (*MFN_App_Signal)(void* o, unsigned sig, pid_t childPid, int childStatus);
typedef struct _DApp {
    iid_t		interface;
    MFN_App_Init	App_Init;
    MFN_App_Signal	App_Signal;
} DApp;

extern const Interface i_App;

void PApp_Init (const Proxy* pp, argc_t argc, argv_t argv) noexcept NONNULL();
void PApp_Signal (const Proxy* pp, unsigned sig, pid_t childPid, int childStatus) noexcept NONNULL();

#define CASYCOM_MAIN(oapp)			\
int main (int argc, argv_t argv) {		\
    casycom_framework_init (&oapp, argc, argv);	\
    return casycom_main();			\
}

#ifdef __cplusplus
} // extern "C"
#endif
