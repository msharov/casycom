// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "xcom.h"
#include "timer.h"
#include <fcntl.h>
#include <sys/un.h>
#include <paths.h>

//{{{ COM interface ----------------------------------------------------

typedef void (*MFN_COM_error)(void* vo, const char* error, const Msg* msg);
typedef void (*MFN_COM_export)(void* vo, const char* elist, const Msg* msg);
typedef void (*MFN_COM_delete)(void* vo, const Msg* msg);
typedef struct _DCOM {
    iid_t		interface;
    MFN_COM_error	COM_error;
    MFN_COM_export	COM_export;
    MFN_COM_delete	COM_delete;
} DCOM;

//}}}-------------------------------------------------------------------
//{{{ PCOM

static void COMRelay_COM_message (void* vo, Msg* msg);

//----------------------------------------------------------------------

enum {
    method_COM_error,
    method_COM_export,
    method_COM_delete
};

static Msg* PCOM_error_message (const Proxy* pp, const char* error)
{
    Msg* msg = casymsg_begin (pp, method_COM_error, casystm_size_string(error));
    WStm os = casymsg_write (msg);
    casystm_write_string (&os, error);
    assert (msg->size == casymsg_validate_signature (msg) && "message data does not match method signature");
    return msg;
}

static Msg* PCOM_export_message (const Proxy* pp, const char* elist)
{
    Msg* msg = casymsg_begin (pp, method_COM_export, casystm_size_string(elist));
    WStm os = casymsg_write (msg);
    casystm_write_string (&os, elist);
    assert (msg->size == casymsg_validate_signature (msg) && "message data does not match method signature");
    return msg;
}

static Msg* PCOM_delete_message (const Proxy* pp)
{
    Msg* msg = casymsg_begin (pp, method_COM_delete, 0);
    assert (msg->size == casymsg_validate_signature (msg) && "message data does not match method signature");
    return msg;
}

//----------------------------------------------------------------------

static void PCOM_create_object (const Proxy* pp)
{
    casymsg_end (casymsg_begin (pp, method_create_object, 0));
}

static void PCOM_dispatch (const DCOM* dtable, void* o, Msg* msg);

static const Interface i_COM = {
    .name = "COM",
    .dispatch = PCOM_dispatch,
    .method = { "error\0s", "export\0s", "delete\0", NULL }
};

