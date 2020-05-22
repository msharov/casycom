// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#include "io.h"
#include "timer.h"

//{{{ IO interface -----------------------------------------------------

enum { method_IO_read, method_IO_write };

void PIO_read (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IO_read, 8);
    msg->h.interface = &i_IO;
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

void PIO_write (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IO_write, 8);
    msg->h.interface = &i_IO;
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

static void IO_dispatch (const DIO* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_IO_read) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	dtable->IO_read (o, d);
    } else if (msg->imethod == method_IO_write) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	dtable->IO_write (o, d);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_IO = {
    .name       = "IO",
    .dispatch   = IO_dispatch,
    .method     = { "read\0x", "write\0x", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ IOR interface

enum { method_IOR_read, method_IOR_written };

void PIOR_read (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IOR_read, 8);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

void PIOR_written (const Proxy* pp, CharVector* d)
{
    Msg* msg = casymsg_begin (pp, method_IOR_written, 8);
    WStm os = casymsg_write (msg);
    casystm_write_ptr (&os, d);
    casymsg_end (msg);
}

static void IOR_dispatch (const DIOR* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_IOR_read) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	if (dtable->IOR_read)
	    dtable->IOR_read (o, d);
    } else if (msg->imethod == method_IOR_written) {
	RStm is = casymsg_read (msg);
	CharVector* d = casystm_read_ptr (&is);
	if (dtable->IOR_written)
	    dtable->IOR_written (o, d);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_IOR = {
    .name       = "IOR",
    .dispatch   = IOR_dispatch,
    .method     = { "read\0x", "written\0x", NULL }
};

//}}}-------------------------------------------------------------------
//{{{ FdIO interface

enum { method_FdIO_attach };

void PFdIO_attach (const Proxy* pp, int fd)
{
    Msg* msg = casymsg_begin (pp, method_FdIO_attach, 4);
    WStm os = casymsg_write (msg);
    casystm_write_int32 (&os, fd);
    casymsg_end (msg);
}

static void FdIO_dispatch (const DFdIO* dtable, void* o, const Msg* msg)
{
    if (msg->imethod == method_FdIO_attach) {
	RStm is = casymsg_read (msg);
	int fd = casystm_read_int32 (&is);
	dtable->FdIO_attach (o, fd);
    } else
	casymsg_default_dispatch (dtable, o, msg);
}

const Interface i_FdIO = {
    .name       = "FdIO",
    .dispatch   = FdIO_dispatch,
    .method     = { "attach\0i", NULL }
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

static void* FdIO_create (const Msg* msg)
{
    FdIO* po = xalloc (sizeof(FdIO));
    po->fd = -1;
    po->reply = casycom_create_reply_proxy (&i_IOR, msg);
    po->timer = casycom_create_proxy (&i_Timer, msg->h.dest);
    return po;
}

static void FdIO_FdIO_attach (FdIO* o, int fd)
{
    o->fd = fd;
}

static void FdIO_TimerR_timer (FdIO* o, int fd UNUSED, const Msg* msg UNUSED)
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
		PIOR_read (&o->reply, o->rbuf);
	}
	if (o->eof)
	    PIOR_read (&o->reply, o->rbuf = NULL);
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
		PIOR_written (&o->reply, o->wbuf);
	    if (!o->wbuf->size)	// stop writing when done
		o->wbuf = NULL;
	}
	if (o->eof)
	    PIOR_written (&o->reply, o->wbuf = NULL);
    }
    if (ccmd)
	PTimer_watch (&o->timer, ccmd, o->fd, TIMER_NONE);
}

static void FdIO_IO_read (FdIO* o, CharVector* d)
{
    o->rbuf = d;
    FdIO_TimerR_timer (o, o->fd, NULL);
}

static void FdIO_IO_write (FdIO* o, CharVector* d)
{
    o->wbuf = d;
    FdIO_TimerR_timer (o, o->fd, NULL);
}

static const DFdIO d_FdIO_FdIO = {
    .interface = &i_FdIO,
    DMETHOD (FdIO, FdIO_attach)
};
static const DIO d_FdIO_IO = {
    .interface = &i_IO,
    DMETHOD (FdIO, IO_read),
    DMETHOD (FdIO, IO_write)
};
static const DTimerR d_FdIO_TimerR = {
    .interface = &i_TimerR,
    DMETHOD (FdIO, TimerR_timer)
};
const Factory f_FdIO = {
    .create = FdIO_create,
    .dtable = { &d_FdIO_FdIO, &d_FdIO_IO, &d_FdIO_TimerR, NULL }
};

//}}}-------------------------------------------------------------------
