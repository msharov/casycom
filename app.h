// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#pragma once
#include "msg.h"
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// PApp

typedef void (*MFN_App_init)(void* o, argc_t argc, argv_t argv);
typedef void (*MFN_App_signal)(void* o, unsigned sig, pid_t child_pid, int child_status);
typedef struct _DApp {
    iid_t		interface;
    MFN_App_init	App_init;
    MFN_App_signal	App_signal;
} DApp;

extern const Interface i_App;

void PApp_init (const Proxy* pp, argc_t argc, argv_t argv) noexcept NONNULL();
void PApp_signal (const Proxy* pp, unsigned sig, pid_t child_pid, int child_status) noexcept NONNULL();

#define CASYCOM_MAIN(oapp)			\
int main (int argc, argv_t argv) {		\
    casycom_framework_init (&oapp, argc, argv);	\
    return casycom_main();			\
}

#ifdef __cplusplus
} // extern "C"
#endif
