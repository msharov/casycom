// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"
#include <sys/socket.h>

#define IPCOM_SOCKET_NAME	"ipcom.socket"

//----------------------------------------------------------------------

DECLARE_VECTOR_TYPE (ProxyVector, PProxy);

typedef struct _SApp {
    PProxy	pingp;
    unsigned	pingCount;
    PProxy	externp;
    pid_t	serverPid;
} SApp;

static const iid_t eil_Ping[] = { &i_Ping, NULL };

//----------------------------------------------------------------------

static void App_ForkAndPipe (SApp* app);
static void App_Client (SApp* app);
static void App_Server (SApp* app);

//----------------------------------------------------------------------

static void* App_Create (const SMsg* msg UNUSED)
    { static SApp o; memset (&o,0,sizeof(o)); return &o; }
static void App_Destroy (void* p UNUSED) {}

static void App_App_Init (SApp* app, unsigned argc, char* const* argv)
{
    // Process command line arguments to determine mode of operation
    enum {
	mode_Test,	// Fork and send commands through a socket pair
	mode_Client,	// Connect to IPCOM_SOCKET_NAME
	mode_Server	// Listen at IPCOM_SOCKET_NAME
    } mode = mode_Test;

    for (int opt; 0 < (opt = getopt (argc, argv, "csd"));) {
	if (opt == 'c')
	    mode = mode_Client;
	else if (opt == 's')
	    mode = mode_Server;
	else if (opt == 'd')
	    casycom_enable_debug_output();
	else {
	    printf ("Usage: ipcom [-csd]\n"
		    "  -c\trun as client only\n"
		    "  -s\trun as server only\n"
		    "  -d\tenable debug tracing\n");
	    exit (EXIT_SUCCESS);
	}
    }
    // Communication between processes requires the use of the Extern
    // interface and auxillary factories for creating passthrough local
    // objects representing both the remote and local addressable objects.
    //
    // The client side creates the Extern object on an fd connected to
    // the server socket and supplies the list of imported interfaces.
    // The imported object types can then be transparently created in
    // the same manner local objects are.
    //
    // Both sides need to start by enabling the externs functionality:
    casycom_enable_externs();
    switch (mode) {
	default:		App_ForkAndPipe (app);	break;
	case mode_Client:	App_Client (app);	break;
	case mode_Server:	App_Server (app);	break;
    }
}

static void App_ForkAndPipe (SApp* app)
{
    app->externp = casycom_create_proxy (&i_Extern, oid_App);
    // To simplify this test program, use the same executable for both
    // client and server, forking, and communicating through a socket pair
    int socks[2];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, 0, socks))
	return casycom_error ("socketpair: %s", strerror(errno));
    int fr = fork();
    if (fr < 0)
	return casycom_error ("fork: %s", strerror(errno));
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

static void App_Client (SApp* app)
{
    // Clients create Extern objects connected to a specific socket
    app->externp = casycom_create_proxy (&i_Extern, oid_App);
    if (0 > PExtern_ConnectUserLocal (&app->externp, IPCOM_SOCKET_NAME, eil_Ping))
	// Connect and Bind calls connect or bind immediately, and so can return errors here
	casycom_error ("Extern_ConnectUserLocal: %s", strerror(errno));
    // Not launching the server, so disable pid checks
    app->serverPid = getpid();
}

static void App_Server (SApp* app)
{
    // When writing a server, it is recommended to use the ExternServer
    // object to manage the sockets being listened to. It will create
    // Extern objects for each accepted connection.
    casycom_register (&f_ExternServer);
    casycom_register (&f_Ping);
    app->externp = casycom_create_proxy (&i_ExternServer, oid_App);
    // Check if using systemd socket activation
    if (sd_listen_fds())	// sd_listen_fds returns number of fds, but for this test, only one is supported
	PExternServer_Open (&app->externp, SD_LISTEN_FDS_START+0, eil_Ping);
    else {	// If not using socket activation, create a socket manually
	// Use BindUserLocal to create the socket in XDG_RUNTIME_DIR,
	// the standard location for sockets of user services.
	// See other Bind variants in xsrv.h.
	if (0 > PExternServer_BindUserLocal (&app->externp, IPCOM_SOCKET_NAME, eil_Ping))
	    casycom_error ("ExternServer_BindUserLocal: %s", strerror(errno));
    }
}

static void App_ExternR_Connected (SApp* app)
{
    // Both directly connected Extern objects and ExternServer objects send this reply.
    if (!app->serverPid)
	return;	// log only the client side
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
