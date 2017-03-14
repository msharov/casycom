// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef __cplusplus
extern "C" {
#endif

//{{{ Extern -----------------------------------------------------------

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

void PExtern_Open (const Proxy* pp, int fd, enum EExternType atype, const iid_t* importInterfaces, const iid_t* exportInterfaces) noexcept NONNULL(1);
void PExtern_Close (const Proxy* pp) noexcept NONNULL();
int  PExtern_Connect (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectLocal (const Proxy* pp, const char* path, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectUserLocal (const Proxy* pp, const char* sockname, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_ConnectSystemLocal (const Proxy* pp, const char* sockname, const iid_t* importedInterfaces) noexcept NONNULL();
int  PExtern_LaunchPipe (const Proxy* pp, const char* exe, const char* arg, const iid_t* importedInterfaces) noexcept NONNULL(1,3);

extern const Interface i_Extern;

void casycom_enable_externs (void) noexcept;

//{{{2 Extern_Connect --------------------------------------------------
#ifdef __cplusplus
namespace {
#endif

/// Create local IPv4 socket at given ip and port
static inline int PExtern_ConnectIP4 (const Proxy* pp, in_addr_t ip, in_port_t port, const iid_t* importedInterfaces)
{
    struct sockaddr_in addr = {
	.sin_family = PF_INET,
	#ifdef UC_VERSION
	    .sin_addr = ip,
	#else
	    .sin_addr = { ip },
	#endif
	.sin_port = port
    };
#ifndef NDEBUG
    char addrbuf [64];
    DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_ntop(PF_INET, &addr.sin_addr, addrbuf, sizeof(addrbuf)), port);
#endif
    return PExtern_Connect (pp, (const struct sockaddr*) &addr, sizeof(addr), importedInterfaces);
}

/// Create local IPv4 socket at given port on the loopback interface
static inline int PExtern_ConnectLocalIP4 (const Proxy* pp, in_port_t port, const iid_t* importedInterfaces)
    { return PExtern_ConnectIP4 (pp, INADDR_LOOPBACK, port, importedInterfaces); }

/// Create local IPv6 socket at given ip and port
static inline int PExtern_ConnectIP6 (const Proxy* pp, struct in6_addr ip, in_port_t port, const iid_t* importedInterfaces)
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
static inline int PExtern_ConnectLocalIP6 (const Proxy* pp, in_port_t port, const iid_t* importedInterfaces)
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
//}}}2
//}}}-------------------------------------------------------------------
//{{{ ExternInfo

DECLARE_VECTOR_TYPE (InterfaceVector, Interface*);
typedef struct _ExternInfo {
    InterfaceVector	interfaces;
    struct ucred	creds;
    oid_t		oid;
    bool		isClient;
    bool		isUnixSocket;
} ExternInfo;

const ExternInfo* casycom_extern_info (oid_t eid) noexcept;
const ExternInfo* casycom_extern_object_info (oid_t oid) noexcept;

//}}}-------------------------------------------------------------------
//{{{ ExternR

typedef void (*MFN_ExternR_Connected)(void* vo, const ExternInfo* einfo);
typedef struct _DExternR {
    iid_t			interface;
    MFN_ExternR_Connected	ExternR_Connected;
} DExternR;

void PExternR_Connected (const Proxy* pp, const ExternInfo* einfo) noexcept NONNULL();

extern const Interface i_ExternR;

//}}}-------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
