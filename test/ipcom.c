// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"

//----------------------------------------------------------------------

typedef struct _App {
    Proxy	pingp;
    unsigned	npings;
    Proxy	externp;
    pid_t	server_pid;
} App;

//----------------------------------------------------------------------
// It is easy to connect to and to create object servers with casycom.
// ipcom illustrates the three common methods of doing so:
// App_client demonstrates how to connect to an object server.
static void App_client (App* app);

// App_server demonstrates how to create an object server.
static void App_server (App* app);

// App_pipe demonstrates how to create an object server on a socket pipe.
static void App_pipe (App* app);

// App_fork_and_pipe launches object server and connects to it
// using a socketpair pipe. This can be used for private servers,
// but here it is just to make running the automated test easy.
static void App_fork_and_pipe (App* app);

// When you run an object server, you must specify a list of interfaces
// it is capable of instantiating. In this example, the only interface
// exported is i_Ping (see ping.h). The client side sends a NULL export
// list when it only imports interfaces.
static const iid_t eil_Ping[] = { &i_Ping, NULL };

// Object servers can be run on a UNIX socket or a TCP port. ipcom shows
// the UNIX socket version. These sockets are created in system standard
// locations; typically /run for root processes or /run/user/<uid> for
// processes launched by the user. If you also implement systemd socket
// activation (see below), any other sockets can be used.
static const char c_IPCOM_socket_name[] = "ipcom.socket";

//----------------------------------------------------------------------

// Singleton App, like in the fwork example
static void* App_create (const Msg* msg UNUSED)
    { static App o = {}; return &o; }
static void App_destroy (void* p UNUSED) {}

static void App_App_init (App* app, argc_t argc, argv_t argv)
{
    // Process command line arguments to determine mode of operation
    enum {
	mode_Test,	// Fork and send commands through a socket pair
	mode_client,	// Connect to c_IPCOM_socket_name
	mode_server,	// Listen at c_IPCOM_socket_name
	mode_pipe	// Connect to socket on stdin
    } mode = mode_Test;

    for (int opt; 0 < (opt = getopt (argc, argv, "cspd"));) {
	if (opt == 'c')
	    mode = mode_client;
	else if (opt == 's')
	    mode = mode_server;
	else if (opt == 'p')
	    mode = mode_pipe;
	else if (opt == 'd')
	    casycom_enable_debug_output();
	else {
	    LOG ("Usage: ipcom [-csd]\n"
		    "  -c\trun as client only\n"
		    "  -s\trun as server only\n"
		    "  -p\tattach to socket pipe on stdin\n"
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
	default:		App_fork_and_pipe (app);	break;
	case mode_client:	App_client (app);	break;
	case mode_server:	App_server (app);	break;
	case mode_pipe:		App_pipe (app);		break;
    }
}

static void App_fork_and_pipe (App* app)
{
    // To simplify this test program, use the same executable for both
    // client and server, forking, and communicating through a socket pair
    int socks[2];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK, 0, socks))
	return casycom_error ("socketpair: %s", strerror(errno));
    int fr = fork();
    if (fr < 0)
	return casycom_error ("fork: %s", strerror(errno));
    if (fr == 0) {	// Server side
	close (socks[0]);
	dup2 (socks[1], STDIN_FILENO);
	App_pipe (app);
    } else {		// Client side
	app->server_pid = fr;
	app->externp = casycom_create_proxy (&i_Extern, oid_App);
	close (socks[1]);
	// List interfaces that can be accessed from outside.
	// On the client side, Ping is imported and no interfaces are exported.
	PExtern_open (&app->externp, socks[0], EXTERN_CLIENT, eil_Ping, NULL);
    }
}

static void App_client (App* app)
{
    // Clients create Extern objects connected to a specific socket
    app->externp = casycom_create_proxy (&i_Extern, oid_App);
    //
    // connect_user_local connects to the named UNIX socket in the user's runtime
    // directory (typically /run/user/<uid>). There are also connect_system_local
    // for connecting to system services, and methods for connecting to TCP services,
    // connect_ip4, connect_local_ip4, etc. See xcom.h.
    //
    if (0 > PExtern_connect_user_local (&app->externp, c_IPCOM_socket_name, eil_Ping))
	// Connect and Bind calls connect or bind immediately, and so can return errors here
	casycom_error ("Extern_connect_user_local: %s", strerror(errno));
    // Not launching the server, so disable pid checks
    app->server_pid = getpid();
}

