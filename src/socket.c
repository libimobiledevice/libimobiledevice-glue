/*
 * socket.c
 *
 * Copyright (C) 2012-2020 Nikias Bassen <nikias@gmx.li>
 * Copyright (C) 2012 Martin Szulecki <m.szulecki@libimobiledevice.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef _MSC_VER
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifndef HAVE_GETIFADDRS
#include <iphlpapi.h>
#endif
#ifndef IFF_RUNNING
#define IFF_RUNNING IFF_UP
#endif
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifdef AF_INET6
#include <net/if.h>
#include <ifaddrs.h>
#if defined (__APPLE__) || defined (__FreeBSD__) || defined (__HAIKU__)
#include <net/if_dl.h>
#endif
#ifdef __linux__
#include <netpacket/packet.h>
#endif
#endif
#endif
#include "common.h"
#include "libimobiledevice-glue/socket.h"
#ifdef HAVE_POLL
#include <sys/poll.h>
#endif

#define RECV_TIMEOUT 20000
#define SEND_TIMEOUT 10000
#define CONNECT_TIMEOUT 5000

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 102
#endif
#ifndef ECONNRESET
#define ECONNRESET 108
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif

#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

static int verbose = 0;

#define SOCKET_ERR(level, msg, ...) \
	if (verbose >= level) { \
		fprintf(stderr, "[socket] " msg , ## __VA_ARGS__); \
	}

void socket_init(void)
{
#ifdef _WIN32
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2,2), &wsa_data) != ERROR_SUCCESS) {
		SOCKET_ERR(0, "WSAStartup failed!\n");
		ExitProcess(1);
	}
#endif
	char *env_debug = getenv("SOCKET_DEBUG");
        if (env_debug) {
		verbose = (int)strtol(env_debug, NULL, 10);
	}
}

void socket_set_verbose(int level)
{
	verbose = level;
}

const char *socket_addr_to_string(struct sockaddr *addr, char *addr_out, size_t addr_out_size)
{
#if defined(_WIN32) && ( _WIN32_WINNT < 0x0600 )
	DWORD addr_out_len = addr_out_size;
	DWORD addrlen = 0;

	if (addr->sa_family == AF_INET) {
		addrlen = sizeof(struct sockaddr_in);
	}
#ifdef AF_INET6
	else if (addr->sa_family == AF_INET6) {
		addrlen = sizeof(struct sockaddr_in6);
	}
#endif
	else {
		errno = EAFNOSUPPORT;
		return NULL;
	}

	if (WSAAddressToStringA(addr, addrlen, NULL, addr_out, &addr_out_len) == 0) {
		return addr_out;
	}
#else
	const void *addrdata = NULL;

	if (addr->sa_family == AF_INET) {
		addrdata = &((struct sockaddr_in*)addr)->sin_addr;
	}
#ifdef AF_INET6
	else if (addr->sa_family == AF_INET6) {
		addrdata = &((struct sockaddr_in6*)addr)->sin6_addr;
	}
#endif
	else {
		errno = EAFNOSUPPORT;
		return NULL;
	}

	if (inet_ntop(addr->sa_family, addrdata, addr_out, addr_out_size)) {
		return addr_out;
	}
#endif
	return NULL;
}

enum poll_status
{
	poll_status_success,
	poll_status_timeout,
	poll_status_error
};

#ifdef _MSC_VER
#define ALWAYS_INLINE __forceinline
#else
#define ALWAYS_INLINE __inline__ __attribute__((__always_inline__))
#endif

#ifdef _WIN32
static ALWAYS_INLINE int WSAError_to_errno(int wsaerr)
{
	switch (wsaerr) {
		case WSAEINVAL:
			return EINVAL;
		case WSAENOTSOCK:
			return ENOTSOCK;
		case WSAENOTCONN:
			return ENOTCONN;
		case WSAESHUTDOWN:
			return ENOTCONN;
		case WSAECONNRESET:
			return ECONNRESET;
		case WSAECONNABORTED:
			return ECONNABORTED;
		case WSAECONNREFUSED:
			return ECONNREFUSED;
		case WSAENETDOWN:
			return ENETDOWN;
		case WSAENETRESET:
			return ENETRESET;
		case WSAEHOSTUNREACH:
			return EHOSTUNREACH;
		case WSAETIMEDOUT:
			return ETIMEDOUT;
		case WSAEWOULDBLOCK:
			return EWOULDBLOCK;
		case WSAEINPROGRESS:
			return EINPROGRESS;
		case WSAENOBUFS:
			return ENOBUFS;
		case WSAEINTR:
			return EINTR;
		case WSAEACCES:
			return EACCES;
		case WSAEFAULT:
			return EFAULT;
		default:
			break;
	}
	return wsaerr;
}
#endif

// timeout of -1 means infinity
static ALWAYS_INLINE enum poll_status poll_wrapper(int fd, fd_mode mode, int timeout)
{
#ifdef HAVE_POLL
	// https://man7.org/linux/man-pages/man2/select.2.html
	// Correspondence between select() and poll() notifications
	// #define POLLIN_SET  (EPOLLRDNORM | EPOLLRDBAND | EPOLLIN |
	//                      EPOLLHUP | EPOLLERR)
	//                    /* Ready for reading */
	// #define POLLOUT_SET (EPOLLWRBAND | EPOLLWRNORM | EPOLLOUT |
	//                      EPOLLERR)
	//                    /* Ready for writing */
	// #define POLLEX_SET  (EPOLLPRI)
	//                    /* Exceptional condition */

	short events;
	switch (mode) {
		case FDM_READ:
			events = POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR;
			break;
		case FDM_WRITE:
			events = POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR;
			break;
		case FDM_EXCEPT:
			events = POLLPRI;
			break;
		default:
			SOCKET_ERR(2, "%s: fd_mode %d unsupported\n", __func__, mode);
			return poll_status_error;
	}
	while (1) {
		struct pollfd pfd = {
			.fd = fd,
			.events = events,
		};
		switch (poll(&pfd, 1, timeout)) {
			case 1:
				if((pfd.revents & (POLLNVAL | POLLERR)) != 0)
				{
					SOCKET_ERR(2, "%s: poll unexpected events: %d\n", __func__, (int)pfd.revents);
					return poll_status_error;
				}
				return poll_status_success;
			case 0:
				return poll_status_timeout;
			case -1:
				if(errno == EINTR)
				{
					SOCKET_ERR(2, "%s: EINTR\n", __func__);
					continue;
				}
				// fallthrough
			default:
				SOCKET_ERR(2, "%s: poll failed: %s\n", __func__, strerror(errno));
				return poll_status_error;
		}
	}
