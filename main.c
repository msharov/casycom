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
    PProxy		h;
    uint32_t		flags;
} SMsgLink;
DECLARE_VECTOR_TYPE (SOMap, SMsgLink);
static VECTOR (SOMap, _casycom_OMap);

//----------------------------------------------------------------------

static void casycom_idle (void);
static size_t casycom_omap_lower_bound (oid_t oid);
static void casycom_create_link (const PProxy* ph, size_t ip);
static void casycom_destroy_link_at (size_t l);
static size_t casycom_link_for_proxy (const PProxy* ph);
static SMsgLink* casycom_find_destination (oid_t doid);
static const SObject* casycom_find_otable (iid_t iid);
static const DTable* casycom_find_dtable (const SObject* o, iid_t iid);
static void casycom_create_link_object (SMsgLink* ml, const SMsg* msg);
static void casycom_destroy_object (SMsgLink* ol);
static void casycom_do_message_queues (void);
static SMsgLink* casycom_link_for_object (const void* o);

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
    while (_casycom_OMap.size)
	casycom_destroy_link_at (_casycom_OMap.size-1);
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
	    casycom_destroy_link_at (i);
	    i = (size_t)-1;	// start over because casycom_destroy_link_at modifies OMap
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
    SMsgLink* ml = casycom_find_destination (oid);
    if (!ml)
	return false;
    if (ml->fn->Error && ml->fn->Error (ml->o, eoid, _casycom_Error)) {
	xfree (_casycom_Error);
	return true;
    }
    // If not, fail this object and forward to creator
    if (ml->h.src == eoid)
	return false;	// unless it already failed
    return casycom_forward_error (ml->h.src, oid);
}

//----------------------------------------------------------------------
// Proxies and link table

PProxy casycom_create_proxy (iid_t iid, oid_t src)
{
    // Find first unused oid value
    // OMap is sorted, so increment nid until a number gap is found
    oid_t nid = oid_First;
    if (_casycom_OMap.size)
	nid = _casycom_OMap.d[0].h.dest;
    size_t ip = 0;
    for (; ip < _casycom_OMap.size; ++ip) {
	if (nid < _casycom_OMap.d[ip].h.dest)
	    break;
	if (nid == _casycom_OMap.d[ip].h.dest)
	    ++nid;
    }
    PProxy r = casycom_create_proxy_to (iid, src, nid);
    casycom_create_link (&r, ip);
    return r;
}

static void casycom_destroy_link_at (size_t l)
{
    if (l >= _casycom_OMap.size)
	return;
    SMsgLink ol = _casycom_OMap.d[l];
    vector_erase (&_casycom_OMap, l);
    if (ol.o)	// If this is the link that created the object, destroy the object
	casycom_destroy_object (&ol);
}

void casycom_destroy_proxy (PProxy* pp)
{
    casycom_destroy_link_at (casycom_link_for_proxy (pp));
    pp->interface = NULL;
    pp->src = 0;
    pp->dest = 0;
}

static size_t casycom_omap_lower_bound (oid_t oid)
{
    size_t first = 0, last = _casycom_OMap.size;
    while (first < last) {
	size_t mid = (first + last) / 2;
	if (_casycom_OMap.d[mid].h.dest < oid)
	    first = mid + 1;
	else
	    last = mid;
    }
    return first;
}

static void casycom_create_link (const PProxy* ph, size_t ip)
{
    SMsgLink* e = (SMsgLink*) vector_emplace (&_casycom_OMap, ip);
    e->h = *ph;
    e->fn = casycom_find_otable (ph->interface);
}

static size_t casycom_link_for_proxy (const PProxy* ph)
{
    for (size_t l = casycom_omap_lower_bound (ph->dest);
		l < _casycom_OMap.size && _casycom_OMap.d[l].h.dest == ph->dest;
		++l)
	if (_casycom_OMap.d[l].h.src == ph->src)
	    return l;
    return SIZE_MAX;
}

static SMsgLink* casycom_link_for_object (const void* o)
{
    for (size_t dp = 0; dp < _casycom_OMap.size; ++dp)
	if (_casycom_OMap.d[dp].o == o)
	    return &_casycom_OMap.d[dp];
    return NULL;
}

static SMsgLink* casycom_find_destination (oid_t doid)
{
    size_t dp = casycom_omap_lower_bound (doid);
    if (dp < _casycom_OMap.size && _casycom_OMap.d[dp].h.dest == doid)
	return &_casycom_OMap.d[dp];
    return NULL;
}

//----------------------------------------------------------------------
// Object registration and creation

