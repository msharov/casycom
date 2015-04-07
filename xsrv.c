// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "xsrv.h"
#include "timer.h"
#include <netinet/ip.h>
#include <sys/un.h>
#include <paths.h>

//{{{ PExternServer ----------------------------------------------------

enum {
    method_ExternServer_Open,
    method_ExternServer_Close
};

void PExternServer_Open (const Proxy* pp, int fd, const iid_t* exportedInterfaces)
{
    assert (pp->interface == &i_ExternServer && "this proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_ExternServer_Open, 8+4);
    WStm os = casymsg_write (msg);
    casystm_write_uint64 (&os, (uintptr_t) exportedInterfaces);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

void PExternServer_Close (const Proxy* pp)
{
    assert (pp->interface == &i_ExternServer && "this proxy is for a different interface");
    casymsg_end (casymsg_begin (pp, method_ExternServer_Close, 0));
}

static void PExternServer_Dispatch (const DExternServer* dtable, void* o, const Msg* msg)
{
    assert (dtable->interface == &i_ExternServer && "dispatch given dtable for a different interface");
    if (msg->imethod == method_ExternServer_Open) {
	RStm is = casymsg_read (msg);
	const iid_t* exportedInterfaces = (const iid_t*) casystm_read_uint64 (&is);
	int fd = casystm_read_int32 (&is);
	dtable->ExternServer_Open (o, fd, exportedInterfaces);
    } else if (msg->imethod == method_ExternServer_Close)
	dtable->ExternServer_Close (o);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_ExternServer = {
    .name = "ExternServer",
    .dispatch = PExternServer_Dispatch,
    .method = { "Open\0xi", "Close\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ PExternServer_Bind

typedef struct _LocalSocketPath {
    char*	path;
    int		fd;
} LocalSocketPath;
DECLARE_VECTOR_TYPE (LocalSocketPathVector, LocalSocketPath);

// Local sockets must be removed manually after closing
static void ExternServer_RegisterLocalName (int fd, const char* path)
{
    static VECTOR (LocalSocketPathVector, pathvec);
    if (path) {
	LocalSocketPath* p = vector_emplace_back (&pathvec);
	p->path = strdup (path);
	p->fd = fd;
    } else {
	for (size_t i = 0; i < pathvec.size; ++i) {
	    if (pathvec.d[i].fd == fd) {
		unlink (pathvec.d[i].path);
		free (pathvec.d[i].path);
		vector_erase (&pathvec, i--);
	    }
	}
    }
}

/// Create server socket bound to the given address
int PExternServer_Bind (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* exportedInterfaces)
{
    int fd = socket (addr->sa_family, SOCK_STREAM| SOCK_NONBLOCK| SOCK_CLOEXEC, IPPROTO_IP);
    if (fd < 0)
	return fd;
    if (0 > bind (fd, addr, addrlen) && errno != EINPROGRESS) {
	DEBUG_PRINTF ("[E] Failed to bind to socket: %s\n", strerror(errno));
	close (fd);
	return -1;
    }
    if (0 > listen (fd, SOMAXCONN)) {
	DEBUG_PRINTF ("[E] Failed to listen to socket: %s\n", strerror(errno));
	close (fd);
	return -1;
    }
    if (addr->sa_family == PF_LOCAL)
	ExternServer_RegisterLocalName (fd, ((const struct sockaddr_un*)addr)->sun_path);
    PExternServer_Open (pp, fd, exportedInterfaces);
    return fd;
}

/// Create local socket with given path
int PExternServer_BindLocal (const Proxy* pp, const char* path, const iid_t* exportedInterfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if ((int) ArraySize(addr.sun_path) <= snprintf (addr.sun_path, ArraySize(addr.sun_path), "%s", path)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

/// Create local socket of the given name in the system standard location for such
int PExternServer_BindSystemLocal (const Proxy* pp, const char* sockname, const iid_t* exportedInterfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if ((int) ArraySize(addr.sun_path) <= snprintf (addr.sun_path, ArraySize(addr.sun_path), _PATH_VARRUN "%s", sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

/// Create local socket of the given name in the user standard location for such
int PExternServer_BindUserLocal (const Proxy* pp, const char* sockname, const iid_t* exportedInterfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    const char* runtimeDir = getenv ("XDG_RUNTIME_DIR");
    if (!runtimeDir)
	runtimeDir = _PATH_TMP;
    if ((int) ArraySize(addr.sun_path) <= snprintf (addr.sun_path, ArraySize(addr.sun_path), "%s/%s", runtimeDir, sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

//}}}-------------------------------------------------------------------
//{{{ ExternServer

DECLARE_VECTOR_TYPE (ProxyVector, Proxy);

typedef struct _ExternServer {
    Proxy		reply;
    int			fd;
    Proxy		timer;
    const iid_t*	exportedInterfaces;
    ProxyVector		pconn;
} ExternServer;

static void* ExternServer_Create (const Msg* msg)
{
    ExternServer* o = (ExternServer*) xalloc (sizeof(ExternServer));
    o->reply = casycom_create_reply_proxy (&i_ExternR, msg);
    o->fd = -1;
    o->timer = casycom_create_proxy (&i_Timer, o->reply.src);
    VECTOR_MEMBER_INIT (ProxyVector, o->pconn);
    return o;
}

static void ExternServer_Destroy (void* vo)
{
    ExternServer* o = (ExternServer*) vo;
    ExternServer_RegisterLocalName (o->fd, NULL);
    free (o);
}

static bool ExternServer_Error (void* vo, oid_t eoid, const char* msg)
{
    ExternServer* o = (ExternServer*) vo;
    for (size_t i = 0; i < o->pconn.size; ++i) {
	if (o->pconn.d[i].dest == eoid) {
	    casycom_log (LOG_ERR, "%s", msg);
	    return true; // Error in accepted socket. Handle by logging the error and removing record in ObjectDestroyed.
	}
    }
    return false;
}

static void ExternServer_ObjectDestroyed (void* vo, oid_t oid)
{
    DEBUG_PRINTF ("[X] Client connection %hu dropped\n", oid);
    ExternServer* o = (ExternServer*) vo;
    for (size_t i = 0; i < o->pconn.size; ++i) {
	if (o->pconn.d[i].dest == oid) {
	    casycom_destroy_proxy (&o->pconn.d[i]);
	    vector_erase (&o->pconn, i--);
	}
    }
}

static void ExternServer_TimerR_Timer (ExternServer* o, int fd)
{
    assert (fd == o->fd);
    for (int cfd; 0 <= (cfd = accept (fd, NULL, NULL));) {
	Proxy* pconn = vector_emplace_back (&o->pconn);
	*pconn = casycom_create_proxy (&i_Extern, o->reply.src);
	DEBUG_PRINTF ("[X] Client connection accepted on fd %d\n", cfd);
	PExtern_Open (pconn, cfd, EXTERN_SERVER, NULL, o->exportedInterfaces);
    }
    if (errno == EAGAIN) {
	DEBUG_PRINTF ("[X] Resuming wait on fd %d\n", fd);
	PTimer_WaitRead (&o->timer, fd);
    } else {
	DEBUG_PRINTF ("[X] Accept failed with error %s\n", strerror(errno));
	casycom_error ("accept: %s", strerror(errno));
	casycom_mark_unused (o);
    }
}

static void ExternServer_ExternServer_Open (ExternServer* o, int fd, const iid_t* exportedInterfaces)
{
    assert (o->fd == -1 && "each ExternServer instance can only listen to one socket");
    o->fd = fd;
    o->exportedInterfaces = exportedInterfaces;
    ExternServer_TimerR_Timer (o, fd);
}

static void ExternServer_ExternServer_Close (ExternServer* o)
{
    casycom_mark_unused (o);
}

static void ExternServer_ExternR_Connected (ExternServer* o, const ExternInfo* einfo)
{
    PExternR_Connected (&o->reply, einfo);
}

static const DExternServer d_ExternServer_ExternServer = {
    .interface	= &i_ExternServer,
    DMETHOD (ExternServer, ExternServer_Open),
    DMETHOD (ExternServer, ExternServer_Close)
};
static const DTimerR d_ExternServer_TimerR = {
    .interface	= &i_TimerR,
    DMETHOD (ExternServer, TimerR_Timer)
};
static const DExternR d_ExternServer_ExternR = {
    .interface	= &i_ExternR,
    DMETHOD (ExternServer, ExternR_Connected)
};
const Factory f_ExternServer = {
    .Create	= ExternServer_Create,
    .Destroy	= ExternServer_Destroy,
    .Error	= ExternServer_Error,
    .ObjectDestroyed = ExternServer_ObjectDestroyed,
    .dtable	= {
	&d_ExternServer_ExternServer,
	&d_ExternServer_TimerR,
	&d_ExternServer_ExternR,
	NULL
    }
};

//}}}-------------------------------------------------------------------