#else
	fd_set fds;
	int sret;
	int eagain;
	struct timeval to;
	struct timeval *pto;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	sret = poll_status_error;

	do {
		if (timeout > 0) {
			to.tv_sec = (time_t) (timeout / 1000);
			to.tv_usec = (time_t) ((timeout - (to.tv_sec * 1000)) * 1000);
			pto = &to;
		} else {
			pto = NULL;
		}
		eagain = 0;
		switch (mode) {
		case FDM_READ:
			sret = select(fd + 1, &fds, NULL, NULL, pto);
			break;
		case FDM_WRITE:
			sret = select(fd + 1, NULL, &fds, NULL, pto);
			break;
		case FDM_EXCEPT:
			sret = select(fd + 1, NULL, NULL, &fds, pto);
			break;
		default:
			SOCKET_ERR(2, "%s: fd_mode %d unsupported\n", __func__, mode);
			return poll_status_error;
		}

		if (sret == 1) {
			return poll_status_success;
		} else if (sret == 0) {
			return poll_status_timeout;
		} else {
#ifdef _WIN32
			errno = WSAError_to_errno(WSAGetLastError());
#endif
			switch (errno) {
			case EINTR:
				// interrupt signal in select
				SOCKET_ERR(2, "%s: EINTR\n", __func__);
				eagain = 1;
				break;
			case EAGAIN:
				SOCKET_ERR(2, "%s: EAGAIN\n", __func__);
				break;
			default:
				SOCKET_ERR(2, "%s: select failed: %s\n", __func__, strerror(errno));
				return poll_status_error;
			}
		}
	} while (eagain);

	return sret;
#endif
}

#ifndef _WIN32
int socket_create_unix(const char *filename)
{
	struct sockaddr_un name;
	int sock;
#ifdef SO_NOSIGPIPE
	int yes = 1;
#endif

	// remove if still present
	unlink(filename);

	/* Create the socket. */
	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		SOCKET_ERR(1, "socket(): %s\n", strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		SOCKET_ERR(1, "setsockopt(): %s\n", strerror(errno));
		return -1;
	}
#endif

	/* Bind a name to the socket. */
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = '\0';

	if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0) {
		SOCKET_ERR(1, "bind(): %s\n", strerror(errno));
		socket_close(sock);
		return -1;
	}

	if (listen(sock, 100) < 0) {
		SOCKET_ERR(1, "listen(): %s\n", strerror(errno));
		socket_close(sock);
		return -1;
	}

	return sock;
}

int socket_connect_unix(const char *filename)
{
	struct sockaddr_un name;
	int sfd = -1;
	struct stat fst;
#ifdef SO_NOSIGPIPE
	int yes = 1;
#endif
	int bufsize = 0x20000;

	// check if socket file exists...
	if (stat(filename, &fst) != 0) {
		SOCKET_ERR(2, "%s: stat '%s': %s\n", __func__, filename, strerror(errno));
		return -1;
	}
	// ... and if it is a unix domain socket
	if (!S_ISSOCK(fst.st_mode)) {
		SOCKET_ERR(2, "%s: File '%s' is not a socket!\n", __func__, filename);
		return -1;
	}
	// make a new socket
	if ((sfd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		SOCKET_ERR(2, "%s: socket: %s\n", __func__, strerror(errno));
		return -1;
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void*)&bufsize, sizeof(int)) == -1) {
		SOCKET_ERR(1, "Could not set send buffer for socket: %s\n", strerror(errno));
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&bufsize, sizeof(int)) == -1) {
		SOCKET_ERR(1, "Could not set receive buffer for socket: %s\n", strerror(errno));
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		SOCKET_ERR(1, "setsockopt(): %s\n", strerror(errno));
		socket_close(sfd);
		return -1;
	}
