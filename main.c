// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "main.h"
#include <signal.h>

//{{{ Module globals ---------------------------------------------------

/// Last non-fatal signal. Set by signal handler, read by main loop.
static int _casycom_LastSignal = 0;
/// Loop exit code
static int _casycom_ExitCode = EXIT_SUCCESS;
/// Loop status
static bool _casycom_Quitting = false;
/// Message queues
DECLARE_VECTOR_TYPE (SMsgVector, SMsg*);
static VECTOR (SMsgVector, _casycom_InputQueue);
static VECTOR (SMsgVector, _casycom_OutputQueue);
/// Creatable object table
DECLARE_VECTOR_TYPE (SObjectTable, const SObject*);
static VECTOR (SObjectTable, _casycom_ObjectTable);
/// Message link map
typedef struct _SMsgLink {
    void*		o;
    const SObject*	fn;
    oid_t		oid;
    oid_t		creator;
    uint32_t		flags;
} SMsgLink;
DECLARE_VECTOR_TYPE (SOMap, SMsgLink);
static VECTOR (SOMap, _casycom_OMap);

//----------------------------------------------------------------------

static void casycom_idle (void);
static const SObject* casycom_find_otable (iid_t iid);
static const void* casycom_find_dtable (const SObject* o, iid_t iid);
static const SMsgLink* casycom_create_object (const SMsg* msg, size_t ip);
static const SMsgLink* casycom_find_destination (const SMsg* msg);
static void casycom_do_message_queues (void);

//}}}-------------------------------------------------------------------
//{{{ Signal handling

#define S(s) (1<<(s))
enum {
    sigset_Quit	= S(SIGINT)|S(SIGQUIT)|S(SIGTERM)|S(SIGPWR),
    sigset_Die	= S(SIGILL)|S(SIGABRT)|S(SIGBUS)|S(SIGFPE)|S(SIGSYS)|S(SIGSEGV)|S(SIGALRM)|S(SIGXCPU),
    sigset_Msg	= sigset_Quit|S(SIGHUP)|S(SIGCHLD)|S(SIGWINCH)|S(SIGURG)|S(SIGXFSZ)|S(SIGUSR1)|S(SIGUSR2)|S(SIGPIPE)
};

static void casycom_on_fatal_signal (int sig)
{
    enum { qc_ShellSignalQuitOffset = 128 };
    static volatile _Atomic(bool) doubleSignal = false;
    if (false == atomic_exchange (&doubleSignal, true)) {
	casycom_log (LOG_CRIT, "[S] Error: %s\n", strsignal(sig));
	#ifndef NDEBUG
	    casycom_backtrace();
	#endif
	exit (qc_ShellSignalQuitOffset+sig);
    }
    _Exit (qc_ShellSignalQuitOffset+sig);
}

static void casycom_on_msg_signal (int sig)
{
    _casycom_LastSignal = sig;
    alarm (1);	// Reset in main loop after clearing _casycom_LastSignal. Ensures that hangs can be broken.
}

static void casycom_install_signal_handlers (void)
{
    for (unsigned sig = 0; sig < sizeof(int)*8; ++sig) {
	if (sigset_Msg & S(sig))
	    signal (sig, casycom_on_msg_signal);
	else if (sigset_Die & S(sig))
	    signal (sig, casycom_on_fatal_signal);
    }
}
#undef S

//}}}-------------------------------------------------------------------
//--- Main API

/// Initializes the library and the event loop
void casycom_init (void)
{
}

/// Replaces casycom_init if casycom is the top-level framework in your process
void casycom_framework_init (void)
{
    casycom_install_signal_handlers();
    casycom_init();
}

/// Sets the quit flag, causing the event loop to quit once all queued events are processed
void casycom_quit (int exitCode)
{
    _casycom_ExitCode = exitCode;
    _casycom_Quitting = true;
}

/// The main event loop. Returns exit code.
int casycom_main (void)
{
    for (_casycom_Quitting = false; !_casycom_Quitting; casycom_idle())
	casycom_do_message_queues();
    return _casycom_ExitCode;
}

static void casycom_idle (void)
{
    if (!_casycom_InputQueue.size && !_casycom_OutputQueue.size)
	casycom_quit (EXIT_SUCCESS);	// No more packets, so quit
}

PProxy casycom_create_proxy (iid_t iid, oid_t src)
{
    static oid_t _lastOid = oid_Broadcast;
    return (PProxy) { iid, src, ++_lastOid };
}

//----------------------------------------------------------------------
// Object registration and creation

void casycom_register (const SObject* o)
{
    vector_push_back (&_casycom_ObjectTable, &o);
}

static const void* casycom_find_dtable (const SObject* o, iid_t iid)
{
    assert (o->interface[0].iid && "an object must implement at least one interface");
    for (const SObjectInterface* oi = o->interface; oi->iid; ++oi)
	if (oi->iid == iid)
	    return oi->dtable;
    return NULL;
}

static const SObject* casycom_find_otable (iid_t iid)
{
    for (size_t i = 0; i < _casycom_ObjectTable.size; ++i) {
	const SObject* ot = _casycom_ObjectTable.d[i];
	if (casycom_find_dtable (ot, iid))
	    return ot;
    }
    return NULL;
}

static const SMsgLink* casycom_create_object (const SMsg* msg, size_t ip)
{
    SMsgLink* e = (SMsgLink*) vector_insert_empty (&_casycom_OMap, ip);
    // Create using the otable
    e->fn = casycom_find_otable (msg->interface);
    if (!e->fn) {
	casycom_log (LOG_ERR, "message sent to unregistered interface %s", msg->interface->name);
	exit (EXIT_FAILURE);
    }
    e->o = e->fn->Create (msg);
    assert (e->o && "Object Create method must return a valid object or die");
    e->oid = msg->dest;
    e->creator = msg->src;
    e->flags = 0;
    return e;
}

//----------------------------------------------------------------------
// Message queue management

void casycom_queue_message (SMsg* msg)
{
    vector_push_back (&_casycom_OutputQueue, &msg);
}

static const SMsgLink* casycom_find_destination (const SMsg* msg)
{
    size_t dp = 0;
    for (; dp < _casycom_OMap.size; ++dp) {
	if (_casycom_OMap.d[dp].oid == msg->dest)
	    return &_casycom_OMap.d[dp];
	else if (_casycom_OMap.d[dp].oid > msg->dest)
	    break;
    }
    return casycom_create_object (msg, dp);
}

static void casycom_do_message_queues (void)
{
    for (size_t m = 0; m < _casycom_InputQueue.size; ++m) {
	const SMsg* msg = _casycom_InputQueue.d[m];
	const SMsgLink* ml = casycom_find_destination (msg);
	const void* dtable = casycom_find_dtable (ml->fn, msg->interface);
	if (!dtable) {
	    assert (!"Destination object does not support message interface");
	    continue;
	}
	pfn_interface_dispatch dispatch = (pfn_interface_dispatch) msg->interface->dispatch;
	dispatch (dtable, ml->o, msg);
    }
    // Clear input queue
    for (size_t m = 0; m < _casycom_InputQueue.size; ++m)
	casymsg_free (_casycom_InputQueue.d[m]);
    vector_clear (&_casycom_InputQueue);
    // And make the output queue the input queue for the next round
    vector_swap (&_casycom_InputQueue, &_casycom_OutputQueue);
}