static void App_server (App* app)
{
    // When writing a server, it is recommended to use the ExternServer
    // object to manage the sockets being listened to. It will create
    // Extern objects for each accepted connection. For the purpose of
    // this example, only one socket is listened on, but additional
    // ExternServer objects can be created to serve on more sockets.
    //
    casycom_register (&f_ExternServer);
    casycom_register (&f_Ping);
    app->externp = casycom_create_proxy (&i_ExternServer, oid_App);
    // 
    // For flexibility it is recommended to implement systemd socket activation.
    // Doing so is very simple; sd_listen_fds returns the number of fds passed to
    // the process by systemd. The fds start at SD_LISTEN_FDS_START. To keep the
    // example simple, only the first one is used; create more ExternServer objects
    // to listen on more than one socket.
    //
    if (sd_listen_fds())
	PExternServer_open (&app->externp, SD_LISTEN_FDS_START+0, eil_Ping, true);
    else {	// If no sockets were passed from systemd, create one manually
	// Use bind_user_local to create the socket in XDG_RUNTIME_DIR,
	// the standard location for sockets of user services.
	// See other bind variants in xsrv.h.
	if (0 > PExternServer_bind_user_local (&app->externp, c_IPCOM_socket_name, eil_Ping))
	    casycom_error ("ExternServer_bind_user_local: %s", strerror(errno));
    }
}

static void App_pipe (App* app)
{
    // When a private object server is needed, it can be launched with
    // PExtern_LaunchPipe and will communicate on only one connection,
    // the socket on stdin. In this case, ExternServer is not needed
    // and only one Extern object is created.
    casycom_register (&f_Ping);
    app->externp = casycom_create_proxy (&i_Extern, oid_App);
    PExtern_open (&app->externp, STDIN_FILENO, EXTERN_SERVER, NULL, eil_Ping);
}

// When a client connects, ExternServer will forward the ExternR_connected message
// here. This is the appropriate place to check that all the imports are satisfied,
// authenticate the connection (if using a UNIX socket), and create objects.
//
static void App_ExternR_connected (App* app, const ExternInfo* einfo)
{
    // Both directly connected Extern objects and ExternServer objects send this reply.
    if (!app->server_pid)
	return;	// log only the client side
    // ExternInfo contains the list of interfaces available on this connection, so
    // here is the right place to check that the expected interfaces can be created.
    if (einfo->interfaces.size < 1 || einfo->interfaces.d[0] != &i_Ping) {
	casycom_error ("Connected to server that does not support the Ping interface");
	return;
    }
    LOG ("Connected to server. Imported %zu interface: %s\n", einfo->interfaces.size, einfo->interfaces.d[0]->name);
    // ExternInfo can be obtained outside this function with casycom_extern_info
    // using the Extern object oid, or with casycom_extern_object_info, using the
    // oid of the COMRelay connecting an object to an Extern object (the COMRelay
    // sends the messages originating from a remote client, so the served object
    // constructor receives a message from it). The ExternInfo also specifies
    // whether the connection is a UNIX socket (allowing passing fds and credentials),
    // and client process credentials, if available. This information may be
    // useful for choosing file transmission mode, or for authentication.
    //
    // To create a remote object, create a proxy object for it
    // (This is exactly the same as accessing a local object)
    app->pingp = casycom_create_proxy (&i_Ping, oid_App);
    // ... which can then be accessed through the proxy methods.
    PPing_ping (&app->pingp, 1);
}

// The ping replies are received exactly the same way as from a local
// object. But now the object resides in the server process and the
// message is read from the socket.
static void App_PingR_ping (App* app, uint32_t u)
{
    LOG ("Ping %u reply received in app; count %u\n", u, ++app->npings);
    casycom_quit (EXIT_SUCCESS);
}

static const DApp d_App_App = {
    .interface = &i_App,
    DMETHOD (App, App_init)
};
static const DPingR d_App_PingR = {
    .interface = &i_PingR,
    DMETHOD (App, PingR_ping)
};
static const DExternR d_App_ExternR = {
    .interface = &i_ExternR,
    DMETHOD (App, ExternR_connected)
};
static const Factory f_App = {
    .create	= App_create,
    .destroy	= App_destroy,
    .dtable	= { &d_App_App, &d_App_PingR, &d_App_ExternR, NULL }
};
CASYCOM_MAIN (f_App)
