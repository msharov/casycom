// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "ping.h"
#include <sys/wait.h>
#include <signal.h>

//----------------------------------------------------------------------
// In an application that already uses an event-driven framework and has
// a message loop, casycom can be used in non-framework mode. While it is
// possible to just run casycom_main in a separate thread (message sending
// is thread safe), enabling threading can have disastrous consequences
// for code not designed to handle it. In this example, casycom is used
// one message loop pass at a time, integrating into a different toplevel
// message loop.
//
// A casycom object is still needed to serve as a destination for reply
// messages. It will, however, not be created as an App object, and need
// not be unique.

typedef struct _PingCaller {
    Proxy	pingp;
    unsigned	pingCount;
    Proxy	externp;
    pid_t	serverPid;
} PingCaller;

//----------------------------------------------------------------------
// Connecting to a server is done as shown in ipcom. In this example,
// only the PingCaller_ForkAndPipe method is implemented, and both sides
// use the non-framework operation.
//
static void PingCaller_ForkAndPipe (PingCaller* o);

// List of imported/exported interfaces
static const iid_t eil_Ping[] = { &i_Ping, NULL };

//----------------------------------------------------------------------
// The PingCaller object will serve as the starting point for the outgoing
// Ping and as the recipient of the PingR reply. On the server side, it
// receives the ExternR_Connected message and does nothing else.

static void* PingCaller_Create (const Msg* msg)
{
    PingCaller* o = xalloc (sizeof(PingCaller));
    //
    // Both sides will need an Extern object. PingCaller oid is the
    // destination of the (dummy) message creating this object.
    //
    o->externp = casycom_create_proxy (&i_Extern, msg->h.dest);
    //
    // The constructor initiates what the App did in ipcom: spawn
    // the server and create an Extern connection. Then, on the client
    // side, the Ping request is sent.
    //
    PingCaller_ForkAndPipe (o);
    return o;
}

static void PingCaller_ForkAndPipe (PingCaller* o)
{
    // This is the same code as in ipcom
    int socks[2];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, 0, socks))
	return casycom_error ("socketpair: %s", strerror(errno));
    int fr = fork();
    if (fr < 0)
	return casycom_error ("fork: %s", strerror(errno));
    if (fr == 0) {	// Server side
	close (socks[0]);
	casycom_register (&f_Ping);	// This is the exported interface
	PExtern_Open (&o->externp, socks[1], EXTERN_SERVER, NULL, eil_Ping);
    } else {		// Client side
	o->serverPid = fr;
	close (socks[1]);
	PExtern_Open (&o->externp, socks[0], EXTERN_CLIENT, eil_Ping, NULL);
    }
}

static void PingCaller_ExternR_Connected (PingCaller* o)
{
    if (!o->serverPid)
	return;	// the server side will just listen
    LOG ("Connected to server.\n");
    o->pingp = casycom_create_proxy (&i_Ping, o->externp.src);
    // ... which can then be accessed through the proxy methods.
    PPing_Ping (&o->pingp, 1);
}

static void PingCaller_PingR_Ping (PingCaller* o, uint32_t u)
{
    LOG ("Ping %u reply received; count %u\n", u, ++o->pingCount);
    if (o->pingCount < 5)
	PPing_Ping (&o->pingp, o->pingCount);
    else
	casycom_quit (EXIT_SUCCESS);
}

static const DPingR d_PingCaller_PingR = {
    .interface = &i_PingR,
    DMETHOD (PingCaller, PingR_Ping)
};
static const DExternR d_PingCaller_ExternR = {
    .interface = &i_ExternR,
    DMETHOD (PingCaller, ExternR_Connected)
};
static const Factory f_PingCaller = {
    .Create	= PingCaller_Create,
    .dtable	= { &d_PingCaller_PingR, &d_PingCaller_ExternR, NULL }
};

//----------------------------------------------------------------------

static void OnChildSignal (int signo UNUSED) { waitpid (-1, NULL, 0); }

