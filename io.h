// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#pragma once
#include "main.h"
#include "vector.h"
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// Abstract async IO interface to read and write CharVector buffers

typedef void (*MFN_IO_read)(void* o, CharVector* d);
typedef void (*MFN_IO_write)(void* o, CharVector* d);
typedef struct _DIO {
    const Interface*	interface;
    MFN_IO_read		IO_read;
    MFN_IO_write	IO_write;
} DIO;

// read some data into the given buffer.
void PIO_read (const Proxy* pp, CharVector* d) noexcept;
// Write all the data in the given buffer.
void PIO_write (const Proxy* pp, CharVector* d) noexcept;

extern const Interface i_IO;

//----------------------------------------------------------------------
// Notifications from the IO interface

typedef void (*MFN_IOR_read)(void* o, CharVector* d);
typedef void (*MFN_IOR_written)(void* o, CharVector* d);
typedef struct _DIOR {
    const Interface*	interface;
    MFN_IOR_read	IOR_read;
    MFN_IOR_written	IOR_written;
} DIOR;

// Sent when some data is read into the buffer
void PIOR_read (const Proxy* pp, CharVector* d) noexcept;
// Sent when the entire requested block has been written
void PIOR_written (const Proxy* pp, CharVector* d) noexcept;

extern const Interface i_IOR;

//----------------------------------------------------------------------
// Implementation of the above IO interface for file descriptors

typedef void (*MFN_FdIO_attach)(void* o, int fd);
typedef struct _DFdIO {
    const Interface*	interface;
    MFN_FdIO_attach	FdIO_attach;
} DFdIO;

// attach to the given file descriptor. The object will not close it.
void PFdIO_attach (const Proxy* pp, int fd) noexcept;

extern const Interface i_FdIO;

//----------------------------------------------------------------------

extern const Factory f_FdIO;

//----------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
