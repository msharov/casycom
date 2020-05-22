// This file is part of the casycom project
//
// Copyright (c) 2015 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the ISC license.

#pragma once
#include "xcom.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*MFN_ExternServer_open)(void* vo, int fd, const iid_t* exported_interfaces, bool close_when_empty);
typedef void (*MFN_ExternServer_close)(void* vo);
typedef struct _DExternServer {
    iid_t interface;
    MFN_ExternServer_open ExternServer_open;
    MFN_ExternServer_close ExternServer_close;
} DExternServer;

void PExternServer_open (const Proxy* pp, int fd, const iid_t* exported_interfaces, bool close_when_empty) noexcept NONNULL(1);
void PExternServer_close (const Proxy* pp) noexcept NONNULL();
int  PExternServer_bind (const Proxy* pp, const struct sockaddr* addr, socklen_t addrlen, const iid_t* exported_interfaces) noexcept NONNULL();
int  PExternServer_bind_local (const Proxy* pp, const char* path, const iid_t* exported_interfaces) noexcept NONNULL();
int  PExternServer_bind_user_local (const Proxy* pp, const char* sockname, const iid_t* exported_interfaces) noexcept NONNULL();
int  PExternServer_bind_system_local (const Proxy* pp, const char* sockname, const iid_t* exported_interfaces) noexcept NONNULL();

extern const Interface i_ExternServer;
extern const Factory f_ExternServer;

//{{{ PExternServer_bind variants --------------------------------------

#ifdef __cplusplus
namespace {
#endif

/// Create local IPv4 socket at given ip and port
static inline int PExternServer_bind_ip4 (const Proxy* pp, in_addr_t ip, in_port_t port, const iid_t* exported_interfaces)
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
    return PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
}

/// Create local IPv4 socket at given port on the loopback interface
static inline int PExternServer_bind_local_ip4 (const Proxy* pp, in_port_t port, const iid_t* exported_interfaces)
    { return PExternServer_bind_ip4 (pp, INADDR_LOOPBACK, port, exported_interfaces); }

/// Create local IPv6 socket at given ip and port
static inline int PExternServer_bind_ip6 (const Proxy* pp, struct in6_addr ip, in_port_t port, const iid_t* exported_interfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = ip,
	.sin6_port = port
    };
    return PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
}

/// Create local IPv6 socket at given ip and port
static inline int PExternServer_bind_local_ip6 (const Proxy* pp, in_port_t port, const iid_t* exported_interfaces)
{
    struct sockaddr_in6 addr = {
	.sin6_family = PF_INET6,
	.sin6_addr = IN6ADDR_LOOPBACK_INIT,
	.sin6_port = port
    };
    return PExternServer_bind (pp, (const struct sockaddr*) &addr, sizeof(addr), exported_interfaces);
}

#ifdef __cplusplus
} // namespace
} // extern "C"
#endif
//}}}-------------------------------------------------------------------
