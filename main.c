// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "timer.h"
#include <signal.h>
#include <stdarg.h>

//{{{ Module globals ---------------------------------------------------

/// Last non-fatal signal. Set by signal handler, read by main loop.
static int _casycom_LastSignal = 0;
/// Loop exit code
static int _casycom_ExitCode = EXIT_SUCCESS;
/// Loop status
static bool _casycom_Quitting = false;
/// Output queue modification lock
static bool _casycom_OutputQueueLock = false;
/// Last error
static char* _casycom_Error = NULL;
/// Message queues
DECLARE_VECTOR_TYPE (SMsgVector, SMsg*);
static VECTOR (SMsgVector, _casycom_InputQueue);
static VECTOR (SMsgVector, _casycom_OutputQueue);
/// Creatable object table
DECLARE_VECTOR_TYPE (SObjectTable, const SObject*);
static VECTOR (SObjectTable, _casycom_ObjectTable);
/// Message link map
typedef enum _OFlags {
    f_Unused,
    f_Last
} OFlags;
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
static const DTable* casycom_find_dtable (const SObject* o, iid_t iid);
static const SMsgLink* casycom_create_object (const SMsg* msg, size_t ip);
static void casycom_destroy_object (SMsgLink* ol);
static const SMsgLink* casycom_find_destination (const SMsg* msg);
static void casycom_do_message_queues (void);
static SMsgLink* casycom_link_for_object (const void* o);
static SMsgLink* casycom_link_for_oid (oid_t oid);

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
    atexit (casycom_reset);
}

/// Cleans up allocated objects
void casycom_reset (void)
{
    for (size_t i = 0; i < _casycom_OMap.size; ++i)
	casycom_destroy_object (&_casycom_OMap.d[i]);
    vector_deallocate (&_casycom_OMap);
    acquire_lock (&_casycom_OutputQueueLock);
    vector_deallocate (&_casycom_OutputQueue);
    release_lock (&_casycom_OutputQueueLock);
    vector_deallocate (&_casycom_InputQueue);
    vector_deallocate (&_casycom_ObjectTable);
    xfree (_casycom_Error);
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
    // Destroy objects marked unused
    for (size_t i = 0; i < _casycom_OMap.size; ++i) {
	if (_casycom_OMap.d[i].flags & (1<<f_Unused)) {
	    casycom_destroy_object (&_casycom_OMap.d[i]);
	    vector_erase (&_casycom_OMap, i);
	    i = 0;	// casycom_destroy_object may set unused flags
	}
    }
    // Process timers and fd waits
    int timerWait = -1;
    if (_casycom_InputQueue.size + _casycom_OutputQueue.size)
	timerWait = 0;	// Do not wait if there are packets in the queue
    bool haveTimers = Timer_RunTimer (timerWait);
    // Quit when there are no more packets or timers
    if (!haveTimers && !(_casycom_InputQueue.size + _casycom_OutputQueue.size))
	casycom_quit (EXIT_SUCCESS);
}

PProxy casycom_create_proxy (iid_t iid, oid_t src)
{
    // Find first unused oid value
    // OMap is sorted, so increment nid until a number gap is found
    oid_t nid = oid_First;
    if (_casycom_OMap.size)
	nid = _casycom_OMap.d[0].oid;
    for (size_t i = 0; i < _casycom_OMap.size; ++i) {
	if (nid < _casycom_OMap.d[i].oid)
	    break;
	if (nid == _casycom_OMap.d[i].oid)
	    ++nid;
    }
    return casycom_create_proxy_to (iid, src, nid);
}

void casycom_error (const char* fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    char* e = NULL;
    // If error printing failed, use default error message
    if (0 > vasprintf (&e, fmt, args))
	e = strdup ("unknown error");
    va_end (args);
    if (!_casycom_Error)	// First error message; move
	_casycom_Error = e;
    else {
	char* ne = NULL;	// Subsequent messages are appended
	if (0 <= asprintf (&ne, "%s\n\t%s", _casycom_Error, e)) {
	    xfree (_casycom_Error);
	    _casycom_Error = ne;
	    xfree (e);
	}
    }
}

bool casycom_forward_error (oid_t oid, oid_t eoid)
{
    assert (_casycom_Error && "you must first set the error with casycom_error");
    // See if the object can handle the error
    SMsgLink* ml = casycom_link_for_oid (oid);
    if (!ml)
	return false;
    if (ml->fn->Error && ml->fn->Error (eoid, _casycom_Error)) {
	xfree (_casycom_Error);
	return true;
    }
    // If not, fail this object and forward to creator
    if (ml->creator == eoid)
	return false;	// unless it already failed
    return casycom_forward_error (ml->creator, oid);
}

