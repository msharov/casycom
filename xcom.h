// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "main.h"
#include "vector.h"
#include <sys/socket.h>
#include <netinet/in.h>
#if !defined(NDEBUG) && !defined(UC_VERSION)
    #include <arpa/inet.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

//{{{ Extern -----------------------------------------------------------

enum EExternType {
    EXTERN_CLIENT,
    EXTERN_SERVER
};
typedef void (*MFN_Extern_open)(void* vo, int fd, enum EExternType atype, const iid_t* imported_interfaces, const iid_t* exportedInterfaces);
typedef void (*MFN_Extern_close)(void* vo);
typedef struct _DExtern {
    iid_t		interface;
    MFN_Extern_open	Extern_open;
    MFN_Extern_close	Extern_close;
} DExtern;

void PExtern_open (const Proxy* pp, int fd, enum EExternType atype, const iid_t* import_interfaces, const iid_t* export_interfaces) noexcept NONNULL(1);
void PExtern_close (const Proxy* pp) noexcept NONNULL();
int  PExtern_connect (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* imported_interfaces) noexcept NONNULL();
int  PExtern_connect_local (const Proxy* pp, const char* path, const iid_t* imported_interfaces) noexcept NONNULL();
int  PExtern_connect_user_local (const Proxy* pp, const char* sockname, const iid_t* imported_interfaces) noexcept NONNULL();
int  PExtern_connect_system_local (const Proxy* pp, const char* sockname, const iid_t* imported_interfaces) noexcept NONNULL();
int  PExtern_launch_pipe (const Proxy* pp, const char* exe, const char* arg, const iid_t* imported_interfaces) noexcept NONNULL(1,2,4);

extern const Interface i_Extern;

void casycom_enable_externs (void) noexcept;

//{{{2 Extern_connect --------------------------------------------------
#ifdef __cplusplus
namespace {
#endif

/// Create local IPv4 socket at given ip and port
static inline int PExtern_connect_ip4 (const Proxy* pp, in_addr_t ip, in_port_t port, const iid_t* imported_interfaces)
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
    #ifdef UC_VERSION
	DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_intop(addr.sin_addr, addrbuf, sizeof(addrbuf)), port);
    #else
	DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_ntop(PF_INET, &addr.sin_addr, addrbuf, sizeof(addrbuf)), port);
    #endif
#endif
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

/// Create local IPv4 socket at given port on the loopback interface
static inline int PExtern_connect_local_ip4 (const Proxy* pp, in_port_t port, const iid_t* imported_interfaces)
    { return PExtern_connect_ip4 (pp, INADDR_LOOPBACK, port, imported_interfaces); }

/// Create local IPv6 socket at given ip and port
static inline int PExtern_connect_ip6 (const Proxy* pp, struct in6_addr ip, in_port_t port, const iid_t* imported_interfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = ip,
	.sin6_port = port
    };
#if !defined(NDEBUG) && !defined(UC_VERSION)
    char addrbuf [128];
    DEBUG_PRINTF ("[X] Connecting to socket %s:%hu\n", inet_ntop(PF_INET6, &addr.sin6_addr, addrbuf, sizeof(addrbuf)), port);
#endif
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

/// Create local IPv6 socket at given ip and port
static inline int PExtern_connect_local_ip6 (const Proxy* pp, in_port_t port, const iid_t* imported_interfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	.sin6_port = port
    };
    DEBUG_PRINTF ("[X] Connecting to socket localhost6:%hu\n", port);
    return PExtern_connect (pp, (const struct sockaddr*) &addr, sizeof(addr), imported_interfaces);
}

#ifdef __cplusplus
} // namespace
#endif
//}}}2
//}}}-------------------------------------------------------------------
//{{{ ExternInfo

DECLARE_VECTOR_TYPE (InterfaceVector, Interface*);

typedef struct _ExternCredentials {
    pid_t	pid;
    uid_t	uid;
    #ifdef SCM_CREDS
	uid_t	euid; // BSD-only field, do not use
    #endif
    gid_t	gid;
} ExternCredentials;

typedef struct _ExternInfo {
    InterfaceVector	interfaces;
    ExternCredentials	creds;
    oid_t		oid;
    bool		is_client;
    bool		is_unix_socket;
} ExternInfo;

const ExternInfo* casycom_extern_info (oid_t eid) noexcept;
const ExternInfo* casycom_extern_object_info (oid_t oid) noexcept;

//}}}-------------------------------------------------------------------
//{{{ ExternR

typedef void (*MFN_ExternR_connected)(void* vo, const ExternInfo* einfo);
typedef struct _DExternR {
    iid_t			interface;
    MFN_ExternR_connected	ExternR_connected;
} DExternR;

void PExternR_connected (const Proxy* pp, const ExternInfo* einfo) noexcept NONNULL();

extern const Interface i_ExternR;

//}}}-------------------------------------------------------------------

#ifdef __cplusplus
} // extern "C"
#endif
