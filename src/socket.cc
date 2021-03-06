/**
 * \file src/socket.cc
 * Socket class
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<netinet/tcp.h>
#include<sys/types.h>
#include<sys/socket.h>

#include"socket.h"
#include"gaiwrap.h"

/* For those OSs that don't read RFC3493, even though their manpage
 * points to it. */
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif

/**
 * Create socket object from existing file descriptor.
 *
 * @param[in] infd File descriptor to use.
 */
Socket::Socket(int infd)
	:debug(false)
{
        connected_af_ = AF_UNSPEC;
        if (infd > 0) {
                struct sockaddr_storage sa;
                socklen_t salen = sizeof(sa);
                if (-1 != getpeername(infd, (struct sockaddr*)&sa, &salen)) {
                        connected_af_ = sa.ss_family;
                }
        }
	fd.set(infd);
}

/**
 * Create new file descriptor
 *
 * @param[in] ai addrinfo struct specifying address family and such.
 */
void
Socket::create_socket(const struct addrinfo *ai)
{
	int s;
	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s == -1) {
                THROW(ErrSys, "socket()");
	}
	fd.set(s);
}

/**
 * Get raw fd
 *
 * @return file descriptor
 */
int
Socket::getfd() const
{
	return fd.get();
}

void
Socket::setfd(int n) throw()
{
	return fd.set(n);
}

/**
 * Forget about underlying file descriptor (don't close at object destruction)
 */
void
Socket::forget()
{
	fd.forget();
}


/**
 *
 */
void
Socket::set_close_on_exec(bool onoff)
{
        fd.set_close_on_exec(onoff);
}


/**
 * set/unset socket option SO_REUSEADDR
 *
 * @param[in] ion Turn on (true) or off (false) the option
 */
void
Socket::set_reuseaddr(bool ion)
{
	int on = !!ion;
	if (0 > setsockopt(fd.get(),
			   SOL_SOCKET,
			   SO_REUSEADDR,
			   &on,sizeof(on))) {
                THROW(ErrSys, "setsockopt(SO_REUSEADDR)");
	}
}

/**
 * Connect to a hostname or address. Address agnostic if af is AF_UNSPEC.
 *
 * RFC3484:
 *   Well-behaved applications SHOULD iterate through the list of
 *   addresses returned from getaddrinfo() until they find a working
 *   address.
 *
 * @param[in] af AF_UNSPEC, AF_INET or AF_INET6. Others may work to.
 * @param[in] host Hostname to connect to
 * @param[in] port Port name or number
 *
 * @todo Some debug logging for the connect() that fail
 */
void
Socket::connect(int af, const std::string &host, const std::string &port)
{
	struct addrinfo hints;
	int err;

	memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_family = af;
        hints.ai_socktype = SOCK_STREAM;

	GetAddrInfo gai(host, port, &hints);
        const struct addrinfo *p;
        err = -1;
        for (p = gai.get_results(); p; p = p->ai_next) {
                create_socket(p);
                err = ::connect(fd.get(), p->ai_addr, p->ai_addrlen);
                if (!err) {
                        break;
                }
        }
	if (0 > err) {
                THROW(ErrSys, "connect()");
	}
        connected_af_ = p->ai_addr->sa_family;
        set_tcp_md5_sock();
}

int
Socket::accept()
{
        struct sockaddr_storage sa;
        socklen_t salen(sizeof(sa));
        int newfd;

        newfd = ::accept(fd.get(), (struct sockaddr*)&sa, &salen);
        if (-1 == newfd) {
                THROW(ErrSys, "accept()");
        }
        return newfd;
}


/**
 * Listen to port on all interfaces
 *
 * @param[in] af Address family (AF_*). Should be AF_UNSPEC.
 * @param[in] port Port name or number.
 */
void
Socket::listen(int af, const std::string &host, const std::string &port)
{
	int err;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
        hints.ai_family = af;
        hints.ai_socktype = SOCK_STREAM;

	GetAddrInfo gai(host, port, &hints);
	const struct addrinfo *p;

        err = -1;
        for (p = gai.get_results(); p; p = p->ai_next) {
                create_socket(p);
                set_reuseaddr(true);

                err = bind(fd.get(), p->ai_addr, p->ai_addrlen);
                if (!err) {
                        break;
                }
        }
	if (err) {
                THROW(ErrSys, "bind()");
	}

	if (::listen(fd.get(), 5)) {
                THROW(ErrSys, "listen()");
	}
}

/**
 * Close socket
 */
void
Socket::close()
{
        fd.close();
}

/**
 * Destructor
 */
Socket::~Socket() throw()
{
        close();
}