#endif
	// and connect to 'filename'
	name.sun_family = AF_UNIX;
	strncpy(name.sun_path, filename, sizeof(name.sun_path));
	name.sun_path[sizeof(name.sun_path) - 1] = 0;

	int flags = fcntl(sfd, F_GETFL, 0);
	fcntl(sfd, F_SETFL, flags | O_NONBLOCK);

	do {
		if (connect(sfd, (struct sockaddr*)&name, sizeof(name)) != -1) {
			break;
		}
		if (errno == EINPROGRESS) {
			if (poll_wrapper(sfd, FDM_WRITE, CONNECT_TIMEOUT) == poll_status_success) {
				int so_error;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error == 0) {
					errno = 0;
					break;
				}
				errno = so_error;
			} else {
				int so_error = 0;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error != 0) {
					errno = so_error;
				}
			}
		}
		socket_close(sfd);
		sfd = -1;
	} while (0);

	if (sfd < 0) {
		SOCKET_ERR(2, "%s: connect: %s\n", __func__, strerror(errno));
		return -1;
	}

	return sfd;
}
#endif

int socket_create(const char* addr, uint16_t port)
{
	int sfd = -1;
	int yes = 1;
	int no = 0;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char portstr[8];
	int res;

	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	hints.ai_protocol = IPPROTO_TCP;

	snprintf(portstr, 8, "%d", port);

	res = getaddrinfo(addr, portstr, &hints, &result);
	if (res != 0) {
		SOCKET_ERR(1, "%s: getaddrinfo: %s\n", __func__, gai_strerror(res));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int)) == -1) {
#ifdef _WIN32
			errno = WSAError_to_errno(WSAGetLastError());
#endif
			SOCKET_ERR(1, "setsockopt() SO_REUSEADDR: %s\n", strerror(errno));
			socket_close(sfd);
			continue;
		}

#ifdef SO_NOSIGPIPE
		if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
			SOCKET_ERR(1, "setsockopt() SO_NOSIGPIPE: %s\n", strerror(errno));
			socket_close(sfd);
			continue;
		}
#endif

#if defined(AF_INET6) && defined(IPV6_V6ONLY)
		if (rp->ai_family == AF_INET6) {
			if (setsockopt(sfd, IPPROTO_IPV6, IPV6_V6ONLY, (addr) ? (void*)&yes : (void*)&no, sizeof(int)) == -1) {
#ifdef _WIN32
				errno = WSAError_to_errno(WSAGetLastError());
#endif
				SOCKET_ERR(1, "setsockopt() IPV6_V6ONLY: %s\n", strerror(errno));
			}
		}
#endif

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) < 0) {
#ifdef _WIN32
			errno = WSAError_to_errno(WSAGetLastError());
#endif
			SOCKET_ERR(1, "bind(): %s\n", strerror(errno));
			socket_close(sfd);
			continue;
		}

		if (listen(sfd, 100) < 0) {
#ifdef _WIN32
			errno = WSAError_to_errno(WSAGetLastError());
#endif
			SOCKET_ERR(1, "listen(): %s\n", strerror(errno));
			socket_close(sfd);
			continue;
		}
		break;
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		return -1;
	}

	return sfd;
}

#ifdef AF_INET6
static uint32_t _in6_addr_scope(struct in6_addr* addr)
{
	uint32_t scope = 0;

	if (IN6_IS_ADDR_MULTICAST(addr)) {
		if (IN6_IS_ADDR_MC_NODELOCAL(addr)) {
			scope = 1;
		} else if (IN6_IS_ADDR_MC_LINKLOCAL(addr)) {
			scope = 2;
		} else if (IN6_IS_ADDR_MC_SITELOCAL(addr)) {
			scope = 5;
		}

		return scope;
	}

	if (IN6_IS_ADDR_LINKLOCAL(addr)) {
		scope = 2;
	} else if (IN6_IS_ADDR_LOOPBACK(addr)) {
		scope = 2;
	} else if (IN6_IS_ADDR_SITELOCAL(addr)) {
		scope = 5;
	} else if (IN6_IS_ADDR_UNSPECIFIED(addr)) {
		scope = 0;
	}

	return scope;
}

#ifndef HAVE_GETIFADDRS
#ifdef _WIN32

struct ifaddrs {
	struct ifaddrs  *ifa_next;    /* Next item in list */
	char            *ifa_name;    /* Name of interface */
	unsigned int     ifa_flags;   /* Flags from SIOCGIFFLAGS */
	struct sockaddr *ifa_addr;    /* Address of interface */
	struct sockaddr *ifa_netmask; /* Netmask of interface */
	union {
		struct sockaddr *ifu_broadaddr; /* Broadcast address of interface */
		struct sockaddr *ifu_dstaddr;   /* Point-to-point destination address */
	} ifa_ifu;
#define                  ifa_broadaddr ifa_ifu.ifu_broadaddr
#define                  ifa_dstaddr   ifa_ifu.ifu_dstaddr
	void            *ifa_data;    /* Address-specific data */
};

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

