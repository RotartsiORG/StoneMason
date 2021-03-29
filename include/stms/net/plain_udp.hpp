#pragma once

#ifndef __STONEMASON_NET_PLAIN_UDP_HPP
#define __STONEMASON_NET_PLAIN_UDP_HPP
//!< Include guard

#include "stms/net/net.hpp"

namespace stms {
    /**
     * @brief Class for UDP Client or server.
     */
    class UDPPeer : public _stms_PlainBase {
    private:

        /**
         * @brief Callback to be called whenever data is received.
         * Parameters:
         *      `const sockaddr *const`: The address the packet originated from
         *      `socklen_t`: Size in bytes of the first parameter. Varies depending on IPv4 or IPv6
         *      `uint8_t *`: Buffer containing raw data of packet
         *      `ssize_t`: Number of bytes received (length of the previous prameter)
         */
        std::function<void(const sockaddr *const, socklen_t, uint8_t *, ssize_t)> recvCallback = [](
                const sockaddr *const, socklen_t, uint8_t *, ssize_t) {};
        
        bool isReading = false; //!< Internal implementation detail. True if data is being received from a peer.

    public:
        /**
         * @brief Construct a new UDPPeer object
         * @param isServ If this object should act as a server or client. If true, `bind` is called to the address
         *               set in `setHostAddr()`. If false, `connect` is called to that address.
         * @param pool The PookLike object to submit tasks to.
         */
        UDPPeer(bool isServ, PoolLike *pool);
        ~UDPPeer() override = default; //!< Default destructor

        UDPPeer(const UDPPeer &rhs) = delete; //!< Deleted copy constructor
        UDPPeer &operator=(const UDPPeer &rhs) = delete; //!< Deleted copy assignment operator

        UDPPeer(UDPPeer &&rhs) noexcept; //!< Move constructor
        UDPPeer &operator=(UDPPeer &&rhs) noexcept; //!< Move assignment operator.

        /**
         * @brief Set the `recvCallback` object
         * @param n New callback (see `recvCallback`)
         */
        inline void setRecvCallback(const std::function<void(const sockaddr *const, socklen_t, uint8_t *, ssize_t)> &n) {
            recvCallback = n;
        }

        /**
         * @brief Send data to the server. Only works in client `UDPPeer`s that have `connect`ed. 
         * This is an alias for `sendTo(nullptr, 0, data, size, copy);`
         * @param data Pointer to the data to send
         * @param size Number of bytes from `data` to send
         * @param copy If true, `data` is copied. Otherwise, it is assumed that `data` will still
         *             be there when it is read from directly on another thread.
         * @return std::future<int> Future that can be used to block until the operation is complete. If there is an error, 
         *                          a negative value is returned; otherwise, the number of bytes sent is returned. 
         *                          -1 is returned if the `UDPPeer` has not been started. -3 is returned if the operation times out.
         */
        inline std::future<int> send(const uint8_t *const data, size_t size, bool copy = false) {
            return sendTo(nullptr, 0, data, size, copy);
        }

        /**
         * @brief Send bytes to another peer
         * 
         * @param addr Address to send bytes to
         * @param addrlen Length of address struct, obtained in `recvCallback`.
         * @param data Data to send
         * @param size Number of bytes to send from data pointer
         * @param copy If true, `data` is copied. Otherwise, it is assumed that `data` will still
         *             be there when it is read from directly on another thread.
         * @return std::future<int> Future that can be used to block until the operation is complete. If there is an error, 
         *                          a negative value is returned; otherwise, the number of bytes sent is returned. 
         *                          -1 is returned if the `UDPPeer` has not been started. -3 is returned if the operation times out.
         */
        std::future<int> sendTo(const sockaddr *const addr, socklen_t addrlen, const uint8_t *const data, size_t size, bool copy = false);

        /**
         * @brief Block until there is data from a peer to process or until `toMs` runs out.
         * @param toMs Maximum number of milliseconds to block.
         */
        void waitEvents(int toMs) override;

        /**
         * @brief Process incoming data from peers.
         * @return true The `UDPPeer` is still running and should keep being ticked.
         * @return false The `UDPPeer` has stopped.
         */
        bool tick();
    };
}

#endif