/**
 * read at most 'm' bytes from socket.
 *
 * Throw error if something goes wrong.
 *
 * @param[in] m Read no more than this many bytes
 * @return Data read
 */
std::string
Socket::read(size_t m)
{
	return fd.read(m);
}

/**
 * Write some data to the socket
 *
 * @param[in] data Data to write
 * @return How many bytes were actually written
 */
size_t
Socket::write(const std::string &data)
{
	return fd.write(data);
}

/**
 * Write exactly all of a std::string
 *
 * Can't use fd.full_write() since this->write() may be from a subclass
 *
 * @param[in] data Data to write
 */
void
Socket::full_write(const std::string &data)
{
        size_t n;
        for (n = 0; n < data.size();) {
                n += write(data.substr(n));
        }
}

/**
 * set/unset TCP_NODELAY
 *
 * @param[in] on Turn on (true) or off (false)
 */
void
Socket::set_nodelay(bool on)
{
        int parm = !!on;
        if (-1 == setsockopt(fd.get(), IPPROTO_TCP, TCP_NODELAY, &parm,
                             sizeof(parm))) {
                THROW(ErrSys, "setsockopt(TCP_NODELAY)");
        }
}

/**
 * set/unset SO_KEEPALIVE
 *
 * @param[in] on Turn on (true) or off (false)
 */
void
Socket::set_keepalive(bool on)
{
        int parm = !!on;
        if (-1 == setsockopt(fd.get(), SOL_SOCKET, SO_KEEPALIVE, &parm,
                             sizeof(parm))) {
                THROW(ErrSys, "setsockopt(SO_KEEPALIVE)");
        }
}

/**
 *
 */
void
Socket::set_tos(int tos)
{
        switch (connected_af_) {
        case AF_INET:
                if (-1 == setsockopt(fd.get(), IPPROTO_IP, IP_TOS, &tos,
                                     sizeof(tos))) {
                        THROW(ErrSys, "setsockopt(IP_TOS)");
                }
                break;
        case AF_INET6:
                if (-1 == setsockopt(fd.get(), SOL_IPV6, IPV6_TCLASS, &tos,
                                     sizeof(tos))) {
                        THROW(ErrSys, "setsockopt(IPV6_TCLASS)");
                }
                break;
        default:
                THROW(ErrBase, "connected_af_ neither AF_INET or AF_INET6");
        }
}

/**
 * set TCP MD5 key
 *
 * @param[in] keystring Key to set
 */
void
Socket::set_tcp_md5(const std::string &keystring)
{
        tcpmd5 = keystring;
}

/**
 * Enable the TCP MD5 key set via set_tcp_md5() on the socket
 *
 * @todo This is temporarily disabled. I think this makes Linux crash
 */
void
Socket::set_tcp_md5_sock()
{
        return;
#ifdef HAVE_TCP_MD5
        struct tcp_md5sig md5sig;
        std::string key = tcpmd5.substr(0, TCP_MD5SIG_MAXKEYLEN);
        socklen_t t = sizeof(struct sockaddr_storage);

        memset(&md5sig, 0, sizeof(md5sig));
        if (getpeername(fd.get(),
                        (struct sockaddr*)&md5sig.tcpm_addr, &t)) {
                THROW(ErrSys, "getpeername()");
        }
        md5sig.tcpm_keylen = key.size();
        memcpy(md5sig.tcpm_key, key.data(), md5sig.tcpm_keylen);
        if (-1 == setsockopt(fd.get(),
                             IPPROTO_TCP, TCP_MD5SIG,
                             &md5sig, sizeof(md5sig))) {
                if (ENOENT == errno) {
                        // when we set no key
                } else {
                        THROW(ErrSys, "setsockopt(TCP_MD5SIG)");
                }
        }
#endif
}

/**
 * Get peer address as string, in numeric format.
 */
std::string
Socket::get_peer_addr_string() const
{
        struct sockaddr_storage sa;
        socklen_t salen = sizeof(sa);
        std::string ret;
        if (getpeername(fd.get(), (struct sockaddr*)&sa, &salen)) {
                THROW(ErrSys, "getpeername()");
        }

        char host[1024];
        if (getnameinfo((struct sockaddr*)&sa, salen,
                        host, sizeof(host),
                        NULL, 0,
                        NI_NUMERICHOST)) {
                THROW(ErrSys, "getnameinfo()");
        }
        ret = host;

        if (ret.substr(0,7) == "::ffff:") {
                ret = ret.substr(7);
        }
        return ret;
}

/* ---- Emacs Variables ----
 * Local Variables:
 * c-basic-offset: 8
 * indent-tabs-mode: nil
 * End:
 */