static void freeifaddrs(struct ifaddrs *ifa)
{
	if (!ifa) {
		return;
	}
	free(ifa->ifa_name);
	free(ifa->ifa_addr);
	free(ifa->ifa_netmask);
	free(ifa->ifa_dstaddr);
	free(ifa->ifa_data);
	freeifaddrs(ifa->ifa_next);
	free(ifa);
}

/*
 * getifaddrs() reference implementation for win32.
 * Heavily based on openpgm's implementation found here:
 * https://github.com/steve-o/openpgm/blob/master/openpgm/pgm/getifaddrs.c
 */
static int getifaddrs(struct ifaddrs** ifap)
{
	struct ifaddrs* ifa = NULL;

	DWORD dwRetVal = 0;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;

	ULONG flags = GAA_FLAG_INCLUDE_PREFIX |
		GAA_FLAG_SKIP_ANYCAST |
		GAA_FLAG_SKIP_DNS_SERVER |
		GAA_FLAG_SKIP_FRIENDLY_NAME |
		GAA_FLAG_SKIP_MULTICAST;

	PIP_ADAPTER_ADDRESSES adapter = NULL;

	if (!ifap) {
		errno = EINVAL;
		return -1;
	}
	*ifap = NULL;

	outBufLen = WORKING_BUFFER_SIZE;
	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
		if (pAddresses == NULL) {
			SOCKET_ERR(1, "Memory allocation failed for IP_ADAPTER_ADDRESSES struct\n");
			return -1;
		}
		dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &outBufLen);
		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			free(pAddresses);
			pAddresses = NULL;
		} else {
			break;
		}
		Iterations++;
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));

	if (dwRetVal != NO_ERROR) {
		free(pAddresses);
		return -1;
	}

	for (adapter = pAddresses; adapter; adapter = adapter->Next) {
		int unicastIndex = 0;
		for (IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next, ++unicastIndex) {
			/* ensure IP adapter */
			if (AF_INET != unicast->Address.lpSockaddr->sa_family && AF_INET6 != unicast->Address.lpSockaddr->sa_family) {
				continue;
			}

			if (!ifa) {
				ifa = calloc(1, sizeof(struct ifaddrs));
				if (!ifa) {
					errno = ENOMEM;
					free(pAddresses);
					return -1;
				}
				*ifap = ifa;
				ifa->ifa_next = NULL;
			} else {
				struct ifaddrs* ifanew = calloc(1, sizeof(struct ifaddrs));
				if (!ifanew) {
					freeifaddrs(*ifap);
					free(pAddresses);
					errno = ENOMEM;
					return -1;
				}
				ifa->ifa_next = ifanew;
				ifa = ifanew;
				ifa->ifa_next = NULL;
			}
			ifa->ifa_data = NULL;

			/* name */
			ifa->ifa_name = strdup(adapter->AdapterName);

			/* flags */
			ifa->ifa_flags = 0;
			if (IfOperStatusUp == adapter->OperStatus)
				ifa->ifa_flags |= IFF_UP;
			if (IF_TYPE_SOFTWARE_LOOPBACK == adapter->IfType)
				ifa->ifa_flags |= IFF_LOOPBACK;
			if (!(adapter->Flags & IP_ADAPTER_NO_MULTICAST))
				ifa->ifa_flags |= IFF_MULTICAST;

			/* address */
			ifa->ifa_addr = (struct sockaddr*)malloc(sizeof(struct sockaddr_storage));
			memcpy(ifa->ifa_addr, unicast->Address.lpSockaddr, unicast->Address.iSockaddrLength);

			/* netmask */
			ifa->ifa_netmask = (struct sockaddr*)calloc(1, sizeof(struct sockaddr_storage));

			/* store mac address */
			if (adapter->PhysicalAddressLength == 6) {
				ifa->ifa_data = malloc(6);
				memcpy(ifa->ifa_data, adapter->PhysicalAddress, 6);
			}

/* pre-Vista must hunt for matching prefix in linked list, otherwise use
 * OnLinkPrefixLength from IP_ADAPTER_UNICAST_ADDRESS structure.
 * FirstPrefix requires Windows XP SP1, from SP1 to pre-Vista provides a
 * single adapter prefix for each IP address.  Vista and later provides
 * host IP address prefix, subnet IP address, and subnet broadcast IP
 * address.  In addition there is a multicast and broadcast address prefix.
 */
			ULONG prefixLength = 0;

#if defined( _WIN32 ) && ( _WIN32_WINNT >= 0x0600 )
/* For a unicast IPv4 address, any value greater than 32 is an illegal
 * value. For a unicast IPv6 address, any value greater than 128 is an
 * illegal value. A value of 255 is commonly used to represent an illegal
 * value.
 *
 * Windows 7 SP1 returns 64 for Teredo links which is incorrect.
 */

#define IN6_IS_ADDR_TEREDO(addr) \
	(((const uint32_t *)(addr))[0] == ntohl (0x20010000))

			if (AF_INET6 == unicast->Address.lpSockaddr->sa_family &&
/* TunnelType only applies to one interface on the adapter and no
 * convenient method is provided to determine which.
 */
				TUNNEL_TYPE_TEREDO == adapter->TunnelType &&
/* Test the interface with the known Teredo network prefix.
 */
				IN6_IS_ADDR_TEREDO( &((struct sockaddr_in6*)(unicast->Address.lpSockaddr))->sin6_addr) &&
/* Test that this version is actually wrong, subsequent releases from Microsoft
 * may resolve the issue.
 */
				32 != unicast->OnLinkPrefixLength)
			{
				prefixLength = 32;
			}
			else
				prefixLength = unicast->OnLinkPrefixLength;
#else
/* The order of linked IP_ADAPTER_UNICAST_ADDRESS structures pointed to by
 * the FirstUnicastAddress member does not have any relationship with the
 * order of linked IP_ADAPTER_PREFIX structures pointed to by the FirstPrefix
 * member.
 *
 * Example enumeration:
 *    [ no subnet ]
 *   ::1/128            - address
 *   ff00::%1/8         - multicast (no IPv6 broadcast)
 *   127.0.0.0/8        - subnet
 *   127.0.0.1/32       - address
 *   127.255.255.255/32 - subnet broadcast
 *   224.0.0.0/4        - multicast
 *   255.255.255.255/32 - broadcast
 *
 * Which differs from most adapters listing three IPv6:
 *   fe80::%10/64       - subnet
 *   fe80::51e9:5fe5:4202:325a%10/128 - address
 *   ff00::%10/8        - multicast
 *
 * !IfOperStatusUp IPv4 addresses are skipped:
 *   fe80::%13/64       - subnet
 *   fe80::d530:946d:e8df:8c91%13/128 - address
 *   ff00::%13/8        - multicast
 *    [ no subnet  ]
 *    [ no address ]
 *   224.0.0.0/4        - multicast
 *   255.255.255.255/32 - broadcast
 *
 * On PTP links no multicast or broadcast addresses are returned:
 *    [ no subnet ]
 *   fe80::5efe:10.203.9.30/128 - address
 *    [ no multicast ]
 *    [ no multicast ]
 *    [ no broadcast ]
 *
 * Active primary IPv6 interfaces are a bit overloaded:
 *   ::/0               - default route
 *   2001::/32          - global subnet
 *   2001:0:4137:9e76:2443:d6:ba87:1a2a/128 - global address
 *   fe80::/64          - link-local subnet
 *   fe80::2443:d6:ba87:1a2a/128 - link-local address
 *   ff00::/8           - multicast
 */

#define IN_LINKLOCAL(a)	((((uint32_t) (a)) & 0xaffff0000) == 0xa9fe0000)

			for (IP_ADAPTER_PREFIX *prefix = adapter->FirstPrefix; prefix; prefix = prefix->Next) {
				LPSOCKADDR lpSockaddr = prefix->Address.lpSockaddr;
				if (lpSockaddr->sa_family != unicast->Address.lpSockaddr->sa_family)
					continue;
/* special cases */
/* RFC2863: IPv4 interface not up */
				if (AF_INET == lpSockaddr->sa_family && adapter->OperStatus != IfOperStatusUp) {
/* RFC3927: link-local IPv4 always has 16-bit CIDR */
					if (IN_LINKLOCAL( ntohl (((struct sockaddr_in*)(unicast->Address.lpSockaddr))->sin_addr.s_addr))) {
						prefixLength = 16;
					}
					break;
				}
/* default IPv6 route */
				if (AF_INET6 == lpSockaddr->sa_family && 0 == prefix->PrefixLength && IN6_IS_ADDR_UNSPECIFIED( &((struct sockaddr_in6*)(lpSockaddr))->sin6_addr)) {
					continue;
				}
/* Assume unicast address for first prefix of operational adapter */
				if (AF_INET == lpSockaddr->sa_family)
					if (IN_MULTICAST( ntohl (((struct sockaddr_in*)(lpSockaddr))->sin_addr.s_addr))) {
						SOCKET_ERR(1, "FATAL: first prefix is non a unicast address\n");
						break;
					}
				if (AF_INET6 == lpSockaddr->sa_family)
					if (IN6_IS_ADDR_MULTICAST( &((struct sockaddr_in6*)(lpSockaddr))->sin6_addr)) {
						SOCKET_ERR(1, "FATAL: first prefix is not a unicast address\n");
						break;
					}
/* Assume subnet or host IP address for XP backward compatibility */

				prefixLength = prefix->PrefixLength;
				break;
			}
#endif /* defined( _WIN32 ) && ( _WIN32_WINNT >= 0x0600 ) */

/* map prefix to netmask */
			ifa->ifa_netmask->sa_family = unicast->Address.lpSockaddr->sa_family;
			switch (unicast->Address.lpSockaddr->sa_family) {
			case AF_INET:
				if (0 == prefixLength || prefixLength > 32) {
					prefixLength = 32;
				}
#if defined( _WIN32) && ( _WIN32_WINNT >= 0x0600 )
/* Added in Vista, but no IPv6 equivalent. */
				{
				ULONG Mask;
				ConvertLengthToIpv4Mask (prefixLength, &Mask);
				((struct sockaddr_in*)ifa->ifa_netmask)->sin_addr.s_addr = Mask;	/* network order */
				}
#else
/* NB: left-shift of full bit-width is undefined in C standard. */
				((struct sockaddr_in*)ifa->ifa_netmask)->sin_addr.s_addr = htonl( 0xffffffffU << ( 32 - prefixLength ) );
#endif
				break;

			case AF_INET6:
				if (0 == prefixLength || prefixLength > 128) {
					prefixLength = 128;
				}
				for (LONG i = prefixLength, j = 0; i > 0; i -= 8, ++j) {
					((struct sockaddr_in6*)ifa->ifa_netmask)->sin6_addr.s6_addr[ j ] = i >= 8 ? 0xff : (ULONG)(( 0xffU << ( 8 - i ) ) & 0xffU );
				}
				break;
			default:
				break;
			}
		}
	}
	free(pAddresses);

	return 0;
}
#else
#error No reference implementation for getifaddrs available for this platform.
#endif
#endif