//----------------------------------------------------------------------

int main (void)
{
    // casycom_init differs from casycom_framework_init in not requiring
    // an app object. It also does not register signal handlers. It does,
    // however, register cleanup handlers for casycom and any objects it
    // will create and own.
    //
    casycom_init();
    casycom_enable_externs();
    //
    // Without casycom's signal handlers, need to catch SIGCHLD
    // to cleanly handle the server side's exit.
    //
    signal (SIGCHLD, OnChildSignal);
    //
    // Because there is no App, no casycom objects exist initially.
    //
    // An object can be created by sending a message to it. Proxies
    // without a source objects should pass oid_Broadcast as the source
    // id to casycom_create_proxy
    //
    // Objects can also be created explicitly, by interface. In this
    // example, PingCaller implements the PingR interface, and so it
    // can be created for it. One advantage of this method is that
    // the pointer to the object is obtained. The pointer is still
    // managed by casycom and must not be deleted or accessed after
    // it is marked unused (which is the usual way of requesting
    // casycom object deletion).
    //
    casycom_register (&f_PingCaller);
    // PingCaller* pco = (PingCaller*)
    casycom_create_object (&i_PingR);
    //
    // A top-level message loop will wait for some exit condition.
    // For example, an Xlib event loop will wait for the window to
    // close. When using casycom, the loop will also need to check
    // for casycom quitting condition. This is necessary for error
    // handling to work, in which case it would indicate the destruction
    // of the above created PingCaller object.
    //
    while (!casycom_is_quitting()) {	// Add other loop conditions here
	// Add top-level message processing here. For example,
	// XNextEvent might go here or in the while condition.
	//
	// The message loop's main purpose is to process messages
	// casycom_loop_once will process all messages in the output
	// queue, and then will swap the queues. It also checks for
	// watched fds and notifies appropriate objects by message,
	// but never waits for fd activity. It returns false when no
	// messages exist in any queue, a condition indicating that
	// the loop should wait for fds to produce something useful.
	//
	bool haveMessages = casycom_loop_once();
	//
	// Once message processing is done for this round, the
	// appropriate thing to do is to wait on file descriptors.
	// Putting the process to sleep is a good thing for prolonging
	// the life of the computer. casycom's Timer object normally
	// takes care of this, and if no other file descriptors need
	// watching, the default poll call can be used as:
	//     Timer_RunTimer (haveMessages ? 0 : -1)
	// waiting indefinitely when no messages are present.
	//
	// If other fds are being polled, such as the Xlib connection
	// to the X server, a pollfd list will have to be built here.
	// Timer_WatchListSize provides an estimate of how many fds
	// are being watched. (An estimate because some of them are
	// just timers) That number is a good starting point for
	// sizing the fds array.
	//
	size_t nFds = Timer_WatchListSize();
	//
	// On the server side, when the client connection is terminated
	// the Extern object is destroyed. When there are no messages
	// and no fds being watched, the server should quit. This is
	// done by casycom_main, but in non-framework mode this
	// condition is not necessarily a reason to quit. In this example
	// it is, so add a check for it here.
	//
	if (!nFds) {
	    if (!haveMessages)
		casycom_quit (EXIT_SUCCESS);
	} else {
	    struct pollfd fds [nFds];
	    //
	    // After, or before, filling in the non-casycom fds, casycom
	    // watched fds can be added. The return value is the number
	    // of fds actually added. A pointer to the timeout variable
	    // can be provided to support pure timers.
	    //
	    int timeout = 0;
	    nFds = Timer_WatchListForPoll (fds, nFds, (haveMessages || casycom_is_quitting()) ? NULL : &timeout);
	    //
	    // Now poll can be called to wait on the results
	    //
	    poll (fds, nFds, timeout);
	    //
	    // Here the results can be checked for non-casycom fds.
	    // Timer will check casycom fds in casycom_loop_once.
	    // Nothing else to do, so go to the next iteration.
	}
    }
    return casycom_exit_code();
}
