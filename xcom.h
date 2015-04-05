// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __cplusplus
extern "C" {
#endif

//{{{ Extern -----------------------------------------------------------

enum EExternMethod {
    method_Extern_Open,
    method_Extern_Close,
    method_Extern_N
};
enum EExternType {
    EXTERN_CLIENT,
    EXTERN_SERVER
};
typedef void (*MFN_Extern_Open)(void* vo, int fd, enum EExternType atype, const iid_t* importedInterfaces, const iid_t* exportedInterfaces);
typedef void (*MFN_Extern_Close)(void* vo);
typedef struct _DExtern {
    iid_t		interface;
    MFN_Extern_Open	Extern_Open;
    MFN_Extern_Close	Extern_Close;
} DExtern;

void PExtern_Open (const PProxy* pp, int fd, enum EExternType atype, const iid_t* importInterfaces, const iid_t* exportInterfaces) noexcept NONNULL(1);
void PExtern_Close (const PProxy* pp) noexcept NONNULL();
int  PExtern_Connect (const PProxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectLocal (const PProxy* pp, const char* path, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectUserLocal (const PProxy* pp, const char* sockname, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectSystemLocal (const PProxy* pp, const char* sockname, const iid_t* importedInterfaces) noexcept NONNULL();

extern const SInterface i_Extern;

void casycom_enable_externs (void) noexcept;

//{{{2 Extern_Connect --------------------------------------------------
#ifdef __cplusplus
namespace {
#endif

/// Create local IPv4 socket at given ip and port
static inline int PExtern_ConnectIP4 (const PProxy* pp, in_addr_t ip, in_port_t port, const iid_t* importedInterfaces)
{
    struct sockaddr_in addr = {
	.sin_family = PF_INET,
	.sin_addr = { ip },
	.sin_port = port
    };
#ifndef NDEBUG
    char addrbuf [64];
    DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_ntop(PF_INET, &addr.sin_addr, addrbuf, sizeof(addrbuf)), port);
#endif
    return PExtern_Connect (pp, (const struct sockaddr*) &addr, sizeof(addr), importedInterfaces);
}

/// Create local IPv4 socket at given port on the loopback interface
static inline int PExtern_ConnectLocalIP4 (const PProxy* pp, in_port_t port, const iid_t* importedInterfaces)
    { return PExtern_ConnectIP4 (pp, INADDR_LOOPBACK, port, importedInterfaces); }

/// Create local IPv6 socket at given ip and port
static inline int PExtern_ConnectIP6 (const PProxy* pp, struct in6_addr ip, in_port_t port, const iid_t* importedInterfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = ip,
	.sin6_port = port
    };
#ifndef NDEBUG
    char addrbuf [128];
    DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_ntop(PF_INET6, &addr.sin6_addr, addrbuf, sizeof(addrbuf)), port);
#endif
    return PExtern_Connect (pp, (const struct sockaddr*) &addr, sizeof(addr), importedInterfaces);
}

/// Create local IPv6 socket at given ip and port
static inline int PExtern_ConnectLocalIP6 (const PProxy* pp, in_port_t port, const iid_t* importedInterfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	.sin6_port = port
    };
    DEBUG_PRINTF ("[X] Connecting to socket localhost6:%hu\n", port);
    return PExtern_Connect (pp, (const struct sockaddr*) &addr, sizeof(addr), importedInterfaces);
}

#ifdef __cplusplus
} // namespace
#endif
//}}}2------------------------------------------------------------------
//}}}-------------------------------------------------------------------
//{{{ ExternR

enum EExternRMethod {
    method_ExternR_Connected,
    method_ExternR_N
};
typedef void (*MFN_ExternR_Connected)(void* vo, const SMsg* msg);
typedef struct _DExternR {
    iid_t			interface;
    MFN_ExternR_Connected	ExternR_Connected;
} DExternR;

void PExternR_Connected (const PProxy* pp) noexcept NONNULL();

extern const SInterface i_ExternR;

//}}}-------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