int get_primary_mac_address(unsigned char mac_addr_buf[6])
{
	int result = -1;
	struct ifaddrs *ifaddr = NULL, *ifa = NULL;
	if (getifaddrs(&ifaddr) != -1) {
		for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr == NULL) {
				continue;
			}
			if ((ifa->ifa_flags & IFF_UP) == 0) {
				continue;
			}
			if (ifa->ifa_flags & IFF_LOOPBACK) {
				continue;
			}
#if defined(__APPLE__) || defined (__FreeBSD__) || defined (__HAIKU__)
			if (ifa->ifa_addr->sa_family != AF_LINK) {
				continue;
			}
#if defined (__APPLE__)
			if (!strcmp(ifa->ifa_name, "en0")) {
#elif defined (__FreeBSD__) || defined (__HAIKU__)
			{
#endif
				memcpy(mac_addr_buf, (unsigned char *)LLADDR((struct sockaddr_dl *)(ifa)->ifa_addr), 6);
				result = 0;
				break;
			}
#elif defined (__linux__)
			if (ifa->ifa_addr->sa_family != AF_PACKET) {
				continue;
			}
			if (strcmp(ifa->ifa_name, "lo") != 0) {
				memcpy(mac_addr_buf, ((struct sockaddr_ll*)ifa->ifa_addr)->sll_addr, 6);
				result = 0;
				break;
			}
#elif defined (WIN32)
			if (ifa->ifa_data) {
				memcpy(mac_addr_buf, ifa->ifa_data, 6);
				result = 0;
				break;
			}
#elif defined(__CYGWIN__)
			if (ifa->ifa_data) {
				memcpy(mac_addr_buf, ((struct ifaddrs_hwdata *)ifa->ifa_data)->ifa_hwaddr.sa_data, 6);
				result = 0;
				break;
			}
#else
#error get_primary_mac_address is not supported on this platform.
#endif
		}
		freeifaddrs(ifaddr);
	}
	return result;
}

