// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "xcom.h"
#include "timer.h"
#include <fcntl.h>
#include <sys/socket.h>

//{{{ Local prototypes -------------------------------------------------

struct _SExtern;

static bool Extern_IsValidSocket (struct _SExtern* o);
static void COM_COM_Message (void* vo, SMsg* msg);
static void Extern_Extern_Close (struct _SExtern* o);
static void Extern_QueueMessage (struct _SExtern* o, SMsg* msg);
static void Extern_TimerR_Timer (struct _SExtern* o);
static void Extern_Reading (struct _SExtern* o);
static bool Extern_Writing (struct _SExtern* o);

//}}}-------------------------------------------------------------------
//{{{ PCOM

void PCOM_Error (const PProxy* pp, const char* error)
{
    SMsg* msg = casymsg_begin (pp, method_COM_Error, casystm_size_string(error));
    WStm os = casymsg_write (msg);
    casystm_write_string (&os, error);
    casymsg_end (msg);
}

void PCOM_Export (const PProxy* pp, const char* elist)
{
    SMsg* msg = casymsg_begin (pp, method_COM_Export, casystm_size_string(elist));
    WStm os = casymsg_write (msg);
    casystm_write_string (&os, elist);
    casymsg_end (msg);
}

void PCOM_Delete (const PProxy* pp)
{
    casymsg_end (casymsg_begin (pp, method_COM_Delete, 0));
}

