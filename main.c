// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "app.h"
#include "timer.h"
#include "vector.h"
#include <signal.h>
#include <stdarg.h>
#include <sys/wait.h>

//{{{ Module globals ---------------------------------------------------

// Last non-fatal signal. Set by signal handler, read by main loop.
static unsigned _casycom_last_signal = 0;
static pid_t _casycom_last_child_pid = 0;
static int _casycom_last_child_status = 0;
// Loop exit code
static int _casycom_exit_code = EXIT_SUCCESS;
// Loop status
static bool _casycom_quitting = false;
// Output queue modification lock
static _Atomic(bool) _casycom_output_queue_lock = false;
// Last error
static char* _casycom_error = NULL;
// App proxy
static Proxy _casycom_appp = {};
// Enables debug trace messages
bool casycom_debug_msg_trace = false;

// Message queues
DECLARE_VECTOR_TYPE (MsgVector, Msg*);
static VECTOR (MsgVector, _casycom_input_queue);	// During each main loop iteration, this queue is read
static VECTOR (MsgVector, _casycom_output_queue);	// ... and this queue is written. Then they are swapped.

// Object table contains object factories and the dtables they support
DECLARE_VECTOR_TYPE (FactoryTable, const Factory*);
static VECTOR (FactoryTable, _casycom_object_table);
static const Factory* _casycom_default_object = NULL;

// Message link map
typedef enum _OFlags {
    f_Unused,	// Objects marked unused are deleted during loop idle
    f_Last
} OFlags;
typedef struct _MsgLink {
    void*		o;
    const Factory*	factory;
    Proxy		h;
    uint32_t		flags;
} MsgLink;
DECLARE_VECTOR_TYPE (SOMap, MsgLink);

// _casycom_omap contains the message routing table, mapping each
// proxy-to-object link. The map is sorted by object id. Each object
// may have multiple incoming and outgoing links. The block of incoming
// links is contiguous, being sorted, and the first link contains the
// object pointer (ml->o). The other links have o set to NULL, and are
// ordered by the order of creation of their proxies. The first
// link is created by the first proxy created to this object, and is
// considered to be the creator link. The creator path is used for error
// handling propagation. Once the creator object is destroyed, all
// objects created by it are also destroyed.
static VECTOR (SOMap, _casycom_omap);

//----------------------------------------------------------------------
// Local private functions

static MsgLink* casycom_find_destination (oid_t doid);
static MsgLink* casycom_find_or_create_destination (const Msg* msg);
static MsgLink* casycom_link_for_object (const void* o);
static const DTable* casycom_find_dtable (const Factory* o, iid_t iid);
static const Factory* casycom_find_factory (iid_t iid);
static size_t casycom_link_for_proxy (const Proxy* ph);
static size_t casycom_omap_lower_bound (oid_t oid);
static void* casycom_create_link_object (MsgLink* ml, const Msg* msg);
static void casycom_destroy_link_at (size_t l);
static void casycom_destroy_object (MsgLink* ol);
static void casycom_do_message_queues (void);
static void casycom_idle (void);

//}}}-------------------------------------------------------------------
//{{{ Signal handling

#define S(s) (1<<(s))
enum {
    sigset_Quit	= S(SIGINT)|S(SIGQUIT)|S(SIGTERM),
    sigset_Die	= S(SIGILL)|S(SIGABRT)|S(SIGBUS)|S(SIGFPE)|S(SIGSYS)|S(SIGSEGV)|S(SIGALRM)|S(SIGXCPU),
    sigset_Msg	= sigset_Quit|S(SIGHUP)|S(SIGCHLD)|S(SIGWINCH)|S(SIGURG)|S(SIGXFSZ)|S(SIGUSR1)|S(SIGUSR2)|S(SIGPIPE)
};
enum { qc_ShellSignalQuitOffset = 128 };

static void casycom_on_fatal_signal (int sig)
{
    static volatile _Atomic(bool) doubleSignal = false;
    if (false == atomic_exchange (&doubleSignal, true)) {
	alarm (1);
	casycom_log (LOG_CRIT, "[S] Error: %s\n", strsignal(sig));
	#ifndef NDEBUG
	    casycom_backtrace();
	#endif
	// The exit code should be success when terminated by user
	int exitcode = EXIT_SUCCESS;
	if (!(S(sig) & sigset_Quit))
	    exitcode = qc_ShellSignalQuitOffset+sig;
	exit (exitcode);
    }
    _Exit (qc_ShellSignalQuitOffset+sig);
}

