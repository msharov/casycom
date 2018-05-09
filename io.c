// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "io.h"
#include "timer.h"

//{{{ IO interface -----------------------------------------------------

enum { method_IO_Read, method_IO_Write };

void PIO_Read (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IO_Read, 8);
    msg->h.interface = &i_IO;
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

void PIO_Write (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IO_Write, 8);
    msg->h.interface = &i_IO;
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

static void IO_Dispatch (const DIO* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_IO_Read) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	dtable->IO_Read (o, d);
    } else if (msg->imethod == method_IO_Write) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	dtable->IO_Write (o, d);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_IO = {
    .name       = "IO",
    .dispatch   = IO_Dispatch,
    .method     = { "Read\0x", "Write\0x", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ IOR interface

enum { method_IOR_Read, method_IOR_Written };

void PIOR_Read (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IOR_Read, 8);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

void PIOR_Written (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IOR_Written, 8);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

static void IOR_Dispatch (const DIOR* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_IOR_Read) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	if (dtable->IOR_Read)
	    dtable->IOR_Read (o, d);
    } else if (msg->imethod == method_IOR_Written) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	if (dtable->IOR_Written)
	    dtable->IOR_Written (o, d);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_IOR = {
    .name       = "IOR",
    .dispatch   = IOR_Dispatch,
    .method     = { "Read\0x", "Written\0x", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ FdIO interface

enum { method_FdIO_Attach };

void PFdIO_Attach (const Proxy* pp, int fd)
{
    Msg* msg = casymsg_begin (pp, method_FdIO_Attach, 4);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

static void FdIO_Dispatch (const DFdIO* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_FdIO_Attach) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	dtable->FdIO_Attach (o, fd);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_FdIO = {
    .name       = "FdIO",
    .dispatch   = FdIO_Dispatch,
    .method     = { "Attach\0i", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ FdIO object

typedef struct _FdIO {
    Proxy	reply;
    int		fd;
    Proxy	timer;
    bool	eof;
    CharVector*	rbuf;
    CharVector*	wbuf;
} FdIO;

const Factory f_FdIO;

static void* FdIO_Create (const Msg* msg)
{
    FdIO* po = xalloc (sizeof(FdIO));
    po->fd = -1;
    po->reply = casycom_create_reply_proxy (&i_IOR, msg);
    po->timer = casycom_create_proxy (&i_Timer, msg->h.dest);
    return po;
}

static void FdIO_FdIO_Attach (FdIO* o, int fd)
{
    o->fd = fd;
}

static void FdIO_TimerR_Timer (FdIO* o, int fd UNUSED, const Msg* msg UNUSED)
{
    enum ETimerWatchCmd ccmd = 0;
    if (o->rbuf) {
	if (!o->eof) {
	    size_t rbufsz = o->rbuf->size;
	    for (size_t btr; 0 < (btr = o->rbuf->allocated - o->rbuf->size);) {
		ssize_t r = read (o->fd, &o->rbuf->d[o->rbuf->size], btr);
		if (r <= 0) {
		    if (!r || errno == ECONNRESET) {
			o->eof = true;
			casycom_mark_unused (o);
		    } else if (errno == EAGAIN)
			ccmd |= WATCH_READ;
		    else if (errno == EINTR)
			continue;
		    else
			casycom_error ("read: %s", strerror(errno));
		    break;
		}
		o->rbuf->size += r;
	    }
	    if (rbufsz != o->rbuf->size || o->eof)
		PIOR_Read (&o->reply, o->rbuf);
	}
	if (o->eof)
	    PIOR_Read (&o->reply, o->rbuf = NULL);
    }
    if (o->wbuf) {
	if (!o->eof) {
	    size_t wbufsize = o->wbuf->size;
	    while (o->wbuf->size) {
		ssize_t r = write (o->fd, o->wbuf->d, o->wbuf->size);
		if (r <= 0) {
		    if (!r || errno == ECONNRESET) {
			o->eof = true;
			casycom_mark_unused (o);
		    } else if (errno == EAGAIN)
			ccmd |= WATCH_WRITE;
		    else if (errno == EINTR)
			continue;
		    else
			casycom_error ("write: %s", strerror(errno));
		    break;
		}
		vector_erase_n (o->wbuf, 0, r);
	    }
	    if (wbufsize != o->wbuf->size && (!o->wbuf->size || o->eof))
		PIOR_Written (&o->reply, o->wbuf);
	    if (!o->wbuf->size)	// stop writing when done
		o->wbuf = NULL;
	}
	if (o->eof)
	    PIOR_Written (&o->reply, o->wbuf = NULL);
    }
    if (ccmd)
	PTimer_Watch (&o->timer, ccmd, o->fd, TIMER_NONE);
}

static void FdIO_IO_Read (FdIO* o, CharVector* d)
{
    o->rbuf = d;
    FdIO_TimerR_Timer (o, o->fd, NULL);
}

static void FdIO_IO_Write (FdIO* o, CharVector* d)
{
    o->wbuf = d;
    FdIO_TimerR_Timer (o, o->fd, NULL);
}

static const DFdIO d_FdIO_FdIO = {
    .interface = &i_FdIO,
    DMETHOD (FdIO, FdIO_Attach)
};
static const DIO d_FdIO_IO = {
    .interface = &i_IO,
    DMETHOD (FdIO, IO_Read),
    DMETHOD (FdIO, IO_Write)
};
static const DTimerR d_FdIO_TimerR = {
    .interface = &i_TimerR,
    DMETHOD (FdIO, TimerR_Timer)
};
const Factory f_FdIO = {
    .Create = FdIO_Create,
    .dtable = { &d_FdIO_FdIO, &d_FdIO_IO, &d_FdIO_TimerR, NULL }
};

//}}}-------------------------------------------------------------------