static void PCOM_Dispatch (const DCOM* dtable, void* o, SMsg* msg)
{
    if (msg->h.interface != &i_COM)
	COM_COM_Message (o, msg);
    else if (msg->imethod == method_COM_Error) {
	RStm is = casymsg_read (msg);
	const char* error = casystm_read_string (&is);
	dtable->COM_Error (o, error, msg);
    } else if (msg->imethod == method_COM_Export) {
	RStm is = casymsg_read (msg);
	const char* elist = casystm_read_string (&is);
	dtable->COM_Export (o, elist, msg);
    } else if (msg->imethod == method_COM_Delete)
	dtable->COM_Delete (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_COM = {
    .name = "COM",
    .dispatch = PCOM_Dispatch,
    .method = { "Error\0s", "Export\0s", "Delete\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ COM object

typedef struct _SCOM {
    PProxy	reply;
} SCOM;

//----------------------------------------------------------------------

static void* COM_Create (const SMsg* msg)
{
    SCOM* o = (SCOM*) xalloc (sizeof(SCOM));
    o->reply = casycom_create_reply_proxy (&i_COM, msg);
    return o;
}

static void COM_Destroy (void* o)
{
    xfree (o);
}

static void COM_ObjectDestroyed (void* o, oid_t oid UNUSED)
{
    casycom_mark_unused (o);
}

static bool COM_Error (void* o, oid_t eoid UNUSED, const char* msg UNUSED)
{
    casycom_mark_unused (o);
    return false;
}

//----------------------------------------------------------------------

static void COM_COM_Error (SCOM* o, const char* error, SMsg* msg UNUSED)
{
    casycom_error ("%s", error);
    casycom_forward_error (o->reply.src, o->reply.dest);
}

static void COM_COM_Export (SCOM* o UNUSED, const char* elist UNUSED, const SMsg* msg UNUSED)
{
}

static void COM_COM_Delete (SCOM* o, const SMsg* msg UNUSED)
{
    casycom_mark_unused (o);
}

static void COM_COM_Message (void* vo, SMsg* msg)
{
    SCOM* o = (SCOM*) vo;
    if (msg->h.src == o->reply.dest)
	Extern_QueueMessage (NULL, msg);
    else
	casymsg_forward (&o->reply, msg);
}

//----------------------------------------------------------------------

static const DCOM d_COM_COM = {
    .interface = &i_COM,
    DMETHOD (COM, COM_Error),
    DMETHOD (COM, COM_Export),
    DMETHOD (COM, COM_Delete)
};
const SFactory f_COM = {
    .Create		= COM_Create,
    .Destroy		= COM_Destroy,
    .ObjectDestroyed	= COM_ObjectDestroyed,
    .Error		= COM_Error,
    .dtable		= { &d_COM_COM, NULL }
};

const SInterface* const eil_None[1] = { NULL };
static const SInterface* const* _casycom_ExportedInterfaces = eil_None;

// Registers the Extern server and the COM forwarder in addition to exported interfaces list
void casycom_register_externs (const SInterface* const* eo)
{
    _casycom_ExportedInterfaces = eo;
    casycom_register (&f_Extern);
    casycom_register_default (&f_COM);
}

//}}}-------------------------------------------------------------------
//{{{ PExtern

void PExtern_Open (const PProxy* pp, int fd, enum EExternType atype)
{
    assert (pp->interface == &i_Extern && "this proxy is for a different interface");
    SMsg* msg = casymsg_begin (pp, method_Extern_Open, 8);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casystm_write_uint32 (&os, atype);
    casymsg_end (msg);
}

void PExtern_Close (const PProxy* pp)
{
    casymsg_end (casymsg_begin (pp, method_Extern_Close, 0));
}

static void PExtern_Dispatch (const DExtern* dtable, void* o, const SMsg* msg)
{
    assert (dtable->interface == &i_Extern && "dispatch given dtable for a different interface");
    if (msg->imethod == method_Extern_Open) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	enum EExternType atype = casystm_read_uint32 (&is);
	dtable->Extern_Open (o, fd, atype, msg);
    } else if (msg->imethod == method_Extern_Close)
	dtable->Extern_Close (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_Extern = {
    .name = "Extern",
    .dispatch = PExtern_Dispatch,
    .method = { "Open\0iu", "Close\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ PExternR

void PExternR_Connected (const PProxy* pp)
{
    assert (pp->interface == &i_ExternR && "this proxy is for a different interface");
    casymsg_end (casymsg_begin (pp, method_ExternR_Connected, 0));
}

static void PExternR_Dispatch (const DExternR* dtable, void* o, const SMsg* msg)
{
    assert (dtable->interface == &i_ExternR && "dispatch given dtable for a different interface");
    if (msg->imethod == method_ExternR_Connected)
	dtable->ExternR_Connected (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_ExternR = {
    .name = "ExternR",
    .dispatch = PExternR_Dispatch,
    .method = { "Connected\0", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ Extern object

DECLARE_VECTOR_TYPE (ProxyVector, PProxy);
DECLARE_VECTOR_TYPE (RObjVector, SCOM*);
DECLARE_VECTOR_TYPE (SMsgVector, SMsg*);

typedef struct _SExtern {
    PProxy	reply;
    int		fd;
    PProxy	timer;
    bool	isClient;
    bool	canPassFd;
    RObjVector	outObjects;
    ProxyVector	inObjects;
    SMsgVector	outgoing;
    SMsgVector	incoming;
} SExtern;

static void* Extern_Create (const SMsg* msg)
{
    SExtern* o = (SExtern*) xalloc (sizeof(SExtern));
    o->reply = casycom_create_reply_proxy (&i_ExternR, msg);
    o->fd = -1;
    o->timer = casycom_create_proxy (&i_Timer, msg->h.dest);
    VECTOR_MEMBER_INIT (RObjVector, o->outObjects);
    VECTOR_MEMBER_INIT (ProxyVector, o->inObjects);
    VECTOR_MEMBER_INIT (SMsgVector, o->outgoing);
    VECTOR_MEMBER_INIT (SMsgVector, o->incoming);
    return o;
}

static void Extern_Destroy (void* vo)
{
    SExtern* o = (SExtern*) vo;
    if (o->fd >= 0)
	close (o->fd);
    vector_deallocate (&o->outObjects);
    vector_deallocate (&o->inObjects);
    for (size_t i = 0; i < o->outgoing.size; ++i)
	casymsg_free (o->outgoing.d[i]);
    vector_deallocate (&o->outgoing);
    for (size_t i = 0; i < o->incoming.size; ++i)
	casymsg_free (o->incoming.d[i]);
    vector_deallocate (&o->incoming);
}

static void Extern_Extern_Open (SExtern* o, int fd, enum EExternType atype)
{
    o->fd = fd;
    o->isClient = atype == EXTERN_CLIENT;
    if (!Extern_IsValidSocket (o)) {
	casycom_error ("incompatible socket type");
	return Extern_Extern_Close (o);
    }
    Extern_TimerR_Timer (o);
}

static void Extern_Extern_Close (SExtern* o)
{
    if (o->fd >= 0) {
	close (o->fd);
	o->fd = -1;
    }
    casycom_mark_unused (o);
}

static void Extern_QueueMessage (SExtern* o, SMsg* msg)
{
    if (!o)
	return;
    SMsg* qm = casymsg_begin (&msg->h, msg->imethod, 0);
    *qm = *msg;
    msg->size = 0;
    msg->body = NULL;
    vector_push_back (&o->outgoing, qm);
}

static bool Extern_IsValidSocket (SExtern* o)
{
    // The incoming socket must be a stream socket
    int v;
    socklen_t l = sizeof(v);
    if (getsockopt (o->fd, SOL_SOCKET, SO_TYPE, &v, &l) < 0 || v != SOCK_STREAM)
	return false;
    // It must be a listening server socket
    if (getsockopt (o->fd, SOL_SOCKET, SO_ACCEPTCONN, &v, &l) < 0 || v != true)
	return false;
    // And it must match the family (PF_LOCAL or PF_INET)
    struct sockaddr_storage ss;
    l = sizeof(ss);
    if (getsockname(o->fd, (struct sockaddr*) &ss, &l) < 0)
	return false;
    o->canPassFd = false;
    if (ss.ss_family == PF_LOCAL)
	o->canPassFd = true;
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

//----------------------------------------------------------------------

static void Extern_TimerR_Timer (SExtern* o)
{
    Extern_Reading (o);
    unsigned tcmd = WATCH_READ;
    if (Extern_Writing (o))
	tcmd |= WATCH_WRITE;
    PTimer_Watch (&o->timer, tcmd, o->fd, TIMER_NONE);
}

static void Extern_Reading (SExtern* o UNUSED)
{
}

static bool Extern_Writing (SExtern* o UNUSED)
{
    return false;
}

//----------------------------------------------------------------------

static const DExtern d_Extern_Extern = {
    .interface	= &i_Extern,
    DMETHOD (Extern, Extern_Open),
    DMETHOD (Extern, Extern_Close)
};
static const DTimerR d_Extern_TimerR = {
    .interface	= &i_TimerR,
    DMETHOD (Extern, TimerR_Timer)
};
const SFactory f_Extern = {
    .Create	= Extern_Create,
    .Destroy	= Extern_Destroy,
    .dtable	= { &d_Extern_Extern, &d_Extern_TimerR, NULL }
};

//}}}-------------------------------------------------------------------