static void casycom_on_msg_signal (int sig)
{
    DEBUG_PRINTF ("[S] Signal %d: %s\n", sig, strsignal(sig));
    #ifdef NDEBUG
	alarm (1);
    #endif
    _casycom_last_signal = sig;
    if (sig == SIGCHLD)
	_casycom_last_child_pid = waitpid (-1, &_casycom_last_child_status, WNOHANG);
    else if (S(sig) & sigset_Quit)
	casycom_quit (qc_ShellSignalQuitOffset+sig);
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

static inline void casycom_send_signal_message (void)
{
    if (!_casycom_last_signal)
	return;
    alarm (0);	// the alarm was set to check for infinite loops
    if (_casycom_appp.interface)
	PApp_signal (&_casycom_appp, _casycom_last_signal, _casycom_last_child_pid, _casycom_last_child_status);
    _casycom_last_signal = 0;
}

//}}}-------------------------------------------------------------------
//{{{ Proxies and link table

/// creates a proxy to a new object from object \p src, using interface \p iid
Proxy casycom_create_proxy (iid_t iid, oid_t src)
{
    // Find first unused oid value
    // OMap is sorted, so increment nid until a number gap is found
    oid_t nid = oid_First;
    if (_casycom_omap.size)
	nid = _casycom_omap.d[0].h.dest;
    size_t ip = 0;
    for (; ip < _casycom_omap.size; ++ip) {
	if (nid < _casycom_omap.d[ip].h.dest)
	    break;
	if (nid == _casycom_omap.d[ip].h.dest)
	    ++nid;
    }
    return casycom_create_proxy_to (iid, src, nid);
}

/// creates a proxy to existing object \p dest from \p src, using interface \p iid
Proxy casycom_create_proxy_to (iid_t iid, oid_t src, oid_t dest)
{
    size_t ip = casycom_omap_lower_bound (dest);
    while (ip < _casycom_omap.size && _casycom_omap.d[ip].h.dest == dest)
	++ip;
    MsgLink* e = vector_emplace (&_casycom_omap, ip);
    e->factory = casycom_find_factory (iid);
    e->h.interface = iid;
    e->h.src = src;
    e->h.dest = dest;
    DEBUG_PRINTF ("[T] created proxy link %hu -> %hu.%s\n", e->h.src, e->h.dest, e->h.interface->name);
    return e->h;
}

static void casycom_destroy_link_at (size_t l)
{
    if (l >= _casycom_omap.size)
	return;
    MsgLink ol = _casycom_omap.d[l];	// casycom_destroy_object may destroy other links, so l will be invalidated
    vector_erase (&_casycom_omap, l);
    DEBUG_PRINTF ("[T] destroyed proxy link %hu -> %hu.%s\n", ol.h.src, ol.h.dest, ol.h.interface->name);
    if (ol.o)	// If this is the link that created the object, destroy the object
	casycom_destroy_object (&ol);
}

void casycom_destroy_proxy (Proxy* pp)
{
    casycom_destroy_link_at (casycom_link_for_proxy (pp));
    pp->interface = NULL;
    pp->src = 0;
    pp->dest = 0;
}

static size_t casycom_omap_lower_bound (oid_t oid)
{
    size_t first = 0, last = _casycom_omap.size;
    while (first < last) {
	size_t mid = (first + last) / 2;
	if (_casycom_omap.d[mid].h.dest < oid)
	    first = mid + 1;
	else
	    last = mid;
    }
    return first;
}

static size_t casycom_link_for_proxy (const Proxy* ph)
{
    for (size_t l = casycom_omap_lower_bound (ph->dest);
		l < _casycom_omap.size && _casycom_omap.d[l].h.dest == ph->dest;
		++l)
	if (_casycom_omap.d[l].h.src == ph->src)
	    return l;
    return SIZE_MAX;
}

static MsgLink* casycom_link_for_object (const void* o)
{
    for (size_t dp = 0; dp < _casycom_omap.size; ++dp)
	if (_casycom_omap.d[dp].o == o)
	    return &_casycom_omap.d[dp];
    return NULL;
}

static MsgLink* casycom_find_destination (oid_t doid)
{
    size_t dp = casycom_omap_lower_bound (doid);
    if (dp < _casycom_omap.size && _casycom_omap.d[dp].h.dest == doid)
	return &_casycom_omap.d[dp];
    return NULL;
}

void casycom_debug_dump_link_table (void)
{
    DEBUG_PRINTF ("[D] Current link table:\n");
    for (size_t i = 0; i < _casycom_omap.size; ++i) {
	const MsgLink* UNUSED l = &_casycom_omap.d[i];
	DEBUG_PRINTF ("\t%hu -> %hu.%s\t(%p),%x\n", l->h.src, l->h.dest, l->h.interface->name, l->o, l->flags);
    }
}

//}}}-------------------------------------------------------------------
//{{{ Object registration and creation

#ifndef NDEBUG
static void casycom_debug_check_object (const Factory* o, const char* it)
{
    assert (o->create && "registered object must have a constructor");
    assert (o->dtable[0] && "an object must implement at least one interface");
    DEBUG_PRINTF ("[T] Registered %s", it);
    for (const DTable* const* oi = (const DTable* const*) o->dtable; *oi; ++oi) {
	assert ((*oi)->interface && "each dtable must contain the pointer to the implemented interface");
	DEBUG_PRINTF ("%c%s", oi == (const DTable* const*) o->dtable ? ' ':',', (*oi)->interface->name);
    }
    DEBUG_PRINTF("\n");
}
#endif

/// Registers an object class for creation
void casycom_register (const Factory* o)
{
    #ifndef NDEBUG
	casycom_debug_check_object (o, "class");
    #endif
    vector_push_back (&_casycom_object_table, &o);
}

/// Registers object class for unknown interfaces
void casycom_register_default (const Factory* o)
{
    #ifndef NDEBUG
	if (o)
	    casycom_debug_check_object (o, "default class");
	else if (_casycom_default_object)
	    DEBUG_PRINTF ("[T] Unregistered default class");
    #endif
    _casycom_default_object = o;
}

/// Finds object id of the given object
oid_t casycom_oid_of_object (const void* o)
{
    MsgLink* ml = casycom_link_for_object (o);
    return ml ? ml->h.dest : oid_Broadcast;
}

/// Marks the given object unused, to be deleted during the next idle
void casycom_mark_unused (const void* o)
{
    MsgLink* ml = casycom_link_for_object (o);
    if (ml)
	ml->flags |= (1<<f_Unused);
}

static void* casycom_create_link_object (MsgLink* ml, const Msg* msg)
{
    assert (!ml->o && "internal error: object already exists");
    // create using the otable
    DEBUG_PRINTF ("[T] Creating object %hu.%s\n", ml->h.dest, casymsg_interface_name(msg));
    void* o = ml->factory->create (msg);
    assert (o && "object create method must return a valid object or die");
    return o;
}

void* casycom_create_object (const iid_t iid)
{
    Proxy op = casycom_create_proxy (iid, oid_Broadcast);
    Msg* msg = casymsg_begin (&op, method_create_object, 0);
    void* o = casycom_find_or_create_destination (msg)->o;
    casymsg_free (msg);
    return o;
}

static void casycom_destroy_object (MsgLink* ol)
{
    // destroying an object can cause all kinds of ugly recursion as notified
    // objects destroy proxies and mess up OMap. To work around these problems
    // links must be saved and various locks set. ol->o is one of those locks.
    if (!ol->o)
	return;
    DEBUG_PRINTF ("[T] destroying object %hu.%s\n", ol->h.dest, ol->h.interface->name);
    // Call the destructor, if set.
    if (ol->factory->destroy) {
	ol->factory->destroy (ol->o);
	ol->o = NULL;
    } else
	xfree (ol->o);	// Otherwise just free
    ol->flags = 0;
    const oid_t oid = ol->h.dest;
    // Notify callers of destruction
    oid_t callers [16];
    unsigned nCallers = 0;
    // In two passes because object_destroyed handlers can modify OMap
    for (size_t di = 0; di < _casycom_omap.size; ++di) {
	const MsgLink* cl = &_casycom_omap.d[di];
	if (cl->h.dest == oid && cl->h.src != oid_Broadcast)		// Object calls the destroyed object
	    callers[nCallers++] = cl->h.src;
    }
    for (unsigned i = 0; i < nCallers; ++i) {
	const MsgLink* cl = casycom_find_destination (callers[i]);	// Find the link with its pointer
	if (cl && cl->factory->object_destroyed) {			// notify of destruction, if requested
	    DEBUG_PRINTF ("[T]\tNotifying object %hu -> %hu.%s\n", cl->h.src, cl->h.dest, cl->h.interface->name);
	    cl->factory->object_destroyed (cl->o, oid);
	}
    }
    // Erase all links from this object
    for (size_t di = 0; di < _casycom_omap.size; ++di) {
	if (_casycom_omap.d[di].h.src == oid) {
	    casycom_destroy_link_at (di);
	    di = SIZE_MAX;	// recursion will modify OMap, so have to start over
	}
    }
}

static inline void casycom_destroy_unused_objects (void)
{
    for (size_t i = 0; i < _casycom_omap.size; ++i) {
	if (_casycom_omap.d[i].flags & (1<<f_Unused)) {
	    DEBUG_PRINTF ("[I] destroying unused object %hu.%s\n", _casycom_omap.d[i].h.dest, _casycom_omap.d[i].h.interface->name);
	    casycom_destroy_object (&_casycom_omap.d[i]);
	    i = SIZE_MAX;	// start over because casycom_destroy_object modifies OMap
	}
    }
}

static const DTable* casycom_find_dtable (const Factory* o, iid_t iid)
{
    for (const DTable* const* oi = (const DTable* const*) o->dtable; *oi; ++oi)
	if ((*oi)->interface == iid)
	    return *oi;
    if (o == _casycom_default_object)
	return _casycom_default_object->dtable[0];
    return NULL;
}

static const Factory* casycom_find_factory (iid_t iid)
{
    for (size_t i = 0; i < _casycom_object_table.size; ++i) {
	const Factory* ot = _casycom_object_table.d[i];
	if (casycom_find_dtable (ot, iid))
	    return ot;
    }
    return _casycom_default_object;
}

iid_t casycom_interface_by_name (const char* iname)
{
    for (size_t i = 0; i < _casycom_object_table.size; ++i) {
	const Factory* f = _casycom_object_table.d[i];
	for (const DTable* const* di = (const DTable* const*) f->dtable; *di; ++di)
	    if (!strcmp ((*di)->interface->name, iname))
		return (*di)->interface;
    }
    if (_casycom_default_object) {
        iid_t default_interface = ((const DTable*)_casycom_default_object->dtable[0])->interface;
	if (!strcmp (default_interface->name, iname))
	    return default_interface;
    }
    return NULL;
}

static MsgLink* casycom_find_or_create_destination (const Msg* msg)
{
    MsgLink* ml;
    // Object constructor may create proxies, modifying the link map,
    // so if a new object is created, need to find the link again.
    for (void* no = NULL;;) {
	ml = casycom_find_destination (msg->h.dest);
	if (!ml || ml->o || (ml->o = no))
	    break;
	no = casycom_create_link_object (ml, msg);	// create the object, if needed
    }
    return ml;
}

//}}}-------------------------------------------------------------------
//{{{ Message queue management

// This is privately exported to msg.c . Do not use directly.
// Places \p msg into the output queue. It is thread-safe.
void casycom_queue_message (Msg* msg)
{
    #ifndef NDEBUG	// Message validity checks
	assert (msg->h.interface && (!msg->size || msg->body) && "invalid message");
	const Factory* dest_factory = casycom_find_factory (msg->h.interface);
	if (!dest_factory)
	    DEBUG_PRINTF ("Error: you must call casycom_register (&f_%s) to use this interface\n", casymsg_interface_name(msg));
	assert (dest_factory && "message addressed to unregistered interface");
	MsgLink* destl = casycom_find_destination (msg->h.dest);
	assert (destl && "message addressed to an unknown destination");
	const DTable* dtable = casycom_find_dtable (destl->factory, msg->h.interface);
	assert (dtable && "message forwarded to object that does not support its interface");
	assert (casycom_link_for_proxy(&msg->h) < _casycom_omap.size && "message sent through a deleted proxy; do not delete proxies in the destructor or in ObjectDeleted!");
	if (msg->imethod != method_create_object) {
	    assert (msg->imethod < casyiface_count_methods (msg->h.interface) && "invalid message destination method");
	    size_t vmsgsize = casymsg_validate_signature (msg);
	    if (DEBUG_MSG_TRACE && msg->size != vmsgsize) {
		DEBUG_PRINTF ("Error: message body size %zu does not match signature '%s':\n", vmsgsize, casymsg_signature(msg));
		casycom_debug_message_dump (msg);
	    }
	    assert (msg->size == vmsgsize && "message data does not match method signature");
	    assert ((!strchr(casymsg_signature(msg),'h') || msg->fdoffset != NO_FD_IN_MESSAGE) && "message signature requires a file descriptor in the message body, but none was written");
	    assert ((msg->fdoffset == NO_FD_IN_MESSAGE || (msg->fdoffset+4u <= msg->size && ceilg(msg->fdoffset,4) == msg->fdoffset)) && "you must use casymsg_write_fd to write a file descriptor to a message");
	} else
	    assert (!msg->size && msg->fdoffset == NO_FD_IN_MESSAGE && "invalid createObject message");
    #endif
    acquire_lock (&_casycom_output_queue_lock);
    vector_push_back (&_casycom_output_queue, &msg);
    release_lock (&_casycom_output_queue_lock);
}

static void casycom_do_message_queues (void)
{
    // Deliver all messages in the input queue
    for (size_t m = 0; m < _casycom_input_queue.size; ++m) {
	const Msg* msg = _casycom_input_queue.d[m];
	if (DEBUG_MSG_TRACE)
	    casycom_debug_message_dump (msg);
	MsgLink* ml = casycom_find_or_create_destination (msg);
	if (!ml)	// message addressed to object deleted after sending
	    continue;
	// Call the interface dispatch with the object and the message
	const DTable* dtable = casycom_find_dtable (ml->factory, msg->h.interface);
	((pfn_dispatch) dtable->interface->dispatch) (dtable, ml->o, msg);
	// After each message, check for generated errors
	if (_casycom_error && !casycom_forward_error (msg->h.dest, msg->h.dest)) {
	    // If nobody can handle the error, print it and quit
	    casycom_log (LOG_ERR, "Error: %s\n", _casycom_error);
	    casycom_quit (EXIT_FAILURE);
	    break;
	}
    }
    // Clear input queue
    for (size_t m = 0; m < _casycom_input_queue.size; ++m)
	casymsg_free (_casycom_input_queue.d[m]);
    vector_clear (&_casycom_input_queue);
    // And make the output queue the input queue for the next round
    acquire_lock (&_casycom_output_queue_lock);
    vector_swap (&_casycom_input_queue, &_casycom_output_queue);
    release_lock (&_casycom_output_queue_lock);
}

void casycom_debug_message_dump (const Msg* msg)
{
    if (!msg) {
	printf ("[T] NULL Message\n");
	return;
    }
    printf ("[T] Message[%u] %hu -> %hu.%s.%s\n", msg->size, msg->h.src, msg->h.dest, casymsg_interface_name(msg), casymsg_method_name(msg));
    hexdump (msg->body, msg->size);
}

//}}}-------------------------------------------------------------------
//--- Main API

/// initializes the library and the event loop
void casycom_init (void)
{
    DEBUG_PRINTF ("[I] initializing casycom\n");
    atexit (casycom_reset);
    int syslogopt = 0, syslogfac = LOG_USER;
    if (isatty (STDIN_FILENO))
	syslogopt = LOG_PERROR;
    else
	syslogfac = LOG_DAEMON;
    openlog (NULL, syslogopt, syslogfac);
}

/// Resets the framework to its initial state
void casycom_reset (void)
{
    DEBUG_PRINTF ("[I] Resetting casycom\n");
    while (_casycom_omap.size)
	casycom_destroy_link_at (_casycom_omap.size-1);
    vector_deallocate (&_casycom_omap);
    acquire_lock (&_casycom_output_queue_lock);
    for (size_t m = 0; m < _casycom_output_queue.size; ++m)
	casymsg_free (_casycom_output_queue.d[m]);
    vector_deallocate (&_casycom_output_queue);
    release_lock (&_casycom_output_queue_lock);
    for (size_t m = 0; m < _casycom_input_queue.size; ++m)
	casymsg_free (_casycom_input_queue.d[m]);
    vector_deallocate (&_casycom_input_queue);
    vector_deallocate (&_casycom_object_table);
    xfree (_casycom_error);
    DEBUG_PRINTF ("[I] Reset complete\n");
}

/// Replaces casycom_init if casycom is the top-level framework in your process
void casycom_framework_init (const Factory* oapp, argc_t argc, argv_t argv)
{
    casycom_install_signal_handlers();
    casycom_init();
    casycom_register (oapp);
    _casycom_appp = casycom_create_proxy_to (&i_App, oid_Broadcast, oid_App);
    PApp_init (&_casycom_appp, argc, argv);
}

/// Sets the quit flag, causing the event loop to quit once all queued events are processed
void casycom_quit (int exitCode)
{
    DEBUG_PRINTF ("[T] Quit requested, exit code %d\n", exitCode);
    _casycom_exit_code = exitCode;
    _casycom_quitting = true;
}

/// Returns the current exit code
int casycom_exit_code (void)
    { return _casycom_exit_code; }
bool casycom_is_quitting (void)
    { return _casycom_quitting; }
bool casycom_is_failed (void)
    { return _casycom_error != NULL; }

/// The main event loop. Returns exit code.
int casycom_main (void)
{
    for (_casycom_quitting = false; !_casycom_quitting; casycom_idle())
	casycom_do_message_queues();
    return _casycom_exit_code;
}

static void casycom_idle (void)
{
    DEBUG_PRINTF ("[I]=======================================================================\n");
    casycom_send_signal_message();	// Check if a signal has fired
    casycom_destroy_unused_objects();	// destroy objects marked unused
    // Process timers and fd waits
    int waittime = -1;
    if (_casycom_input_queue.size + _casycom_output_queue.size + _casycom_quitting)
	waittime = 0;	// Do not wait if there are packets in the queue
    bool haveTimers = Timer_run_timer (waittime);
    // Quit when there are no more packets or timers
    if (!haveTimers && !(_casycom_input_queue.size + _casycom_output_queue.size)) {
	DEBUG_PRINTF ("[E] Ran out of messages. Quitting.\n");
	casycom_quit (EXIT_SUCCESS);
    }
}

/// Do one message loop iteration; for non-framework operation
/// Returns false when all queues are empty
bool casycom_loop_once (void)
{
    Timer_run_timer (0);		// Check watched fds
    casycom_do_message_queues();	// Process any resulting messages
    casycom_destroy_unused_objects();	// Destroy objects marked unused
    return _casycom_input_queue.size+_casycom_output_queue.size;
}

/// Create error to be handled at next casycom_forward_error call
void casycom_error (const char* fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    char* e = NULL;
    // If error printing failed, use default error message
    if (0 > vasprintf (&e, fmt, args))
	e = strdup ("unknown error");
    va_end (args);
    if (!_casycom_error)	// First error message; move
	_casycom_error = e;
    else {
	char* ne = NULL;	// Subsequent messages are appended
	if (0 <= asprintf (&ne, "%s\n\t%s", _casycom_error, e)) {
	    xfree (_casycom_error);
	    _casycom_error = ne;
	    xfree (e);
	}
    }
    DEBUG_PRINTF ("[E] Error set: %s\n", _casycom_error);
}

/// Forward error to object \p oid, indicating \e eoid as the failed object.
///
/// If the called object can not handle the error, it will be marked as failed
/// and the error will be forward to its creator. If no object in this chain
/// handles the error, false will be returned. casycom_forward_error is always
/// called if an error exists after a message has been delivered. Manual
/// invocation is typically needed only when an object failure causes additional
/// errors not on the create chain.
bool casycom_forward_error (oid_t oid, oid_t eoid)
{
    assert (_casycom_error && "you must first set the error with casycom_error");
    // See if the object can handle the error
    MsgLink* ml = casycom_find_destination (oid);
    if (!ml)	// no further links in the chain, set to unhandled
	return false;
    DEBUG_PRINTF ("[E] Handling error in object %hu\n", ml->h.dest);
    if (ml->o && ml->factory->error && ml->factory->error (ml->o, eoid, _casycom_error)) {
	DEBUG_PRINTF ("[E] Error handled\n");
	xfree (_casycom_error);
	return true;
    }
    assert (ml->h.src != oid && "an object is never created by itself; use oid_Broadcast as creator for static objects");
    // If not, fail this object and forward to creator
    return casycom_forward_error (ml->h.src, oid);
}