static void PCOM_dispatch (const DCOM* dtable, void* o, Msg* msg)
{
    if (msg->h.interface != &i_COM)
	COMRelay_COM_message (o, msg);
    else if (msg->imethod == method_COM_error) {
	RStm is = casymsg_read (msg);
	const char* error = casystm_read_string (&is);
	dtable->COM_error (o, error, msg);
    } else if (msg->imethod == method_COM_export) {
	RStm is = casymsg_read (msg);
	const char* elist = casystm_read_string (&is);
	if (dtable->COM_export)
	    dtable->COM_export (o, elist, msg);
    } else if (msg->imethod == method_COM_delete)
	dtable->COM_delete (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

//}}}-------------------------------------------------------------------
//{{{ PExtern

enum {
    method_Extern_open,
    method_Extern_close
};

void PExtern_open (const Proxy* pp, int fd, enum EExternType atype, const iid_t* imported_interfaces, const iid_t* exported_interfaces)
{
    assert (pp->interface == &i_Extern && "this proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_Extern_open, 4+4+8+8);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casystm_write_uint32 (&os, atype);
    casystm_write_ptr (&os, imported_interfaces);
    casystm_write_ptr (&os, exported_interfaces);
    casymsg_end (msg);
}

void PExtern_close (const Proxy* pp)
{
    casymsg_end (casymsg_begin (pp, method_Extern_close, 0));
}

static void PExtern_dispatch (const DExtern* dtable, void* o, const Msg* msg)
{
    assert (dtable->interface == &i_Extern && "dispatch given dtable for a different interface");
    if (msg->imethod == method_Extern_open) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	enum EExternType atype = casystm_read_uint32 (&is);
	const iid_t* imported_interfaces = casystm_read_ptr (&is);
	const iid_t* exported_interfaces = casystm_read_ptr (&is);
	dtable->Extern_open (o, fd, atype, imported_interfaces, exported_interfaces);
    } else if (msg->imethod == method_Extern_close)
	dtable->Extern_close (o);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_Extern = {
    .name = "Extern",
    .dispatch = PExtern_dispatch,
    .method = { "open\0iuxx", "close\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ PExtern_connect

int PExtern_connect (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* imported_interfaces)
{
    int fd = socket (addr->sa_family, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, IPPROTO_IP);
    if (fd < 0)
	return fd;
    if (0 > connect (fd, addr, addrlen) && errno != EINPROGRESS && errno != EINTR) {
	DEBUG_PRINTF ("[E] Failed to connect to socket: %s\n", strerror(errno));
	close (fd);
	return -1;
    }
    PExtern_open (pp, fd, EXTERN_CLIENT, imported_interfaces, NULL);
    return fd;
}

/// create local socket with given path
int PExtern_connect_local (const Proxy* pp, const char* path, const iid_t* imported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if (ARRAY_SIZE(addr.sun_path) <= (size_t) snprintf (ARRAY_BLOCK(addr.sun_path), "%s", path)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] connecting to socket %s\n", addr.sun_path);
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

/// create local socket of the given name in the system standard location for such
int PExtern_connect_system_local (const Proxy* pp, const char* sockname, const iid_t* imported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if (ARRAY_SIZE(addr.sun_path) <= (size_t) snprintf (ARRAY_BLOCK(addr.sun_path), _PATH_VARRUN "%s", sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] connecting to socket %s\n", addr.sun_path);
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

/// create local socket of the given name in the user standard location for such
int PExtern_connect_user_local (const Proxy* pp, const char* sockname, const iid_t* imported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    const char* runtimeDir = getenv ("XDG_RUNTIME_DIR");
    if (!runtimeDir)
	runtimeDir = _PATH_TMP;
    if (ARRAY_SIZE(addr.sun_path) <= (size_t) snprintf (ARRAY_BLOCK(addr.sun_path), "%s/%s", runtimeDir, sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] connecting to socket %s\n", addr.sun_path);
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

int PExtern_launch_pipe (const Proxy* pp, const char* exe, const char* arg, const iid_t* imported_interfaces)
{
    // Check if executable exists before the fork to allow proper error handling
    char exepath [PATH_MAX];
    const char* exefp = executable_in_path (exe, ARRAY_BLOCK(exepath));
    if (!exefp) {
	errno = ENOENT;
	return -1;
    }

    // create socket pipe, will be connected to stdin in server
    enum { socket_ClientSide, socket_ServerSide, socket_N };
    int socks [socket_N];
    if (0 > socketpair (PF_LOCAL, SOCK_STREAM| SOCK_NONBLOCK, 0, socks))
	return -1;

    int fr = fork();
    if (fr < 0) {
	close (socks[socket_ClientSide]);
	close (socks[socket_ServerSide]);
	return -1;
    }
    if (fr == 0) {	// Server side
	close (socks[socket_ClientSide]);
	dup2 (socks[socket_ServerSide], STDIN_FILENO);
	execl (exefp, exe, arg, NULL);

	// If exec failed, log the error and exit
	casycom_log (LOG_ERR, "Error: failed to launch pipe to '%s %s': %s\n", exe, arg, strerror(errno));
	casycom_disable_debug_output();
	exit (EXIT_FAILURE);
    } else {		// Client side
	close (socks[socket_ServerSide]);
	PExtern_open (pp, socks[socket_ClientSide], EXTERN_CLIENT, imported_interfaces, NULL);
    }
    return socks[0];
}

//}}}-------------------------------------------------------------------
//{{{ PExternR

enum { method_ExternR_connected };

void PExternR_connected (const Proxy* pp, const ExternInfo* einfo)
{
    assert (pp->interface == &i_ExternR && "this proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_ExternR_connected, 8);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, einfo);
    casymsg_end (msg);
}

static void PExternR_dispatch (const DExternR* dtable, void* o, const Msg* msg)
{
    assert (dtable->interface == &i_ExternR && "dispatch given dtable for a different interface");
    if (msg->imethod == method_ExternR_connected) {
	RStm is = casymsg_read (msg);
	const ExternInfo* einfo = casystm_read_ptr (&is);
	if (dtable->ExternR_connected)
	    dtable->ExternR_connected (o, einfo);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_ExternR = {
    .name = "ExternR",
    .dispatch = PExternR_dispatch,
    .method = { "connected\0x", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ Extern
//----------------------------------------------------------------------
//{{{2 Module data

DECLARE_VECTOR_TYPE (MsgVector, Msg*);

enum { MAX_MSG_HEADER_SIZE = UINT8_MAX-8 };

enum {
    extid_COM,
    extid_ClientBase = extid_COM,		// Set these up to be equal to COMRelay nodeid on the client
    extid_ClientLast = extid_ClientBase+oid_Last,
    extid_ServerBase = extid_ClientLast+1,	// and nodeid + halfrange on the server (32000 is 0x7d00, mostly round number in both bases)
    extid_ServerLast = extid_ServerBase+oid_Last
};

typedef struct _ExtMsgHeader {
    uint32_t	sz;		///< message body size, aligned to c_Msgceilgment
    uint16_t	extid;		///< Destination node iid
    uint8_t	fdoffset;	///< Offset to file descriptor in message body, if passing
    uint8_t	hsz;		///< Full size of header
} ExtMsgHeader;

typedef union _ExtMsgHeaderBuf {
    ExtMsgHeader	h;
    char		d [MAX_MSG_HEADER_SIZE];
} ExtMsgHeaderBuf;

typedef struct _COMConn {
    Proxy	proxy;
    uint16_t	extid;
} COMConn;

DECLARE_VECTOR_TYPE (COMConnVector, COMConn);

typedef struct _Extern {
    Proxy		reply;
    int			fd;
    uint32_t		outHWritten;
    uint32_t		outBWritten;
    uint32_t		inHRead;
    uint32_t		inBRead;
    Msg*		inMsg;
    const iid_t*	exported_interfaces;
    const iid_t*	all_imported_interfaces;
    ExternInfo		info;
    COMConnVector	conns;
    MsgVector		outgoing;
    Proxy		timer;
    int			inLastFd;
    ExtMsgHeaderBuf	inHBuf;
} Extern;

DECLARE_VECTOR_TYPE (ExternsVector, Extern*);
static VECTOR (ExternsVector, _Extern_externs);

//----------------------------------------------------------------------

static COMConn* Extern_COMConn_by_extid (Extern* o, uint16_t extid);
static bool Extern_is_interface_exported (const Extern* o, iid_t iid);
static bool Extern_is_valid_socket (Extern* o);
static bool Extern_validate_message (Extern* o, Msg* msg);
static bool Extern_validate_message_header (const Extern* o, const ExtMsgHeader* h);
static bool Extern_writing (Extern* o);
static iid_t Extern_lookup_in_msg_interface (const Extern* o);
static uint32_t Extern_lookup_in_msg_method (const Extern* o, const Msg* msg);
static void Extern_Extern_close (Extern* o);
static void Extern_queue_incoming_message (Extern* o, Msg* msg);
static void Extern_queue_outgoing_message (Extern* o, Msg* msg);
static void Extern_reading (Extern* o);
static void Extern_set_credentials_passing (Extern* o, int enable);
static void Extern_TimerR_timer (Extern* o, int fd, const Msg* msg);

//}}}2------------------------------------------------------------------
//{{{2 Interfaces

static void* Extern_create (const Msg* msg)
{
    Extern* o = xalloc (sizeof(Extern));
    vector_push_back (&_Extern_externs, &o);
    o->reply = casycom_create_reply_proxy (&i_ExternR, msg);
    o->info.oid = o->reply.src;
    o->fd = -1;
    o->inLastFd = -1;
    o->timer = casycom_create_proxy (&i_Timer, msg->h.dest);
    VECTOR_MEMBER_INIT (COMConnVector, o->conns);
    VECTOR_MEMBER_INIT (InterfaceVector, o->info.interfaces);
    VECTOR_MEMBER_INIT (MsgVector, o->outgoing);
    return o;
}

static void Extern_destroy (void* vo)
{
    Extern* o = vo;
    if (o->fd >= 0) {
	close (o->fd);
	o->fd = -1;
    }
    if (o->inLastFd >= 0) {
	close (o->inLastFd);
	o->inLastFd = -1;
    }
    casymsg_free (o->inMsg);
    for (size_t i = 0; i < o->outgoing.size; ++i)
	casymsg_free (o->outgoing.d[i]);
    vector_deallocate (&o->outgoing);
    vector_deallocate (&o->info.interfaces);
    vector_deallocate (&o->conns);
    for (size_t ei = 0; ei < _Extern_externs.size; ++ei)
	if (_Extern_externs.d[ei] == o)
	    vector_erase (&_Extern_externs, ei--);
    if (!_Extern_externs.size)
	vector_deallocate (&_Extern_externs);
    xfree (o);
}

static void Extern_Extern_open (Extern* o, int fd, enum EExternType atype, const iid_t* imported_interfaces, const iid_t* exported_interfaces)
{
    o->fd = fd;
    o->info.is_client = atype == EXTERN_CLIENT;
    o->all_imported_interfaces = imported_interfaces;	// o->info.interfaces will be set when COM_export message arrives
    o->exported_interfaces = exported_interfaces;
    if (!Extern_is_valid_socket (o)) {
	casycom_error ("incompatible socket type");
	return Extern_Extern_close (o);
    }
    if (o->info.is_unix_socket)
	Extern_set_credentials_passing (o, true);
    // To complete the handshake, create the list of export interfaces
    char exlist[256] = {}, *pexlist = &exlist[0];
    for (const iid_t* ei = o->exported_interfaces; ei && *ei; ++ei)
	pexlist += sprintf (pexlist, "%s,", (*ei)->name);
    assert (pexlist < &exlist[ARRAY_SIZE(exlist)] && "too many exported interfaces");
    pexlist[-1] = 0;	// Replace last comma
    // Send the exported interfaces list in a COM_export message
    const Proxy comp = { .interface = &i_COM, .dest = o->info.oid, .src = o->info.oid };
    Extern_queue_outgoing_message (o, PCOM_export_message (&comp, exlist));
    // Queueing will also initialize the fd timer
}

static void Extern_Extern_close (Extern* o)
{
    if (o->fd >= 0) {
	close (o->fd);
	o->fd = -1;
    }
    casycom_mark_unused (o);
}

static Extern* Extern_find_by_interface (iid_t iid)
{
    for (size_t ei = 0; ei < _Extern_externs.size; ++ei) {
	Extern* e = _Extern_externs.d[ei];
	for (size_t ii = 0; ii < e->info.interfaces.size; ++ii)
	    if (e->info.interfaces.d[ii] == iid)
		return e;
    }
    return NULL;
}

static Extern* Extern_find_by_id (oid_t oid)
{
    for (size_t ei = 0; ei < _Extern_externs.size; ++ei) {
	Extern* e = _Extern_externs.d[ei];
	for (size_t ii = 0; ii < e->conns.size; ++ii)
	    if (e->conns.d[ii].proxy.dest == oid)
		return e;
    }
    return NULL;
}

static void Extern_COM_error (Extern* o, const char* e, const Msg* msg UNUSED)
{
    // Error arriving to extid_COM indicates an error in the Extern
    // object on the other side, terminating the connection.
    casycom_error ("%s", e);
    Extern_Extern_close (o);
}

static void Extern_COM_export (Extern* o, const char* ilist, const Msg* msg UNUSED)
{
    // The export list arrives during the handshake and contains a
    // comma-separated list of interfaces exported by the other side.
    vector_clear (&o->info.interfaces);	// Changing the list afterwards is also allowed.
    for (const char* iname = ilist; iname && *iname; ++ilist) {
	if (!*ilist || *ilist == ',') {
	    // info.interfaces contains iid_ts from all_imported_interfaces that are in ilist
	    for (const iid_t* iii = o->all_imported_interfaces; iii && *iii; ++iii)
		if (0 == strncmp ((*iii)->name, iname, ilist-iname))
		    vector_push_back (&o->info.interfaces, iii);
	    iname = ilist;
	}
    }
    // Now that the info.interfaces list is filled, the handshake is complete
    PExternR_connected (&o->reply, &o->info);
}

static void Extern_COM_delete (Extern* o, const Msg* msg UNUSED)
{
    Extern_Extern_close (o);
}

static const DCOM d_Extern_COM = {
    .interface	= &i_COM,
    DMETHOD (Extern, COM_error),
    DMETHOD (Extern, COM_export),
    DMETHOD (Extern, COM_delete)
};

//}}}2------------------------------------------------------------------
//{{{2 Socket management

static bool Extern_is_valid_socket (Extern* o)
{
    // The incoming socket must be a stream socket
    int v;
    socklen_t l = sizeof(v);
    if (getsockopt (o->fd, SOL_SOCKET, SO_TYPE, &v, &l) < 0 || v != SOCK_STREAM)
	return false;
    // And it must match the family (PF_LOCAL or PF_INET)
    struct sockaddr_storage ss;
    l = sizeof(ss);
    if (getsockname(o->fd, (struct sockaddr*) &ss, &l) < 0)
	return false;
    o->info.is_unix_socket = false;
    if (ss.ss_family == PF_LOCAL)
	o->info.is_unix_socket = true;
    else if (ss.ss_family != PF_INET)
	return false;
    // If matches, need to set the fd nonblocking for the poll loop to work
    int f = fcntl (o->fd, F_GETFL);
    if (f < 0)
	return false;
    if (0 > fcntl (o->fd, F_SETFL, f| O_NONBLOCK| O_CLOEXEC))
	return false;
    return true;
}

#ifdef SCM_CREDS // BSD interface
    #define SCM_CREDENTIALS	SCM_CREDS
    #define SO_PASSCRED		LOCAL_PEERCRED
    #define ucred		cmsgcred
#elif !defined(SCM_CREDENTIALS)
    #error "socket credentials passing not supported"
#endif

static void Extern_set_credentials_passing (Extern* o, int enable)
{
    if (o->fd < 0 || !o->info.is_unix_socket)
	return;
    if (0 > setsockopt (o->fd, SOL_SOCKET, SO_PASSCRED, &enable, sizeof(enable))) {
	casycom_error ("setsockopt(SO_PASSCRED): %s", strerror(errno));
	Extern_Extern_close (o);
    }
}

static void Extern_TimerR_timer (Extern* o, int fd UNUSED, const Msg* msg UNUSED)
{
    if (o->fd >= 0)
	Extern_reading (o);
    enum ETimerWatchCmd tcmd = WATCH_READ;
    if (o->fd >= 0 && Extern_writing (o))
	tcmd = WATCH_RDWR;
    if (o->fd >= 0)
	PTimer_watch (&o->timer, tcmd, o->fd, TIMER_NONE);
}

//}}}2------------------------------------------------------------------
//{{{2 reading

static DEFINE_ALIAS_CAST(cred_alias_cast, ExternCredentials)
static DEFINE_ALIAS_CAST(int_alias_cast, int)

static void Extern_reading (Extern* o)
{
    for (;;) {	// Read until EAGAIN
	// create iovecs for input
	// There are three of them, representing the three parts of each
	// message: the fixed header (2), the header strings (0), and the
	// message body. The common case is to read the variable parts
	// and the fixed header of the next message in each recvmsg call.
	struct iovec iov[3] = {};
	if (o->inHRead >= sizeof(o->inHBuf.h)) {
	    assert (o->inMsg && "the message must be created after the fixed header is read");
	    iov[0].iov_base = &o->inHBuf.d[o->inHRead];
	    iov[0].iov_len = o->inHBuf.h.hsz - o->inHRead;
	    iov[1].iov_base = (char*) o->inMsg->body + o->inBRead;
	    iov[1].iov_len = o->inMsg->size - o->inBRead;
	    iov[2].iov_base = &o->inHBuf.h;
	    iov[2].iov_len = sizeof(o->inHBuf.h);
	} else {	// Fixed header partially read
	    iov[2].iov_base = &o->inHBuf.d[o->inHRead];
	    iov[2].iov_len = sizeof(o->inHBuf.h) - o->inHRead;
	}
	// Build struct for recvmsg
	struct msghdr mh = {
	    .msg_iov = iov,
	    .msg_iovlen = ARRAY_SIZE(iov)
	};
	// Ancillary space for fd and credentials
	char cmsgbuf [CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(struct ucred))] = {};
	mh.msg_control = cmsgbuf;
	mh.msg_controllen = sizeof(cmsgbuf);
	// Receive some data
	int br = recvmsg (o->fd, &mh, 0);
	if (br <= 0) {
	    if (!br || errno == ECONNRESET)	// br == 0 when remote end closes. No error then, just need to close this end too.
		DEBUG_PRINTF ("[X] %hu.Extern: rsocket %d closed by the other end\n", o->info.oid, o->fd);
	    else {
		if (errno == EINTR)
		    continue;
		if (errno == EAGAIN)
		    return;
		casycom_error ("recvmsg: %s", strerror(errno));
	    }
	    return Extern_Extern_close (o);
	}
	DEBUG_PRINTF ("[X] Read %d bytes from socket %d\n", br, o->fd);
	// Check if ancillary data was passed
	for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mh); cmsg; cmsg = CMSG_NXTHDR(&mh, cmsg)) {
	    if (cmsg->cmsg_type == SCM_CREDENTIALS) {
		o->info.creds = *cred_alias_cast (CMSG_DATA(cmsg));
		Extern_set_credentials_passing (o, false);	// Checked when the socket is connected. Changing credentials (such as by passing the socket to another process) is not supported.
		DEBUG_PRINTF ("[X] Received credentials: pid=%u,uid=%u,gid=%u\n", o->info.creds.pid, o->info.creds.uid, o->info.creds.gid);
	    } else if (cmsg->cmsg_type == SCM_RIGHTS) {
		if (o->inLastFd >= 0) {
		    casycom_error ("multiple file descriptors received in one message");
		    return Extern_Extern_close (o);
		}
		o->inLastFd = *int_alias_cast (CMSG_DATA(cmsg));
		DEBUG_PRINTF ("[X] Received fd %d\n", o->inLastFd);
	    }
	}
	// Adjust read sizes
	unsigned hbr = br;
	if (hbr > iov[0].iov_len)
	    hbr = iov[0].iov_len;
	o->inHRead += hbr;
	br -= hbr;
	unsigned bbr = br;
	if (bbr > iov[1].iov_len)
	    bbr = iov[1].iov_len;
	o->inBRead += bbr;
	br -= bbr;
	// Check if a message has been completed
	if (o->inMsg && bbr == iov[1].iov_len) {
	    if (DEBUG_MSG_TRACE) {
		DEBUG_PRINTF ("[X] message for extid %u of size %u completed:\n", o->inMsg->extid, o->inMsg->size);
		hexdump (o->inHBuf.d, o->inHBuf.h.hsz);
		hexdump (o->inMsg->body, o->inMsg->size);
	    }
	    assert (o->inBRead == o->inMsg->size);
	    if (!Extern_validate_message (o, o->inMsg)) {
		casycom_error ("invalid message");
		return Extern_Extern_close (o);
	    }
	    // If it has, queue it and reset inMsg pointer
	    if (o->inMsg)
		Extern_queue_incoming_message (o, o->inMsg);
	    o->inMsg = NULL;
	    o->inHRead = 0;
	    o->inBRead = 0;
	    // Clear variable header data
	    memset (&o->inHBuf.d[sizeof(o->inHBuf.h)], 0, sizeof(o->inHBuf)-sizeof(o->inHBuf.h));
	}
	// Check for partial fixed header read
	if (br) {
	    assert (!o->inMsg && o->inHRead <= sizeof(o->inHBuf.h));
	    o->inHRead += br;
	}
	// Check if a new message can be started
	if (br && o->inHRead >= sizeof(o->inHBuf.h)) {
	    assert (o->inHRead == sizeof(o->inHBuf.h) && !o->inMsg && "internal error: message data read, but message not complete");
	    if (!Extern_validate_message_header (o, &o->inHBuf.h)) {
		casycom_error ("invalid message");
		return Extern_Extern_close (o);
	    }
	    o->inMsg = casymsg_begin (&o->reply, method_create_object, o->inHBuf.h.sz);
	    o->inMsg->extid = o->inHBuf.h.extid;
	    o->inMsg->fdoffset = o->inHBuf.h.fdoffset;
	}
    }
}

//}}}2------------------------------------------------------------------
//{{{2 Incoming message processing

static bool Extern_validate_message_header (const Extern* o UNUSED, const ExtMsgHeader* h)
{
    if (h->hsz & (MESSAGE_HEADER_ALIGNMENT-1))
	return false;
    if (h->sz & (MESSAGE_BODY_ALIGNMENT-1))
	return false;
    if (h->fdoffset != NO_FD_IN_MESSAGE && h->fdoffset+4u > h->sz)
	return false;
    return true;
}

static COMConn* Extern_COMConn_by_extid (Extern* o, uint16_t extid)
{
    for (size_t i = 0; i < o->conns.size; ++i)
	if (o->conns.d[i].extid == extid)
	    return &o->conns.d[i];
    return NULL;
}

static COMConn* Extern_COMConn_by_oid (Extern* o, oid_t oid)
{
    for (size_t i = 0; i < o->conns.size; ++i)
	if (o->conns.d[i].proxy.dest == oid)
	    return &o->conns.d[i];
    return NULL;
}

static bool Extern_validate_message (Extern* o, Msg* msg)
{
    // The interface and method names are now read, so can get the local pointers for them
    msg->h.interface = Extern_lookup_in_msg_interface (o);
    if (!msg->h.interface) {
	DEBUG_PRINTF ("[X] Unable to find the message interface\n");
	return false;
    }
    msg->imethod = Extern_lookup_in_msg_method (o, msg);
    if (msg->imethod == method_invalid) {
	DEBUG_PRINTF ("[X] Invalid method index in message\n");
	return false;
    }
    // And validate the message body by signature
    size_t vmsize = casymsg_validate_signature (msg);
    if (ceilg (vmsize, MESSAGE_BODY_ALIGNMENT) != msg->size) {	// Written size must be the aligned real size
	DEBUG_PRINTF ("[X] message body fails signature verification\n");
	return false;
    }
    msg->size = vmsize;	// The written size was its aligned value. The real value comes from the validator.
    if (msg->fdoffset != NO_FD_IN_MESSAGE) {
	DEBUG_PRINTF ("[X] Setting message file descriptor to %d at %hhu\n", o->inLastFd, msg->fdoffset);
	*int_alias_cast((char*) msg->body + msg->fdoffset) = o->inLastFd;
	o->inLastFd = -1;
    }
    if (msg->extid == extid_COM) {
	if (msg->h.interface != &i_COM)
	    DEBUG_PRINTF ("[X] extid_COM may only be used for the COM interface\n");
	return msg->h.interface == &i_COM;
    }
    // Look up existing object for this extid
    COMConn* conn = Extern_COMConn_by_extid (o, msg->extid);
    if (!conn) {		// If not present, then this is a request to create one
	// Do not create object for COM messages (such as COM delete)
	if (msg->h.interface == &i_COM) {
	    DEBUG_PRINTF ("[X] Ignoring COM message to extid %hu\n", msg->extid);
	    casymsg_free (msg);
	    o->inMsg = NULL;	// to skip queue_incoming_message in caller
	    return true;
	}
	// Prohibit creation of objects not on the export list
	if (!Extern_is_interface_exported (o, msg->h.interface)) {
	    DEBUG_PRINTF ("[X] message addressed to interface not on export list\n");
	    return false;
	}
	// Verify that the other end set the extid of new object in appropriate range
	if (msg->extid >= extid_ClientLast || (o->info.is_client && (msg->extid < extid_ServerBase || msg->extid >= extid_ServerLast))) {
	    DEBUG_PRINTF ("[X] Invalid extid in message\n");
	    return false;
	}
	// create the new connection object
	conn = vector_emplace_back (&o->conns);
	conn->proxy = casycom_create_proxy (&i_COM, o->info.oid);
	// The remote end sets the extid
	conn->extid = msg->extid;
	PCOM_create_object (&conn->proxy);
	DEBUG_PRINTF ("[X] New incoming connection %hu -> %hu.%s, extid %hu\n", conn->proxy.src, conn->proxy.dest, casymsg_interface_name(msg), conn->extid);
    }
    // Translate the extid into local addresses
    msg->h.src = conn->proxy.src;
    msg->h.dest = conn->proxy.dest;
    return true;
}

static void Extern_queue_incoming_message (Extern* o, Msg* msg)
{
    if (msg->extid == extid_COM) {
	PCOM_dispatch (&d_Extern_COM, o, msg);
	casymsg_free (msg);
    } else {
	DEBUG_PRINTF ("[X] Queueing incoming message[%u] %hu -> %hu.%s.%s\n", msg->size, msg->h.src, msg->h.dest, casymsg_interface_name(msg), casymsg_method_name(msg));
	casymsg_end (msg);
    }
}

static iid_t Extern_lookup_in_msg_interface (const Extern* o)
{
    const char* iname = &o->inHBuf.d[sizeof(o->inHBuf.h)];
    assert (o->inHRead < MAX_MSG_HEADER_SIZE && o->inHRead >= sizeof(o->inHBuf.h)+5);
    if (o->inHBuf.d[o->inHRead-1])	// Interface name and method must be nul terminated
	return NULL;
    return casycom_interface_by_name (iname);
}

static bool Extern_is_interface_exported (const Extern* o, iid_t iid)
{
    if (!o->exported_interfaces)
	return false;
    for (const Interface* const* ii = o->exported_interfaces; *ii; ++ii)
	if (iid == *ii)
	    return true;
    return false;
}

static uint32_t Extern_lookup_in_msg_method (const Extern* o, const Msg* msg)
{
    const char* hend = &o->inHBuf.d[o->inHBuf.h.hsz];
    const char* iname = &o->inHBuf.d[sizeof(o->inHBuf.h)];
    const char* mname = strnext (iname);
    if (mname >= hend)
	return method_invalid;
    const char* msig = strnext (mname);
    if (msig >= hend)
	return method_invalid;
    const char* mend = strnext (msig);
    if (mend > hend)
	return method_invalid;
    size_t mnsize = mend - mname;
    for (uint32_t mi = 0; msg->h.interface->method[mi]; ++mi) {
	const char* m = msg->h.interface->method[mi];
	size_t msz = strnext(strnext(m)) - m;
	if (mnsize == msz && 0 == memcmp (m, mname, msz))
	    return mi;
    }
    return method_invalid;
}

//}}}2------------------------------------------------------------------
//{{{2 writing

static void Extern_queue_outgoing_message (Extern* o, Msg* msg)
{
    if (msg->h.dest == o->info.oid)	// messages to the Extern object itself have extid_COM
	msg->extid = extid_COM;
    else {
	COMConn* conn = Extern_COMConn_by_oid (o, msg->h.dest);
	if (!conn) {
	    conn = vector_emplace_back (&o->conns);
	    // create the reply proxy from Extern to the COMRelay
	    conn->proxy = casycom_create_proxy_to (&i_COM, o->info.oid, msg->h.dest);
	    // Extids are assigned from oid with side-based offset
	    conn->extid = msg->h.dest + (o->info.is_client ? extid_ClientBase : extid_ServerBase);
	    DEBUG_PRINTF ("[X] New outgoing connection %hu -> %hu.%s, extid %hu\n", msg->h.src, msg->h.dest, casymsg_interface_name(msg), conn->extid);
	}
	msg->extid = conn->extid;
	// Once the object is deleted, the COMConn record must be recreated because it is linked to a specific object interface
	if (msg->h.interface == &i_COM && msg->imethod == method_COM_delete) {
	    DEBUG_PRINTF ("[X] destroying connection with extid %hu\n", conn->extid);
	    vector_erase (&o->conns, conn - o->conns.d);
	}
    }
    vector_push_back (&o->outgoing, &msg);
    Extern_TimerR_timer (o, 0, NULL);
}

static bool Extern_writing (Extern* o)
{
    // Write all queued messages
    while (o->outgoing.size) {
	Msg* msg = o->outgoing.d[0];
	// Marshal message header
	ExtMsgHeaderBuf hbuf = {};
	hbuf.h.sz = ceilg (msg->size, MESSAGE_BODY_ALIGNMENT);
	hbuf.h.extid = msg->extid;
	hbuf.h.fdoffset = msg->fdoffset;
	char* phstr = &hbuf.d[sizeof(hbuf.h)];
	const char* iname = casymsg_interface_name(msg);
	const char* mname = casymsg_method_name(msg);
	const char* msig = strnext (mname);
	assert (sizeof(ExtMsgHeader)+strlen(iname)+1+strlen(mname)+1+strlen(msig)+1 <= MAX_MSG_HEADER_SIZE && "the interface and method names for this message are too long to export");
	char* phend = stpcpy (stpcpy (stpcpy (phstr, iname)+1, mname)+1, msig)+1;
	hbuf.h.hsz = sizeof(hbuf.h) + ceilg (phend - phstr, MESSAGE_HEADER_ALIGNMENT);
	// create iovecs for output
	struct iovec iov[2] = {};
	if (hbuf.h.hsz > o->outHWritten) {
	    iov[0].iov_base = &hbuf.d[o->outHWritten];
	    iov[0].iov_len = hbuf.h.hsz - o->outHWritten;
	}
	if (hbuf.h.sz > o->outBWritten) {
	    iov[1].iov_base = (char*) msg->body + o->outBWritten;
	    iov[1].iov_len = hbuf.h.sz - o->outBWritten;
	}
	// Build outgoing struct for sendmsg
	struct msghdr mh = {
	    .msg_iov = iov,
	    .msg_iovlen = ARRAY_SIZE(iov)
	};
	// Add fd if being passed
	char fdbuf [CMSG_SPACE(sizeof(int))] = {};
	int fdpassed = -1;
	if (hbuf.h.fdoffset != NO_FD_IN_MESSAGE) {
	    mh.msg_control = fdbuf;
	    mh.msg_controllen = sizeof(fdbuf);
	    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&mh);
	    cmsg->cmsg_len = sizeof(fdbuf);
	    cmsg->cmsg_level = SOL_SOCKET;
	    cmsg->cmsg_type = SCM_RIGHTS;
	    fdpassed = *int_alias_cast((char*) msg->body + hbuf.h.fdoffset);
	    *int_alias_cast(CMSG_DATA (cmsg)) = fdpassed;
	}
	// And try writing it all
	int bw = sendmsg (o->fd, &mh, MSG_NOSIGNAL);
	if (bw <= 0) {
	    if (!bw || errno == ECONNRESET)	// bw == 0 when remote end closes. No error then, just need to close this end too.
		DEBUG_PRINTF ("[X] %hu.Extern: wsocket %d closed by the other end\n", o->info.oid, o->fd);
	    else {
		if (errno == EINTR)
		    continue;
		if (errno == EAGAIN)
		    return true;
		casycom_error ("sendmsg: %s", strerror(errno));
	    }
	    Extern_Extern_close (o);
	    return false;
	}
	DEBUG_PRINTF ("[X] Wrote %d of %u bytes of message %s.%s to socket %d\n", bw, hbuf.h.hsz+hbuf.h.sz, casymsg_interface_name(msg), casymsg_method_name(msg), o->fd);
	// Adjust written sizes
	unsigned hbw = bw;
	if (hbw > iov[0].iov_len)
	    hbw = iov[0].iov_len;
	o->outHWritten += hbw;
	bw -= hbw;
	o->outBWritten += bw;
	assert (o->outBWritten <= hbuf.h.sz && "sendmsg wrote more than given");
	// close the fd once successfully passed
	if (fdpassed >= 0) {
	    close (fdpassed);
	    fdpassed = -1;
	    // And prevent it being passed more than once
	    msg->fdoffset = NO_FD_IN_MESSAGE;
	}
	// Check if message has been fully written, and move to the next one if so
	if (o->outBWritten >= hbuf.h.sz) {
	    o->outHWritten = 0;
	    o->outBWritten = 0;
	    casymsg_free (o->outgoing.d[0]);
	    vector_erase (&o->outgoing, 0);
	}
    }
    return false;
}

//}}}2------------------------------------------------------------------
//{{{2 ExternInfo lookup interface

const ExternInfo* casycom_extern_info (oid_t eid)
{
    for (size_t ei = 0; ei < _Extern_externs.size; ++ei)
	if (_Extern_externs.d[ei]->info.oid == eid)
	    return &_Extern_externs.d[ei]->info;
    return NULL;
}

const ExternInfo* casycom_extern_object_info (oid_t oid)
{
    const Extern* e = Extern_find_by_id (oid);
    return e ? &e->info : NULL;
}

//}}}2------------------------------------------------------------------
//{{{2 Dtables and factory

static const DExtern d_Extern_Extern = {
    .interface	= &i_Extern,
    DMETHOD (Extern, Extern_open),
    DMETHOD (Extern, Extern_close)
};
static const DTimerR d_Extern_TimerR = {
    .interface	= &i_TimerR,
    DMETHOD (Extern, TimerR_timer)
};
static const Factory f_Extern = {
    .create	= Extern_create,
    .destroy	= Extern_destroy,
    .dtable	= { &d_Extern_Extern, &d_Extern_TimerR, NULL }
};
//}}}2
//}}}-------------------------------------------------------------------
//{{{ COMRelay object

typedef struct _COMRelay {
    Proxy	localp;		///< Proxy to the local object
    oid_t	externid;	///< oid of the extern connection
    Extern*	pExtern;	///< Outgoing connection
} COMRelay;

//----------------------------------------------------------------------

static void* COMRelay_create (const Msg* msg)
{
    COMRelay* o = xalloc (sizeof(COMRelay));
    // COM objects are created in one of two ways:
    // 1. By message going out-of-process, via the default object mechanism
    //    This results in msg->h.interface containing the imported interface.
    //    The local object already exists, and its proxy is created here.
    // 2. By message coming into the process, sent by Extern object. Extern
    //    will explicitly create a COM intermediate by sending a COM message
    //    with method_create_object. The message with the real interface will
    //    follow immediately after, and localp will be created in COMRelay_COM_message.
    if (msg->h.interface != &i_COM) {
	o->localp = casycom_create_reply_proxy (&i_COM, msg);
	o->pExtern = Extern_find_by_interface (msg->h.interface);
    } else
	o->pExtern = Extern_find_by_id (msg->h.dest);
    if (o->pExtern) {
	o->externid = o->pExtern->info.oid;
	// COMRelay does not send any messages to the Extern object, it calls its
	// functions directly. But having a proxy link allows object_destroyed notification.
	casycom_create_proxy_to (&i_COM, msg->h.dest, o->externid);
    }
    return o;
}

static void COMRelay_COM_message (void* vo, Msg* msg)
{
    COMRelay* o = vo;
    // The local object proxy is created in the constructor when the COM
    // object is created by it. When the COM object is created by Extern
    // for an exported interface, then the first message will create the
    // appropriate object using its interface.
    if (!o->localp.interface)
	o->localp = casycom_create_proxy (msg->h.interface, msg->h.dest);
    if (!o->pExtern)
	return casycom_error ("could not find outgoing connection for interface %s", casymsg_interface_name(msg));
    if (msg->h.src != o->localp.dest)	// Incoming message - forward to local
	return casymsg_forward (&o->localp, msg);
    // Outgoing message - queue in extern
    Msg* qm = casymsg_begin (&msg->h, msg->imethod, 0);	// Need to create a new message owned here
    *qm = *msg;
    msg->size = 0;	// The body is now owned by qm
    msg->body = NULL;
    Extern_queue_outgoing_message (o->pExtern, qm);
}

static void COMRelay_destroy (void* vo)
{
    COMRelay* o = vo;
    // The relay is destroyed when:
    // 1. The local object is destroyed. COM delete message is sent to the
    //    remote side as notification.
    // 2. The remote object is destroyed. The relay is marked unused in
    //    COMRelay_COM_delete and the extern pointer is reset to prevent
    //    further messages to remote object. Here, no message is sent.
    // 3. The Extern object is destroyed. pExtern is reset in
    //    COMRelay_object_destroyed, and no message is sent here.
    if (o->pExtern) {
	const Proxy failp = {	// The message comes from the real object
	    .interface = &i_COM,
	    .src = o->localp.dest,
	    .dest = o->localp.src
	};
	Extern_queue_outgoing_message (o->pExtern, PCOM_delete_message (&failp));
    }
    xfree (o);
}

static bool COMRelay_error (void* vo, oid_t eoid, const char* msg)
{
    COMRelay* o = vo;
    // An unhandled error in the local object is forwarded to the remote
    // object. At this point it will be considered handled. The remote
    // will decide whether to delete itself, which will propagate here.
    if (o->pExtern && eoid == o->localp.dest) {
	const Proxy failp = {	// The message comes from the real object
	    .interface = &i_COM,
	    .src = o->localp.dest,
	    .dest = o->localp.src
	};
	DEBUG_PRINTF ("[X] COMRelay forwarding error to extern creator\n");
	Extern_queue_outgoing_message (o->pExtern, PCOM_error_message (&failp, msg));
	return true;	// handled on the remote end.
    }
    // Errors occuring in the Extern object or elsewhere can not be handled
    // by forwarding, so fall back to default handling.
    return false;
}

static void COMRelay_object_destroyed (void* vo, oid_t oid)
{
    COMRelay* o = vo;
    if (oid == o->externid)	// Extern connection destroyed
	o->pExtern = NULL;	// no further messages are to be sent there.
    casycom_mark_unused (o);
}

//----------------------------------------------------------------------

static void COMRelay_COM_error (COMRelay* o, const char* error, const Msg* msg UNUSED)
{
    // COM_error is received for errors in the remote object. The remote
    // object is destroyed and COM_delete will shortly follow. Here, create
    // a local error and send it to the local object.
    casycom_error ("%s", error);
    casycom_forward_error (o->localp.dest, o->localp.src);
}

static void COMRelay_COM_delete (COMRelay* o, const Msg* msg UNUSED)
{
    // COM_delete indicates that the remote object has been destroyed.
    o->pExtern = NULL;		// No further messages are to be sent.
    casycom_mark_unused (o);	// The relay and local object are to be destroyed.
}

//----------------------------------------------------------------------

static const DCOM d_COMRelay_COM = {
    .interface = &i_COM,
    DMETHOD (COMRelay, COM_error),
    DMETHOD (COMRelay, COM_delete)
};
const Factory f_COMRelay = {
    .create		= COMRelay_create,
    .destroy		= COMRelay_destroy,
    .object_destroyed	= COMRelay_object_destroyed,
    .error		= COMRelay_error,
    .dtable		= { &d_COMRelay_COM, NULL }
};

//----------------------------------------------------------------------

/// Enables extern connections.
void casycom_enable_externs (void)
{
    casycom_register (&f_Timer);
    casycom_register (&f_Extern);
    casycom_register_default (&f_COMRelay);
}

//}}}-------------------------------------------------------------------
