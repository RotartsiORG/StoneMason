#pragma once

#ifndef __STONEMASON_NET_PLAIN_UDP_HPP
#define __STONEMASON_NET_PLAIN_UDP_HPP

#include "stms/net/plain.hpp"

namespace stms {
    class UDPPeer : public _stms_PlainBase {
    private:
        std::function<void(const sockaddr *const, socklen_t, uint8_t *, ssize_t)> recvCallback = [](
                const sockaddr *const, socklen_t, uint8_t *, ssize_t) {};
        
        bool isReading = false;

    public:
        UDPPeer(bool isServ, PoolLike *pool);
        ~UDPPeer() override = default;

        UDPPeer(const UDPPeer &rhs) = delete;
        UDPPeer &operator=(const UDPPeer &rhs) = delete;

        UDPPeer(UDPPeer &&rhs) noexcept;
        UDPPeer &operator=(UDPPeer &&rhs) noexcept;

        inline void setRecvCallback(const std::function<void(const sockaddr *const, socklen_t, uint8_t *, ssize_t)> &n) {
            recvCallback = n;
        }

        // only relavent to connect'd clients
        inline std::future<int> send(const uint8_t *const data, size_t size, bool copy = false) {
            return sendTo(nullptr, 0, data, size, copy);
        }

        /**
         * @brief Send bytes to another peer
         * 
         * @param addr Address to send bytes to
         * @param addrlen Length of address struct, obtained in recvCallback.
         * @param data Data to send
         * @param size Number of bytes to send from data pointer
         * @param copy If true, `data` is copied. Otherwise, it is assumed that `data` will still
         *             be there when it is read from directly on another thread.
         * @return std::future<int> Future that can be used to block until the operation is complete. If there is an error, 
         *                          a negative value is returned; otherwise, the number of bytes sent is returned. 
         *                          -1 is returned if the `UDPPeer` has not been started. -3 is returned if the operation times out.
         */
        std::future<int> sendTo(const sockaddr *const addr, socklen_t addrlen, const uint8_t *const data, size_t size, bool copy = false);

        void waitEvents(int toMs) override;

        bool tick();
    };
}

#endif