static int32_t _sockaddr_in6_scope_id(struct sockaddr_in6* addr)
{
	int32_t res = -1;
	struct ifaddrs *ifaddr = NULL, *ifa = NULL;
	uint32_t addr_scope;

	/* get scope for requested address */
	addr_scope = _in6_addr_scope(&addr->sin6_addr);
	if (addr_scope == 0) {
		/* global scope doesn't need a specific scope id */
		return addr_scope;
	}

	/* get interfaces */
	if (getifaddrs(&ifaddr) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "getifaddrs(): %s\n", strerror(errno));
		return res;
	}

	/* loop over interfaces */
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		/* skip if no address is available */
		if (ifa->ifa_addr == NULL) {
			continue;
		}

		/* skip if wrong family */
		if (ifa->ifa_addr->sa_family != AF_INET6) {
			continue;
		}

		/* skip if not up */
		if ((ifa->ifa_flags & IFF_UP) == 0) {
			continue;
		}

#ifndef __HAIKU__
		/* skip if not running */
		if ((ifa->ifa_flags & IFF_RUNNING) == 0) {
			continue;
		}
#endif

		struct sockaddr_in6* addr_in = (struct sockaddr_in6*)ifa->ifa_addr;

		/* skip if scopes do not match */
		if (_in6_addr_scope(&addr_in->sin6_addr) != addr_scope) {
			continue;
		}

		/* use if address is equal */
		if (memcmp(&addr->sin6_addr.s6_addr, &addr_in->sin6_addr.s6_addr, sizeof(addr_in->sin6_addr.s6_addr)) == 0) {
			/* if scope id equals the requested one then assume it was valid */
			if (addr->sin6_scope_id == addr_in->sin6_scope_id) {
				res = addr_in->sin6_scope_id;
				break;
			}
			res = addr_in->sin6_scope_id;
			continue;
		}

		/* skip loopback interface if not already matched exactly above */
		if ((ifa->ifa_flags & IFF_LOOPBACK) != 0) {
			continue;
		}

		res = addr_in->sin6_scope_id;

		/* if scope id equals the requested one then assume it was valid */
		if (addr->sin6_scope_id == addr_in->sin6_scope_id) {
			/* set the scope id of this interface as most likely candidate */
			break;
		}
	}

	freeifaddrs(ifaddr);

	return res;
}
#endif

