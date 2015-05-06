// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"
#ifdef __cplusplus
extern "C" {
#endif

//----------------------------------------------------------------------
// Abstract async IO interface to read and write CharVector buffers

typedef void (*MFN_IO_Read)(void* o, CharVector* d);
typedef void (*MFN_IO_Write)(void* o, CharVector* d);
typedef struct _DIO {
    const Interface*	interface;
    MFN_IO_Read		IO_Read;
    MFN_IO_Write	IO_Write;
} DIO;

// Read some data into the given buffer.
void PIO_Read (const Proxy* pp, CharVector* d) noexcept;
// Write all the data in the given buffer.
void PIO_Write (const Proxy* pp, CharVector* d) noexcept;

extern const Interface i_IO;

//----------------------------------------------------------------------
// Notifications from the IO interface

typedef void (*MFN_IOR_Read)(void* o, CharVector* d);
typedef void (*MFN_IOR_Written)(void* o, CharVector* d);
typedef struct _DIOR {
    const Interface*	interface;
    MFN_IOR_Read	IOR_Read;
    MFN_IOR_Written	IOR_Written;
} DIOR;

// Sent when some data is read into the buffer
void PIOR_Read (const Proxy* pp, CharVector* d) noexcept;
// Sent when the entire requested block has been written
void PIOR_Written (const Proxy* pp, CharVector* d) noexcept;

extern const Interface i_IOR;

//----------------------------------------------------------------------
// Implementation of the above IO interface for file descriptors

typedef void (*MFN_FdIO_Attach)(void* o, int fd);
typedef struct _DFdIO {
    const Interface*	interface;
    MFN_FdIO_Attach	FdIO_Attach;
} DFdIO;

// Attach to the given file descriptor. The object will not close it.
void PFdIO_Attach (const Proxy* pp, int fd) noexcept;

extern const Interface i_FdIO;

//----------------------------------------------------------------------

extern const Factory f_FdIO;

//----------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
