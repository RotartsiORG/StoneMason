#include "stms/net/net.hpp"

#include "stms/logging.hpp"

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>

namespace stms {
    std::string getAddrStr(const sockaddr *const addr) {
        if (addr->sa_family == AF_INET) {
            auto *v4Addr = reinterpret_cast<const sockaddr_in *>(addr);
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v4Addr->sin_addr), ipStr, INET_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v4Addr->sin_port));
        } else if (addr->sa_family == AF_INET6) {
            auto *v6Addr = reinterpret_cast<const sockaddr_in6 *>(addr);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(addr->sa_family, &(v6Addr->sin6_addr), ipStr, INET6_ADDRSTRLEN);
            return std::string(ipStr) + ":" + std::to_string(ntohs(v6Addr->sin6_port));
        }
        return "[ERR: BAD FAM]";
    }

    _stms_PlainBase::_stms_PlainBase(bool isServ, PoolLike *pool, bool isUdp) : isServ(isServ), 
    isUdp(isUdp), pPool(pool) {
        setHostAddr();
    }

    _stms_PlainBase::~_stms_PlainBase() {
        freeaddrinfo(pAddrCandidates);
    }

    _stms_PlainBase &_stms_PlainBase::operator=(_stms_PlainBase &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }

        movePlain(&rhs);

        return *this;
    }

    _stms_PlainBase::_stms_PlainBase(_stms_PlainBase &&rhs) noexcept {
        *this = std::move(rhs);
    }

    void _stms_PlainBase::movePlain(_stms_PlainBase *rhs) {
        freeaddrinfo(pAddrCandidates);

        isServ = rhs->isServ;
        isUdp = rhs->isUdp;
        wantV6 = rhs->wantV6;
        addrStr = std::move(rhs->addrStr);
        pAddrCandidates = rhs->pAddrCandidates;
        pAddr = rhs->pAddr;
        sock = rhs->sock;
        timeoutMs = rhs->timeoutMs;
        maxTimeouts = rhs->maxTimeouts;
        running = rhs->running;
        pPool = rhs->pPool;

        rhs->pAddr = nullptr;
        rhs->pAddrCandidates = nullptr;
        rhs->sock = 0;
    }

    void _stms_PlainBase::setHostAddr(const std::string &port, const std::string &addr) {
        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;

        if (isUdp) {
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;
        } else {
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
        }

        if (addr == "any") {
            hints.ai_flags = AI_PASSIVE;
        }

        int lookupStatus = getaddrinfo(addr == "any" ? nullptr : addr.c_str(), port.c_str(), &hints, &pAddrCandidates);
        if (lookupStatus != 0) {
            STMS_WARN("Failed to resolve ip address of {}:{} (Errno: {})", addr, port, gai_strerror(lookupStatus));
        }
    }

    void _stms_PlainBase::start() {
        if (running) {
            STMS_WARN("_stms_PlainBase::start() called when server/client was already started! Ignoring...");
            return;
        }

        if (!pPool->isRunning()) {
            STMS_ERROR("_stms_PlainBase::start() called with stopped ThreadPool! Starting the thread pool now!");
            pPool->start();
        }

        int acc = 0, i = 0;
        pAddr = nullptr;
        for (addrinfo *p = pAddrCandidates; p != nullptr; p = p->ai_next) {

            if (tryAddr(p, i)) {
                acc = i;
                if ((p->ai_family == AF_INET6 && wantV6) || (p->ai_family == AF_INET && !wantV6)) {
                    STMS_INFO("Using Candidate {} because it is IPv{}", i, p->ai_family == AF_INET ? '4' : '6');
                    running = true;
                    onStart();
                    return;
                }
            }

            i++;
        }

        if (!pAddr) {
            STMS_WARN("No IP addresses resolved from supplied address and port can be used!");
            stop();
        } else {
            STMS_INFO("Using Candidate {} as best fit.", acc);
            running = true;
            onStart();
        }
    }

    bool _stms_PlainBase::tryAddr(addrinfo *addr, int num) {
        int on = 1;

        addrStr = getAddrStr(addr->ai_addr);
        STMS_INFO("Candidate {}: IPv{} {}", num, addr->ai_family == AF_INET6 ? 6 : 4, addrStr);

        if (sock != 0) {
            if (close(sock) == -1) {
                STMS_WARN("Failed to close socket: {}", strerror(errno));
            }
            sock = 0;
        }

        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == -1) {
            STMS_WARN("Candidate {}: Unable to create socket: {}", num, strerror(errno));
            return false;
        }

        if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
            STMS_WARN("Candidate {}: Failed to set socket to non-blocking: {}", num, strerror(errno));
        }

        if (addr->ai_family == AF_INET6) {
            if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(int)) == -1) {
                STMS_WARN("Candidate {}: Failed to setsockopt to allow IPv4 connections: {}", num, strerror(errno));
            }
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int)) == -1) {
            STMS_WARN(
                    "Candidate {}: Failed to setscokopt to reuse address (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(int)) == -1) {
            STMS_WARN(
                    "Candidate {}: Failed to setsockopt to reuse port (this may result in 'Socket in Use' errors): {}",
                    num, strerror(errno));
        }

        if (isServ) {
            if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1) {
                STMS_WARN("Candidate {}: Unable to bind socket: {}", num, strerror(errno));
                return false;
            }

            if (!isUdp) {
                STMS_INFO("listen");
                if (listen(sock, tcpListenBacklog) == -1) {
                    STMS_WARN("Candidate {}: listen() failed: {}", num, strerror(errno));
                }
            }
        } else {
            if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1) {

                /*
                 * EINPROGRESS (see `man connect`)
                 *
                 * The socket is nonblocking and the connection cannot be completed
                 * immediately.   (UNIX domain sockets failed with EAGAIN instead.)
                 * It is possible to select(2) or poll(2) for completion by select‐
                 * ing the socket for writing.  After select(2) indicates writabil‐
                 * ity, use getsockopt(2) to read  the  SO_ERROR  option  at  level
                 * SOL_SOCKET to determine whether connect() completed successfully
                 * (SO_ERROR is zero) or unsuccessfully (SO_ERROR  is  one  of  the
                 * usual  error  codes  listed  here, explaining the reason for the
                 * failure).
                 */
                if (errno == EINPROGRESS) { // should we also be testing for EAGAIN?
                    pollfd toPoll{};
                    toPoll.events = POLLOUT;
                    toPoll.fd = sock;

                    if (poll(&toPoll, 1, static_cast<int>(timeoutMs < minIoTimeout ? minIoTimeout : timeoutMs)) < 1) {
                        STMS_WARN("Candidate {}: connect() timed out!", num);
                        return false;
                    }

                    int errcode = -999;
                    socklen_t errlen = sizeof(int);
                    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &errcode, &errlen) == -1) {
                        STMS_WARN("Candidate {}: connect() state querying failed: {}", num, strerror(errno));
                        return false;
                    }

                    if (errcode != 0) {
                        STMS_WARN("Candidate {}: connect() failed: {}", num, strerror(errcode));
                    }

                    // yay connect was successful!
                } else {
                    STMS_WARN("Candidate {}: Failed to connect socket: {}", num, strerror(errno));
                    return false;
                }
            }
        }

        pAddr = addr;
        STMS_INFO("Candidate {} is viable and active. However, it is not necessarily preferred.", num);
        return true;
    }

    void _stms_PlainBase::stop() {
        if (!running) {
            STMS_WARN("_stms_PlainBase::stop() called when server/client was already stopped! Ignoring...");
            return;
        }

        running = false;
        onStop();
        if (sock == 0) {
            STMS_INFO("Server/client stopped. Resources freed. (Skipped socket as fd was 0)");
            return;
        }

        if (close(sock) == -1) {
            STMS_INFO("Failed to close server/client socket: {}", strerror(errno));
        }
        sock = 0;
        STMS_INFO("Server/client stopped. Resources freed.");
    }

}