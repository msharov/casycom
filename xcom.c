// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "xcom.h"

//----------------------------------------------------------------------
// COM proxy

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

static void PCOM_Dispatch (const DCOM* dtable, void* vo, SMsg* msg)
{
    SCOM* o = (SCOM*) vo;
    if (msg->h.interface != &i_COM)
	COM_COM_Message (o, msg);
    else if (msg->imethod == method_COM_Error) {
	RStm is = casymsg_read (msg);
	const char* error = casystm_read_string (&is);
	COM_COM_Error (o, error, msg);
    } else if (msg->imethod == method_COM_Export) {
	RStm is = casymsg_read (msg);
	const char* elist = casystm_read_string (&is);
	COM_COM_Export (o, elist, msg);
    } else if (msg->imethod == method_COM_Delete)
	COM_COM_Delete (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_COM = {
    .name = "COM",
    .dispatch = PCOM_Dispatch,
    .method = { "Error\0s", "Export\0s", "Delete\0", NULL }
};

//----------------------------------------------------------------------
// COM object

void* COM_Create (const SMsg* msg)
{
    SCOM* o = (SCOM*) xalloc (sizeof(SCOM));
    o->reply = casycom_create_reply_proxy (&i_COM, msg);
    o->forwd = casycom_create_proxy (&i_COM, msg->h.dest);
    return o;
}

void COM_Destroy (void* o)
{
    xfree (o);
}

void COM_ObjectDestroyed (void* o, oid_t oid UNUSED)
{
    casycom_mark_unused (o);
}

bool COM_Error (void* o, oid_t eoid UNUSED, const char* msg UNUSED)
{
    casycom_mark_unused (o);
    return false;
}

const SObject o_COM = {
    .Create		= COM_Create,
    .Destroy		= COM_Destroy,
    .ObjectDestroyed	= COM_ObjectDestroyed,
    .Error		= COM_Error,
    .interface		= { &i_COM, NULL }
};

//----------------------------------------------------------------------

void COM_COM_Error (SCOM* o, const char* error UNUSED, SMsg* msg)
{
    if (msg->h.src == o->reply.dest)
	casymsg_forward (&o->forwd, msg);
    else if (msg->h.src == o->forwd.dest)
	casymsg_forward (&o->forwd, msg);
    else
	assert (!"internal error: COM object receives message from unexpected source");
}

void COM_COM_Export (SCOM* o UNUSED, const char* elist UNUSED, const SMsg* msg UNUSED)
{
}

void COM_COM_Delete (SCOM* o UNUSED, const SMsg* msg UNUSED)
{
}

void COM_COM_Message (SCOM* o, SMsg* msg)
{
    if (msg->h.src == o->reply.dest)
	casymsg_forward (&o->forwd, msg);
    else if (msg->h.src == o->forwd.dest)
	casymsg_forward (&o->forwd, msg);
    else
	assert (!"internal error: COM object receives message from unexpected source");
}

//----------------------------------------------------------------------
// Extern

void PExtern_Attach (const PProxy* pp, int fd, enum EExternAttachType atype)
{
    SMsg* msg = casymsg_begin (pp, method_Extern_Attach, 8);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casystm_write_uint32 (&os, atype);
    casymsg_end (msg);
}

void PExtern_Detach (const PProxy* pp)
{
    casymsg_end (casymsg_begin (pp, method_Extern_Detach, 0));
}

static void PExtern_Dispatch (const DExtern* dtable, void* o, const SMsg* msg)
{
    if (msg->imethod == method_Extern_Attach) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	enum EExternAttachType atype = casystm_read_uint32 (&is);
	dtable->Extern_Attach (o, fd, atype, msg);
    } else if (msg->imethod == method_Extern_Detach)
	dtable->Extern_Detach (o, msg);
    else
	casymsg_default_dispatch (dtable, o, msg);
}

const SInterface i_Extern = {
    .name = "Extern",
    .dispatch = PExtern_Dispatch,
    .method = { "Attach\0iu", "Detach\0", NULL }
};

//----------------------------------------------------------------------
// Extern object

DECLARE_VECTOR_TYPE (ProxyVector, PProxy);
DECLARE_VECTOR_TYPE (SMsgVector, SMsg*);

typedef struct _SExtern {
    int		fd;
    bool	isClient;
    bool	canPassFd;
    ProxyVector	ownedObjects;
    SMsgVector	outgoing;
    SMsgVector	incoming;
} SExtern;

static void* Extern_Create (const SMsg* msg UNUSED)
{
    SExtern* o = (SExtern*) xalloc (sizeof(SExtern));
    o->fd = -1;
    VECTOR_MEMBER_INIT (ProxyVector, o->ownedObjects);
    VECTOR_MEMBER_INIT (SMsgVector, o->outgoing);
    VECTOR_MEMBER_INIT (SMsgVector, o->incoming);
    return o;
}

static void Extern_Destroy (void* vo)
{
    SExtern* o = (SExtern*) vo;
    if (o->fd >= 0)
	close (o->fd);
    vector_deallocate (&o->ownedObjects);
    for (size_t i = 0; i < o->outgoing.size; ++i)
	casymsg_free (o->outgoing.d[i]);
    vector_deallocate (&o->outgoing);
    for (size_t i = 0; i < o->incoming.size; ++i)
	casymsg_free (o->incoming.d[i]);
    vector_deallocate (&o->incoming);
}

static void ExternObject_Extern_Attach (SExtern* o UNUSED, int fd UNUSED, enum EExternAttachType atype UNUSED, const SMsg* msg UNUSED)
{
}

static void ExternObject_Extern_Detach (SExtern* o UNUSED, const SMsg* msg UNUSED)
{
}

static const DExtern d_ExternObject_Extern = {
    .interface	= &i_Extern,
    DMETHOD (ExternObject, Extern_Attach),
    DMETHOD (ExternObject, Extern_Detach)
};
static const SObject o_ExternObject = {
    .Create	= Extern_Create,
    .Destroy	= Extern_Destroy,
    .interface	= { &d_ExternObject_Extern, NULL }
};
