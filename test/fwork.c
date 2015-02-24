// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"

typedef struct _SApp {
    PProxy	pingp;
    unsigned	pingCount;
} SApp;

static void* App_Create (const SMsg* msg UNUSED)
{
    // Only one app objects can exist, so use the singleton pattern
    static SApp app = {{NULL,0,0},0};
    if (!app.pingp.interface) {	// use the proxy to detect already initialized case
	// Each addressable object must be registered
	casycom_register (&f_PingObject);
	// To create an object, create a proxy object for it
	app.pingp = casycom_create_proxy (&i_Ping, oid_App);
    }
    return &app;
}

static void App_Destroy (void* o UNUSED) {}

static void App_App_Init (SApp* app, unsigned argc UNUSED, const char* const* argv UNUSED)
{
    // Now the ping object can be accessed using PPing methods
    PPing_Ping (&app->pingp, 1);
}

static void App_PingR_Ping (SApp* app, uint32_t u)
{
    printf ("Ping %u reply received in app; count %u\n", u, ++app->pingCount);
    casycom_quit (EXIT_SUCCESS);
}

// Objects are created using an SFactory, containing a list of all
// interfaces implemented by the object. Each interface is associated
// with a dispatch table, containing methods receiving messages for that
// interface. The app must implement at least the App interface.
static const DApp d_App_App = {
    .interface = &i_App,
    DMETHOD (App, App_Init)
};
// Here PingR is also implemented to receive Ping replies
static const DPingR d_App_PingR = {
    .interface = &i_PingR,
    DMETHOD (App, PingR_Ping)
};
// The dtables are then listed in the .dtable field of the factory
static const SFactory f_App = {
    .Create	= App_Create,
    .Destroy	= App_Destroy,
    .dtable	= { &d_App_App, &d_App_PingR, NULL }
};
CASYCOM_MAIN (f_App)
