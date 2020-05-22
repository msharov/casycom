// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#include "xsrv.h"
#include "timer.h"
#include <sys/stat.h>
#include <sys/un.h>
#include <paths.h>
#include <fcntl.h>

//{{{ PExternServer ----------------------------------------------------

enum {
    method_ExternServer_open,
    method_ExternServer_close
};

void PExternServer_open (const Proxy* pp, int fd, const iid_t* exported_interfaces, bool close_when_empty)
{
    assert (pp->interface == &i_ExternServer && "this proxy is for a different interface");
    Msg* msg = casymsg_begin (pp, method_ExternServer_open, 8+4+1);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, exported_interfaces);
    casystm_write_int32 (&os, fd);
    casystm_write_bool (&os, close_when_empty);
    casymsg_end (msg);
}

void PExternServer_close (const Proxy* pp)
{
    assert (pp->interface == &i_ExternServer && "this proxy is for a different interface");
    casymsg_end (casymsg_begin (pp, method_ExternServer_close, 0));
}

static void PExternServer_dispatch (const DExternServer* dtable, void* o, const Msg* msg)
{
    assert (dtable->interface == &i_ExternServer && "dispatch given dtable for a different interface");
    if (msg->imethod == method_ExternServer_open) {
	RStm is = casymsg_read (msg);
	const iid_t* exported_interfaces = casystm_read_ptr (&is);
	int fd = casystm_read_int32 (&is);
	bool close_when_empty = casystm_read_bool (&is);
	dtable->ExternServer_open (o, fd, exported_interfaces, close_when_empty);
    } else if (msg->imethod == method_ExternServer_close)
	dtable->ExternServer_close (o);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_ExternServer = {
    .name = "ExternServer",
    .dispatch = PExternServer_dispatch,
    .method = { "open\0xib", "close\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ PExternServer_bind

typedef struct _LocalSocketPath {
    char*	path;
    int		fd;
} LocalSocketPath;
DECLARE_VECTOR_TYPE (LocalSocketPathVector, LocalSocketPath);

// Local sockets must be removed manually after closing
static void ExternServer_register_local_name (int fd, const char* path)
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

/// create server socket bound to the given address
int PExternServer_bind (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* exported_interfaces)
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
	ExternServer_register_local_name (fd, ((const struct sockaddr_un*)addr)->sun_path);
    PExternServer_open (pp, fd, exported_interfaces, false);
    return fd;
}

/// create local socket with given path
int PExternServer_bind_local (const Proxy* pp, const char* path, const iid_t* exported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if ((int) ARRAY_SIZE(addr.sun_path) <= snprintf (ARRAY_BLOCK(addr.sun_path), "%s", path)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    int fd = PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
    if (0 > chmod (addr.sun_path, DEFFILEMODE))
	DEBUG_PRINTF ("[E] Failed to change socket permissions: %s\n", strerror(errno));
    return fd;
}

/// create local socket of the given name in the system standard location for such
int PExternServer_bind_system_local (const Proxy* pp, const char* sockname, const iid_t* exported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    if ((int) ARRAY_SIZE(addr.sun_path) <= snprintf (ARRAY_BLOCK(addr.sun_path), _PATH_VARRUN "%s", sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    int fd = PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
    if (0 > chmod (addr.sun_path, DEFFILEMODE))
	DEBUG_PRINTF ("[E] Failed to change socket permissions: %s\n", strerror(errno));
    return fd;
}

/// create local socket of the given name in the user standard location for such
int PExternServer_bind_user_local (const Proxy* pp, const char* sockname, const iid_t* exported_interfaces)
{
    struct sockaddr_un addr;
    addr.sun_family = PF_LOCAL;
    const char* runtimeDir = getenv ("XDG_RUNTIME_DIR");
    if (!runtimeDir)
	runtimeDir = _PATH_TMP;
    if ((int) ARRAY_SIZE(addr.sun_path) <= snprintf (ARRAY_BLOCK(addr.sun_path), "%s/%s", runtimeDir, sockname)) {
	errno = ENAMETOOLONG;
	return -1;
    }
    DEBUG_PRINTF ("[X] Creating server socket %s\n", addr.sun_path);
    int fd = PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
    if (0 > chmod (addr.sun_path, S_IRUSR| S_IWUSR))
	DEBUG_PRINTF ("[E] Failed to change socket permissions: %s\n", strerror(errno));
    return fd;
}

//}}}-------------------------------------------------------------------
//{{{ ExternServer

DECLARE_VECTOR_TYPE (ProxyVector, Proxy);

typedef struct _ExternServer {
    Proxy		reply;
    int			fd;
    Proxy		timer;
    bool		close_when_empty;
    const iid_t*	exported_interfaces;
    ProxyVector		pconn;
} ExternServer;

static void* ExternServer_create (const Msg* msg)
{
    ExternServer* o = xalloc (sizeof(ExternServer));
    o->reply = casycom_create_reply_proxy (&i_ExternR, msg);
    o->fd = -1;
    o->timer = casycom_create_proxy (&i_Timer, o->reply.src);
    VECTOR_MEMBER_INIT (ProxyVector, o->pconn);
    return o;
}

static void ExternServer_destroy (void* vo)
{
    ExternServer* o = vo;
    ExternServer_register_local_name (o->fd, NULL);
    free (o);
}

static bool ExternServer_error (void* vo, oid_t eoid, const char* msg)
{
    ExternServer* o = vo;
    for (size_t i = 0; i < o->pconn.size; ++i) {
	if (o->pconn.d[i].dest == eoid) {
	    casycom_log (LOG_ERR, "%s", msg);
	    return true; // error in accepted socket. Handle by logging the error and removing record in object_destroyed.
	}
    }
    return false;
}

static void ExternServer_object_destroyed (void* vo, oid_t oid)
{
    DEBUG_PRINTF ("[X] Client connection %hu dropped\n", oid);
    ExternServer* o = vo;
    for (size_t i = 0; i < o->pconn.size; ++i) {
	if (o->pconn.d[i].dest == oid) {
	    casycom_destroy_proxy (&o->pconn.d[i]);
	    vector_erase (&o->pconn, i--);
	}
    }
    if (!o->pconn.size && o->close_when_empty)
	casycom_mark_unused (o);
}

static void ExternServer_TimerR_timer (ExternServer* o, int fd, const Msg* msg UNUSED)
{
    assert (fd == o->fd);
    for (int cfd; 0 <= (cfd = accept (fd, NULL, NULL));) {
	Proxy* pconn = vector_emplace_back (&o->pconn);
	*pconn = casycom_create_proxy (&i_Extern, o->reply.src);
	DEBUG_PRINTF ("[X] Client connection accepted on fd %d\n", cfd);
	PExtern_open (pconn, cfd, EXTERN_SERVER, NULL, o->exported_interfaces);
    }
    if (errno == EAGAIN) {
	DEBUG_PRINTF ("[X] Resuming wait on fd %d\n", fd);
	PTimer_wait_read (&o->timer, fd);
    } else {
	DEBUG_PRINTF ("[X] Accept failed with error %s\n", strerror(errno));
	casycom_error ("accept: %s", strerror(errno));
	casycom_mark_unused (o);
    }
}

static void ExternServer_ExternServer_open (ExternServer* o, int fd, const iid_t* exported_interfaces, bool close_when_empty)
{
    assert (o->fd == -1 && "each ExternServer instance can only listen to one socket");
    o->fd = fd;
    o->exported_interfaces = exported_interfaces;
    o->close_when_empty = close_when_empty;
    fcntl (o->fd, F_SETFL, O_NONBLOCK| fcntl (o->fd, F_GETFL));
    ExternServer_TimerR_timer (o, fd, NULL);
}

static void ExternServer_ExternServer_close (ExternServer* o)
{
    casycom_mark_unused (o);
}

static void ExternServer_ExternR_connected (ExternServer* o, const ExternInfo* einfo)
{
    PExternR_connected (&o->reply, einfo);
}

static const DExternServer d_ExternServer_ExternServer = {
    .interface	= &i_ExternServer,
    DMETHOD (ExternServer, ExternServer_open),
    DMETHOD (ExternServer, ExternServer_close)
};
static const DTimerR d_ExternServer_TimerR = {
    .interface	= &i_TimerR,
    DMETHOD (ExternServer, TimerR_timer)
};
static const DExternR d_ExternServer_ExternR = {
    .interface	= &i_ExternR,
    DMETHOD (ExternServer, ExternR_connected)
};
const Factory f_ExternServer = {
    .create	= ExternServer_create,
    .destroy	= ExternServer_destroy,
    .error	= ExternServer_error,
    .object_destroyed = ExternServer_object_destroyed,
    .dtable	= {
	&d_ExternServer_ExternServer,
	&d_ExternServer_TimerR,
	&d_ExternServer_ExternR,
	NULL
    }
};

//}}}-------------------------------------------------------------------
