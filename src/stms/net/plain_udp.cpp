#include "stms/net/plain_udp.hpp"

#include <sys/socket.h>
#include <poll.h>
#include "stms/logging.hpp"

namespace stms {
    UDPPeer::UDPPeer(bool is, PoolLike *p) : _stms_PlainBase(is, p, true) {};

    UDPPeer::UDPPeer(UDPPeer &&rhs) noexcept {
        *this = std::move(rhs);
    }

    UDPPeer &UDPPeer::operator=(UDPPeer &&rhs) noexcept {
        if (this == &rhs) {
            return *this;
        }
        
        isReading = rhs.isReading;
        recvCallback = rhs.recvCallback;
        movePlain(&rhs);

        return *this;
    }

    bool UDPPeer::tick() {
        if (!running) {
            STMS_WARN("UDPPeer::tick() called when stopped! Ignoring invocation.");
            return false;
        }

        if (recvfrom(sock, nullptr, 0, MSG_PEEK, nullptr, nullptr) == -1
             && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return running;  // No data could be read
        }

        if (!isReading) {
            isReading = true;
            pPool->submitTask([&, capThis{this}, capSock{sock}]() {

                int numTries = 0;
                while (numTries < maxTimeouts) {
                    numTries++;

                    sockaddr_storage fromAddr{};
                    socklen_t addrSize = sizeof(sockaddr_storage);

                    uint8_t buf[maxPlainRecvLen];
                    ssize_t recv = recvfrom(capSock, buf, maxPlainRecvLen, MSG_WAITALL, reinterpret_cast<sockaddr *>(&fromAddr), &addrSize);
                    if (recv == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            STMS_INFO("Read operation failed: EAGAIN/EWOULDBLOCK. Retrying (Attempt #{}).", numTries);
                            pollfd params{};
                            params.fd = capSock;
                            params.events = POLLIN;

                            poll(&params, 1, static_cast<int>(timeoutMs < minIoTimeout ? minIoTimeout : timeoutMs));
                            continue;
                        }

                        STMS_WARN("recvfrom failed with errno {}: {}", errno, strerror(errno));
                    } else {
                        capThis->recvCallback(reinterpret_cast<sockaddr *>(&fromAddr), addrSize, buf, recv);
                        numTries = 0;
                        break;
                    }
                }

                if (numTries >= maxTimeouts) {
                    STMS_WARN("Read operation failed completely!");
                }

                capThis->isReading = false;
            });
        }

        return running;
    }

    std::future<int> UDPPeer::sendTo(const sockaddr *const addr, socklen_t addrlen, const uint8_t *const data, size_t size, bool copy) {
        std::shared_ptr<std::promise<int>> pProm = std::make_shared<std::promise<int>>();
        if (!running) {
            STMS_ERROR("UDPPeer::sendTo called when stopped! {} bytes dropped.", size);
            pProm->set_value(-1);
            return pProm->get_future();
        }

        uint8_t *cpdata = const_cast<uint8_t *>(data);
        if (copy) {
            cpdata = new uint8_t[size];
            std::copy(data, data + size, cpdata);
        }

        pPool->submitTask([&, capData{cpdata}, capCpy(copy), capSize{size}, capSock{sock},
                           capAddr{addr}, capLen{addrlen}, capProm{pProm}]() {

            int numTries = 0;
            while (numTries < maxTimeouts) {
                numTries++;

                int sent = ::sendto(capSock, capData, capSize, 0, capAddr, capLen);
                if (sent == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        STMS_INFO("UDPPeer::sendTo() failed: EAGAIN/EWOULDBLOCK. Retrying (Attempt #{}).", numTries);
                        pollfd params{};
                        params.fd = capSock;
                        params.events = POLLOUT;

                        poll(&params, 1, static_cast<int>(timeoutMs < minIoTimeout ? minIoTimeout : timeoutMs));
                        continue;
                    }

                    STMS_WARN("UDPPeer::sendTo() failed with errno {}: {}", errno, strerror(errno));
                } else {
                    capProm->set_value(sent);
                    numTries = 0;
                    break;
                }
            }

            if (capCpy) {
                delete[] capData;
            }

            if (numTries >= maxTimeouts) {
                STMS_WARN("UDPPeer::sendTo() timed out completely!");
                capProm->set_value(-3);
            }
        });

        return pProm->get_future();
    }

    void UDPPeer::waitEvents(int toMs) {
        pollfd params{};
        params.fd = sock;
        params.events = POLLIN;

        poll(&params, 1, toMs);
    };
}
