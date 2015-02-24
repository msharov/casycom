// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"

typedef struct _SApp {
    PProxy	pingp;
    unsigned	pingCount;
} SApp;

static void App_App_Init (SApp* app, unsigned argc UNUSED, const char* const* argv UNUSED)
{
    #ifndef NDEBUG
	casycom_enable_debug_output();
    #endif
    // Communication between processes requires the use of the Extern
    // interface and auxillary factories for creating passthrough local
    // objects representing both the remote and local addressable objects.
#if 0
    // To simplify this test program, use the same executable for both
    // client and server, forking, and communicating through a socket pair
    int socks[2];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, 0, socks)) {
	perror ("socketpair");
	return EXIT_FAILURE;
    }
    int fr = fork();
    if (fr < 0) {
	perror ("fork");
	return EXIT_FAILURE;
    }
#endif
    int fr = 1;
    if (fr == 0) {	// Server side
	// Export the ping interface on this side
	static const SInterface* const eil_Ping[] = { &i_Ping, NULL };
	casycom_register_externs (eil_Ping);
    } else {		// Client side
	// List interfaces that can be accessed from outside.
	// On the client side, no interfaces are exported.
	casycom_register_externs (eil_None);
	// To create a remote object, create a proxy object for it
	// (This is exactly the same as accessing a local object)
	app->pingp = casycom_create_proxy (&i_Ping, oid_App);
	// ... which can then be accessed through the proxy methods.
	PPing_Ping (&app->pingp, 1);
    }
}

static void App_ExternR_Connected (SApp* app UNUSED)
{
    printf ("Connected to server\n");
}

static void App_PingR_Ping (SApp* app, uint32_t u)
{
    printf ("Ping %u reply received in app; count %u\n", u, ++app->pingCount);
    casycom_quit (EXIT_SUCCESS);
}

CASYCOM_DEFINE_SINGLETON(App)
static const DApp d_App_App = {
    .interface = &i_App,
    DMETHOD (App, App_Init)
};
static const DPingR d_App_PingR = {
    .interface = &i_PingR,
    DMETHOD (App, PingR_Ping)
};
static const DExternR d_App_ExternR = {
    .interface = &i_ExternR,
    DMETHOD (App, ExternR_Connected)
};
static const SFactory f_App = {
    .Create	= App_Create,
    .Destroy	= App_Destroy,
    .dtable	= { &d_App_App, &d_App_PingR, &d_App_ExternR, NULL }
};
CASYCOM_MAIN (f_App)