int socket_connect_addr(struct sockaddr* addr, uint16_t port)
{
	int sfd = -1;
	int yes = 1;
	int bufsize = 0x20000;
	int addrlen = 0;
#ifdef _WIN32
	u_long l_yes = 1;
#endif

	if (addr->sa_family == AF_INET) {
		struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
		addr_in->sin_port = htons(port);
		addrlen = sizeof(struct sockaddr_in);
	}
#ifdef AF_INET6
	else if (addr->sa_family == AF_INET6) {
		struct sockaddr_in6* addr_in = (struct sockaddr_in6*)addr;
		addr_in->sin6_port = htons(port);

		/*
		 * IPv6 Routing Magic:
		 *
		 * If the scope of the address is a link-local one, IPv6 requires the
		 * scope id set to an interface number to allow proper routing. However,
		 * as the provided sockaddr might contain a wrong scope id, we must find
		 * a scope id from a suitable interface on this system or routing might
		 * fail. An IPv6 guru should have another look though...
		 */
		addr_in->sin6_scope_id = _sockaddr_in6_scope_id(addr_in);

		addrlen = sizeof(struct sockaddr_in6);
	}
#endif
	else {
		SOCKET_ERR(1, "ERROR: Unsupported address family\n");
		return -1;
	}

	sfd = socket(addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (sfd == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "socket(): %s\n", strerror(errno));
		return -1;
	}

#ifdef SO_NOSIGPIPE
	if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
		SOCKET_ERR(1, "setsockopt() SO_NOSIGPIPE: %s\n", strerror(errno));
		socket_close(sfd);
		return -1;
	}
#endif

	if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "setsockopt() SO_REUSEADDR: %s\n", strerror(errno));
		socket_close(sfd);
		return -1;
	}

#ifdef _WIN32
	ioctlsocket(sfd, FIONBIO, &l_yes);
#else
	int flags = fcntl(sfd, F_GETFL, 0);
	fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
#endif

	do {
		if (connect(sfd, addr, addrlen) != -1) {
			break;
		}
#ifdef _WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
		if (errno == EINPROGRESS)
#endif
		{
			if (poll_wrapper(sfd, FDM_WRITE, CONNECT_TIMEOUT) == poll_status_success) {
				int so_error;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error == 0) {
					errno = 0;
					break;
				}
#ifdef _WIN32
				so_error = WSAError_to_errno(so_error);
#endif
				errno = so_error;
			} else {
				int so_error = 0;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error != 0) {
#ifdef _WIN32
					so_error = WSAError_to_errno(so_error);
#endif
					errno = so_error;
				}
			}
		}
		socket_close(sfd);
		sfd = -1;
	} while (0);

	if (sfd < 0) {
		if (verbose >= 2) {
			char addrtxt[48];
			socket_addr_to_string(addr, addrtxt, sizeof(addrtxt));
			SOCKET_ERR(2, "%s: Could not connect to %s port %d\n", __func__, addrtxt, port);
		}
		return -1;
	}

	if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void*)&yes, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set TCP_NODELAY on socket: %s\n", strerror(errno));
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void*)&bufsize, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set send buffer for socket: %s\n", strerror(errno));
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&bufsize, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set receive buffer for socket: %s\n", strerror(errno));
	}

	return sfd;
}

int socket_connect(const char *addr, uint16_t port)
{
	int sfd = -1;
	int yes = 1;
	int bufsize = 0x20000;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char portstr[8];
	int res;
#ifdef _WIN32
	u_long l_yes = 1;
#else
	int flags = 0;
#endif

	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_protocol = IPPROTO_TCP;

	snprintf(portstr, 8, "%d", port);

	res = getaddrinfo(addr, portstr, &hints, &result);
	if (res != 0) {
		SOCKET_ERR(1, "%s: getaddrinfo: %s\n", __func__, gai_strerror(res));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}

#ifdef SO_NOSIGPIPE
		if (setsockopt(sfd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&yes, sizeof(int)) == -1) {
			SOCKET_ERR(1, "setsockopt() SO_NOSIGPIPE: %s\n", strerror(errno));
			socket_close(sfd);
			return -1;
		}
#endif

		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void*)&yes, sizeof(int)) == -1) {
#ifdef _WIN32
			errno = WSAError_to_errno(WSAGetLastError());
#endif
			SOCKET_ERR(1, "setsockopt() SO_REUSEADDR: %s\n", strerror(errno));
			socket_close(sfd);
			continue;
		}

#ifdef _WIN32
		ioctlsocket(sfd, FIONBIO, &l_yes);
