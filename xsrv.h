// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#pragma once
#include "xcom.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MFN_ExternServer_Open)(void* vo, int fd, const iid_t* exportedInterfaces, bool closeWhenEmpty);
typedef void (*MFN_ExternServer_Close)(void* vo);
typedef struct _DExternServer {
    iid_t interface;
    MFN_ExternServer_Open ExternServer_Open;
    MFN_ExternServer_Close ExternServer_Close;
} DExternServer;

void PExternServer_Open (const Proxy* pp, int fd, const iid_t* exportedInterfaces, bool closeWhenEmpty) noexcept NONNULL(1);
void PExternServer_Close (const Proxy* pp) noexcept NONNULL();
int  PExternServer_Bind (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* exportedInterfaces) noexcept NONNULL();
int  PExternServer_BindLocal (const Proxy* pp, const char* path, const iid_t* exportedInterfaces) noexcept NONNULL();
int  PExternServer_BindUserLocal (const Proxy* pp, const char* sockname, const iid_t* exportedInterfaces) noexcept NONNULL();
int  PExternServer_BindSystemLocal (const Proxy* pp, const char* sockname, const iid_t* exportedInterfaces) noexcept NONNULL();

extern const Interface i_ExternServer;
extern const Factory f_ExternServer;

//{{{ PExternServer_Bind variants --------------------------------------

#ifdef __cplusplus
namespace {
#endif

/// Create local IPv4 socket at given ip and port
static inline int PExternServer_BindIP4 (const Proxy* pp, in_addr_t ip, in_port_t port, const iid_t* exportedInterfaces)
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
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

/// Create local IPv4 socket at given port on the loopback interface
static inline int PExternServer_BindLocalIP4 (const Proxy* pp, in_port_t port, const iid_t* exportedInterfaces)
    { return PExternServer_BindIP4 (pp, INADDR_LOOPBACK, port, exportedInterfaces); }

/// Create local IPv6 socket at given ip and port
static inline int PExternServer_BindIP6 (const Proxy* pp, struct in6_addr ip, in_port_t port, const iid_t* exportedInterfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = ip,
	.sin6_port = port
    };
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

/// Create local IPv6 socket at given ip and port
static inline int PExternServer_BindLocalIP6 (const Proxy* pp, in_port_t port, const iid_t* exportedInterfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	.sin6_port = port
    };
    return PExternServer_Bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exportedInterfaces);
}

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
//}}}-------------------------------------------------------------------