void casycom_register (const SObject* o)
{
    #ifndef NDEBUG
	assert (o->Create && "registered object must have a constructor");
	assert (o->interface[0] && "an object must implement at least one interface");
	for (const DTable* const* oi = (const DTable* const*) o->interface; *oi; ++oi)
	    assert ((*oi)->interface && "each dtable must contain the pointer to the implemented interface");
    #endif
    vector_push_back (&_casycom_ObjectTable, &o);
}

oid_t casycom_oid_of_object (const void* o)
{
    SMsgLink* ml = casycom_link_for_object (o);
    return ml ? ml->h.dest : oid_Broadcast;
}

void casycom_mark_unused (const void* o)
{
    SMsgLink* ml = casycom_link_for_object (o);
    if (ml)
	ml->flags |= (1<<f_Unused);
}

static void casycom_create_link_object (SMsgLink* ml, const SMsg* msg)
{
    assert (!ml->o && "internal error: object already exists");
    // Create using the otable
    ml->o = ml->fn->Create (msg);
    assert (ml->o && "object Create method must return a valid object or die");
}

static void casycom_destroy_object (SMsgLink* ol)
{
    // Call the destructor, if set.
    if (ol->fn->Destroy) {
	ol->fn->Destroy (ol->o);
	ol->o = NULL;
    } else
	xfree (ol->o);	// Otherwise just free
    // Erase all links to this object
    for (size_t di = 0; di < _casycom_OMap.size; ++di) {
	if (_casycom_OMap.d[di].h.dest == ol->h.dest) {		// Object calls the destroyed object
	    assert (!_casycom_OMap.d[di].o && "internal error: object pointer present on multiple links");
	    const SMsgLink* ml = casycom_find_destination (_casycom_OMap.d[di].h.src);
	    if (ml && ml->fn->ObjectDestroyed)			// notify of destruction, if requested
		ml->fn->ObjectDestroyed (ml->o, ol->h.dest);
	} else if (_casycom_OMap.d[di].h.src != ol->h.dest)
	    continue;
	casycom_destroy_link_at (di);
	di = (size_t)-1;	// recursion will modify OMap, so have to start over
    }
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

//----------------------------------------------------------------------
// Message queue management

void casycom_queue_message (SMsg* msg)
{
    #ifndef NDEBUG	// Message validity checks
	assert (msg && msg->h.interface && (!msg->size || msg->body) && "invalid message");
	assert (casycom_find_otable (msg->h.interface) && "message addressed to unregistered interface");
	assert (casycom_find_destination (msg->h.dest) && "message addressed to an unknown destination");
	assert (casycom_link_for_proxy(&msg->h) < _casycom_OMap.size && "message sent through a deleted proxy; do not delete proxies in the destructor or in ObjectDeleted!");
	assert (msg->imethod < casyiface_count_methods (msg->h.interface) && "invalid message destination method");
	RStm is = casymsg_read(msg);
	assert (msg->size == casymsg_validate_signature (casymsg_signature(msg), &is) && "message data does not match method signature");
    #endif
    acquire_lock (&_casycom_OutputQueueLock);
    vector_push_back (&_casycom_OutputQueue, &msg);
    release_lock (&_casycom_OutputQueueLock);
}

static void casycom_do_message_queues (void)
{
    // Deliver all messages in the input queue
    for (size_t m = 0; m < _casycom_InputQueue.size; ++m) {
	const SMsg* msg = _casycom_InputQueue.d[m];
	SMsgLink* ml = casycom_find_destination (msg->h.dest);
	if (!ml)	// message addressed to object deleted after sending
	    continue;
	assert (casycom_link_for_proxy(&msg->h) < _casycom_OMap.size && "message sent through a deleted proxy; do not delete proxies in the destructor or in ObjectDeleted!");
	const DTable* dtable = casycom_find_dtable (ml->fn, msg->h.interface);
	assert (dtable && "internal error: message queued without checking");
	// Create the object, if needed
	if (!ml->o)
	    casycom_create_link_object (ml, msg);
	// Call the interface dispatch explicitly listed by the object
	((pfn_dispatch) msg->h.interface->dispatch) (dtable, ml->o, msg);
	// After each message, check for generated errors
	if (_casycom_Error && !casycom_forward_error (ml->h.dest, ml->h.dest)) {
	    // If nobody can handle the error, print it and quit
	    casycom_log (LOG_ERR, "Error: %s\n", _casycom_Error);
	    casycom_quit (EXIT_FAILURE);
	    break;
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