#else
		flags = fcntl(sfd, F_GETFL, 0);
		fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
#endif

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
#ifdef _WIN32
		if (WSAGetLastError() == WSAEWOULDBLOCK)
#else
		if (errno == EINPROGRESS)
#endif
		{
			if (poll_wrapper(sfd, FDM_WRITE, CONNECT_TIMEOUT) == poll_status_success) {
				int so_error;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error == 0) {
					errno = 0;
					break;
				}
#ifdef _WIN32
				so_error = WSAError_to_errno(so_error);
#endif
				errno = so_error;
			} else {
				int so_error = 0;
				socklen_t len = sizeof(so_error);
				getsockopt(sfd, SOL_SOCKET, SO_ERROR, (void*)&so_error, &len);
				if (so_error != 0) {
#ifdef _WIN32
					so_error = WSAError_to_errno(so_error);
#endif
					errno = so_error;
				}
			}
		}
		socket_close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL) {
		SOCKET_ERR(2, "%s: Could not connect to %s:%d\n", __func__, addr, port);
		return -1;
	}

	if (setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, (void*)&yes, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set TCP_NODELAY on socket: %s\n", strerror(errno));
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, (void*)&bufsize, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set send buffer for socket: %s\n", strerror(errno));
	}

	if (setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, (void*)&bufsize, sizeof(int)) == -1) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		SOCKET_ERR(1, "Could not set receive buffer for socket: %s\n", strerror(errno));
	}

	return sfd;
}

int socket_check_fd(int fd, fd_mode fdm, unsigned int timeout)
{
	if (fd < 0) {
		SOCKET_ERR(2, "ERROR: invalid fd in check_fd %d\n", fd);
		return -EINVAL;
	}

	int timeout_ms;
	if (timeout > 0) {
		timeout_ms = (int)timeout;
		if (timeout_ms <= 0)
			timeout_ms = -1;
	} else {
		timeout_ms = -1;
	}

	switch (poll_wrapper(fd, fdm, timeout_ms)) {
		case poll_status_success:
			return 1;
		case poll_status_timeout:
			return -ETIMEDOUT;
		case poll_status_error:
		default:
			SOCKET_ERR(2, "%s: poll_wrapper failed\n", __func__);
			return -ECONNRESET;
	}

	return -ECONNRESET;
}

int socket_accept(int fd, uint16_t port)
{
#ifdef _WIN32
	int addr_len;
#else
	socklen_t addr_len;
#endif
	int result;
	struct sockaddr_storage addr;
	addr_len = sizeof(addr);

	result = accept(fd, (struct sockaddr*)&addr, &addr_len);
#ifdef _WIN32
	if (result < 0) {
		errno = WSAError_to_errno(WSAGetLastError());
	}
#endif
	return result;
}

int socket_shutdown(int fd, int how)
{
	int result = shutdown(fd, how);
#ifdef _WIN32
	if (result < 0) {
		errno = WSAError_to_errno(WSAGetLastError());
	}
#endif
	return result;
}

int socket_close(int fd)
{
#ifdef _WIN32
	int result = closesocket(fd);
	if (result < 0) {
		errno = WSAError_to_errno(WSAGetLastError());
	}
	return result;
#else
	return close(fd);
#endif
}

int socket_receive(int fd, void *data, size_t length)
{
	return socket_receive_timeout(fd, data, length, 0, RECV_TIMEOUT);
}

int socket_peek(int fd, void *data, size_t length)
{
	return socket_receive_timeout(fd, data, length, MSG_PEEK, RECV_TIMEOUT);
}

int socket_receive_timeout(int fd, void *data, size_t length, int flags, unsigned int timeout)
{
	int res;
	int result;

	// check if data is available
	res = socket_check_fd(fd, FDM_READ, timeout);
	if (res <= 0) {
		return res;
	}
	// if we get here, there _is_ data available
	result = recv(fd, data, length, flags);
	if (result == 0) {
		// but this is an error condition
		SOCKET_ERR(3, "%s: fd=%d recv returned 0\n", __func__, fd);
		return -ECONNRESET;
	}
	if (result < 0) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		return -errno;
	}
	return result;
}

int socket_send(int fd, const void *data, size_t length)
{
	int flags = 0;
	int res = socket_check_fd(fd, FDM_WRITE, SEND_TIMEOUT);
	if (res <= 0) {
		return res;
	}
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
	int s = (int)send(fd, data, length, flags);
	if (s < 0) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		return -errno;
	}
	return s;
}

int socket_get_socket_port(int fd, uint16_t *port)
{
#ifdef _WIN32
	int addr_len;
#else
	socklen_t addr_len;
#endif
	struct sockaddr_in addr;

	memset(&addr, 0, sizeof(addr));

	addr_len = sizeof(addr);
	if (0 > getsockname(fd, (struct sockaddr*)&addr, &addr_len)) {
#ifdef _WIN32
		errno = WSAError_to_errno(WSAGetLastError());
#endif
		return -1;
	}

	*port = ntohs(addr.sin_port);
	return 0;
}
