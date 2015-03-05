// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"
#include <sys/socket.h>

typedef struct _SApp {
    PProxy	externp;
    pid_t	serverPid;
    PProxy	pingp;
    unsigned	pingCount;
} SApp;

static void App_App_Init (SApp* app, unsigned argc UNUSED, const char* const* argv UNUSED)
{
    casycom_enable_externs();
    // Communication between processes requires the use of the Extern
    // interface and auxillary factories for creating passthrough local
    // objects representing both the remote and local addressable objects.
    app->externp = casycom_create_proxy (&i_Extern, oid_App);
    // To simplify this test program, use the same executable for both
    // client and server, forking, and communicating through a socket pair
    int socks[2];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, 0, socks))
	return casycom_error ("socketpair: %s", strerror(errno));
    int fr = fork();
    if (fr < 0)
	return casycom_error ("fork: %s", strerror(errno));
    static const iid_t const eil_Ping[] = { &i_Ping, NULL };
    if (fr == 0) {	// Server side
	close (socks[0]);
	casycom_register (&f_Ping);
	// Export the ping interface on this side, and import nothing.
	PExtern_Open (&app->externp, socks[1], EXTERN_SERVER, NULL, eil_Ping);
    } else {		// Client side
	app->serverPid = fr;
	close (socks[1]);
	// List interfaces that can be accessed from outside.
	// On the client side, no interfaces are exported, but ping is imported.
	PExtern_Open (&app->externp, socks[0], EXTERN_CLIENT, eil_Ping, NULL);
    }
}

static void App_ExternR_Connected (SApp* app)
{
    if (!app->serverPid)
	return;
    printf ("Connected to server\n");
    // To create a remote object, create a proxy object for it
    // (This is exactly the same as accessing a local object)
    app->pingp = casycom_create_proxy (&i_Ping, oid_App);
    // ... which can then be accessed through the proxy methods.
    PPing_Ping (&app->pingp, 1);
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
