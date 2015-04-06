// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"

// The simplest way to use casycom is in framework mode, which provides
// an event loop, a standard main, and enables fully object-oriented way
// of managing root-level control flow and data by using an app object.
// The app object is constructed from main and destructed with an atexit
// handler. Additionally, framework mode installs signal handlers and
// converts incoming signals to events delivered to the app object.
//
typedef struct _App {
    Proxy	pingp;
    unsigned	pingCount;
} App;

// App object constructor. Note the naming convention, used in lieu of C++ class scope
static void* App_Create (const Msg* msg UNUSED)
{
    // Only one app objects can exist, so use the singleton pattern
    static App app = {PROXY_INIT,0};
    if (!app.pingp.interface) {	// use the proxy to detect already initialized case
	// Each addressable object must be registered
	casycom_register (&f_Ping);
	// To create an object, create a proxy object for it
	app.pingp = casycom_create_proxy (&i_Ping, oid_App);
    }
    return &app;
}

static void App_Destroy (void* o UNUSED) {} // Singletons do not need destruction

// Implements the Init method of App interface on the App object. Note the naming convention.
static void App_App_Init (App* app, unsigned argc UNUSED, const char* const* argv UNUSED)
{
    // Now the ping object can be accessed using PPing methods
    PPing_Ping (&app->pingp, 1);
}

// Implements the Ping method of the PingR interface. In casycom, all method
// calls are one-way and can not return a value. The design is instead fully
// asynchronous, requiring the use of an event loop. Instead of per-method
// replies, a complete reply interface may be associated with an interface.
// By convention, reply interfaces are postfixed with an R.
//
static void App_PingR_Ping (App* app, uint32_t u)
{
    printf ("Ping %u reply received in app; count %u\n", u, ++app->pingCount);
    casycom_quit (EXIT_SUCCESS);
}

// Objects are created using an Factory, containing a list of all
// interfaces implemented by the object. Each interface is associated
// with a dispatch table, containing methods receiving messages for that
// interface. The app must implement at least the App interface.
//
static const DApp d_App_App = {
    .interface = &i_App,
    DMETHOD (App, App_Init)
};

// Here PingR is also implemented to receive Ping replies
//
// (If you forget to implement an interface, but a message for it is sent
//  to the object, an assert will fire in debug builds. Release builds will
//  crash dereferencing a NULL, because ignoring a message can have serious
//  consequences such as user data loss. Such problems must be fixed during
//  development. Debug builds contain many other asserts to catch a variety
//  of errors, and so should always be used during development)
//
static const DPingR d_App_PingR = {
    .interface = &i_PingR,
    DMETHOD (App, PingR_Ping)
};
// The dtables are then listed in the .dtable field of the factory
static const Factory f_App = {
    .Create	= App_Create,
    .Destroy	= App_Destroy,
    .dtable	= { &d_App_App, &d_App_PingR, NULL }
};
// The default main can be generated with a macro taking the app factory
CASYCOM_MAIN (f_App)