//----------------------------------------------------------------------
// Object registration and creation

void casycom_register (const SObject* o)
{
    #ifndef NDEBUG
	assert (o->interface[0] && "an object must implement at least one interface");
	for (const DTable* const* oi = (const DTable* const*) o->interface; *oi; ++oi)
	    assert ((*oi)->interface && "each dtable must contain the pointer to the implemented interface");
    #endif
    vector_push_back (&_casycom_ObjectTable, &o);
}

static const DTable* casycom_find_dtable (const SObject* o, iid_t iid)
{
    for (const DTable* const* oi = (const DTable* const*) o->interface; *oi; ++oi)
	if ((*oi)->interface == iid)
	    return *oi;
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
    e->oid = msg->dest;
    e->creator = msg->src;
    // Create using the otable
    e->fn = casycom_find_otable (msg->interface);
    if (!e->fn) {
	casycom_log (LOG_ERR, "message sent to unregistered interface %s", msg->interface->name);
	exit (EXIT_FAILURE);
    }
    e->o = e->fn->Create (msg);
    assert (e->o && "Object Create method must return a valid object or die");
    return e;
}

static void casycom_destroy_object (SMsgLink* ol)
{
    // Notify linked objects of the destruction
    for (size_t i = 0; i < _casycom_OMap.size; ++i) {
	SMsgLink* ml = &_casycom_OMap.d[i];
	if (ml->creator == ol->oid || ml->oid == ol->creator) {
	    if (ml->creator == ol->oid) // Mark unused all objects created by this one
		ml->flags |= (1<<f_Unused);
	    if (ml->fn->ObjectDestroyed)
		ml->fn->ObjectDestroyed (ol->oid);
	}
    }
    // Call the destructor, if set.
    if (ol->fn->Destroy) {
	ol->fn->Destroy (ol->o);
	ol->o = NULL;
    } else
	xfree (ol->o);	// Otherwise just free
}

//----------------------------------------------------------------------
// Message queue management

void casycom_queue_message (SMsg* msg)
{
    acquire_lock (&_casycom_OutputQueueLock);
    vector_push_back (&_casycom_OutputQueue, &msg);
    release_lock (&_casycom_OutputQueueLock);
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
	const DTable* dtable = casycom_find_dtable (ml->fn, msg->interface);
	if (!dtable) {
	    assert (!"Destination object does not support message interface");
	    continue;
	}
	pfn_interface_dispatch dispatch = (pfn_interface_dispatch) msg->interface->dispatch;
	dispatch (dtable, ml->o, msg);
	if (_casycom_Error) {
	    if (!casycom_forward_error (ml->oid, ml->oid)) {
		// If nobody can handle the error, print it and quit
		casycom_log (LOG_ERR, "Error: %s\n", _casycom_Error);
		casycom_quit (EXIT_FAILURE);
		break;
	    }
	}
    }
    // Clear input queue
    for (size_t m = 0; m < _casycom_InputQueue.size; ++m)
	casymsg_free (_casycom_InputQueue.d[m]);
    vector_clear (&_casycom_InputQueue);
    // And make the output queue the input queue for the next round
    acquire_lock (&_casycom_OutputQueueLock);
    vector_swap (&_casycom_InputQueue, &_casycom_OutputQueue);
    release_lock (&_casycom_OutputQueueLock);
}

static SMsgLink* casycom_link_for_oid (oid_t oid)
{
    for (size_t dp = 0; dp < _casycom_OMap.size; ++dp)
	if (_casycom_OMap.d[dp].oid == oid)
	    return &_casycom_OMap.d[dp];
    return NULL;
}

static SMsgLink* casycom_link_for_object (const void* o)
{
    for (size_t dp = 0; dp < _casycom_OMap.size; ++dp)
	if (_casycom_OMap.d[dp].o == o)
	    return &_casycom_OMap.d[dp];
    return NULL;
}

void casycom_mark_unused (const void* o)
{
    SMsgLink* ml = casycom_link_for_object (o);
    if (ml)
	ml->flags |= (1<<f_Unused);
}

oid_t casycom_oid_of_object (const void* o)
{
    SMsgLink* ml = casycom_link_for_object (o);
    return ml ? ml->oid : oid_Broadcast;
}
