#ifndef NET_HPP
#define NET_HPP

#include <boost/asio.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "messages.hpp"

// Decodes a datagram into a SlidePosition, but only when it is exactly the right
// size. memcpy avoids the strict-aliasing UB of reinterpreting the byte buffer.
inline std::optional<Messages::SlidePosition> decodeSlide(
    const std::array<std::byte, 1200>& aBuf, std::size_t aN)
{
    if (aN != sizeof(Messages::SlidePosition))
        return std::nullopt;
    Messages::SlidePosition lMsg;
    std::memcpy(&lMsg, aBuf.data(), sizeof(lMsg));
    return lMsg;
}

// UDP client. Sends position frames to a fixed server endpoint and reads the
// server's replies without blocking the game loop.
//
// One socket serves both directions: the server replies to the source address
// of our datagrams, so replies arrive on the very socket we sent from. The game
// thread only calls send()/poll(); a background thread (started on the first
// send) only calls receive_from and keeps only the newest frame in an atomic
// slot. Concurrent send_to + receive_from on one UDP socket is safe at the OS
// level — they are independent syscalls on the same descriptor.
class Client
{
public:
    Client() = delete;  // a Client is meaningless without a server address

    Client(std::string_view aServerHost, std::uint16_t aServerPort)
        : mServer{resolveServer(mIoContext, aServerHost, aServerPort)},
          mSocket{mIoContext}
    {
        mSocket.open(boost::asio::ip::udp::v4());  // kernel assigns a local port on first send
    }

    ~Client()
    {
        boost::system::error_code ec;
        mSocket.close(ec);  // unblock receive_from; mThread joins (no-op if never started)
    }

    // Game thread. Sends one frame to the server and, on the first call, starts
    // the background receiver. Caller owns aMsg.mOrderId — increment it per call.
    void send(const Messages::SlidePosition& aMsg)
    {
        mSocket.send_to(boost::asio::buffer(&aMsg, sizeof(aMsg)), mServer);
        if (!mThread.joinable())
            mThread = std::jthread{[this](std::stop_token aStop) { recvLoop(aStop); }};
    }

    // Game thread, non-blocking. Writes the newest frame into aMsg and returns
    // true only when one newer than the last poll has arrived; otherwise leaves
    // aMsg untouched and returns false.
    bool poll(Messages::SlidePosition& aMsg)
    {
        const Messages::SlidePosition lLatest = mLatest.load(std::memory_order_acquire);
        if (lLatest.mOrderId <= mLastSeenOrderId)
            return false;
        mLastSeenOrderId = lLatest.mOrderId;
        aMsg = lLatest;
        return true;
    }

private:
    // Resolves host (numeric IP *or* hostname like "localhost") + port to a v4
    // UDP endpoint. make_address only parses numeric IPs; a resolver also
    // handles names. Throws if the host cannot be resolved.
    static boost::asio::ip::udp::endpoint resolveServer(
        boost::asio::io_context& aIo, std::string_view aHost, std::uint16_t aPort)
    {
        boost::asio::ip::udp::resolver lResolver{aIo};
        return *lResolver.resolve(boost::asio::ip::udp::v4(),
                                  std::string{aHost}, std::to_string(aPort)).begin();
    }

    void recvLoop(std::stop_token aStop)
    {
        while (!aStop.stop_requested())
        {
            boost::asio::ip::udp::endpoint lFrom;
            std::size_t lN{0};
            try { lN = mSocket.receive_from(boost::asio::buffer(mRecvBuf), lFrom); }
            catch (const boost::system::system_error&) { break; }  // socket closed

            auto lMsg = decodeSlide(mRecvBuf, lN);
            if (!lMsg || lMsg->mOrderId <= mLastOrderId)  // drop bad / stale / duplicate
                continue;
            mLastOrderId = lMsg->mOrderId;
            mLatest.store(*lMsg, std::memory_order_release);  // keep only the newest
        }
    }

    boost::asio::io_context              mIoContext;
    boost::asio::ip::udp::endpoint       mServer;
    boost::asio::ip::udp::socket         mSocket;
    std::array<std::byte, 1200>          mRecvBuf{};
    std::atomic<Messages::SlidePosition> mLatest{Messages::SlidePosition{-1, 0, 0}};
    std::int32_t                         mLastOrderId{-1};      // net thread only
    std::int32_t                         mLastSeenOrderId{-1};  // game thread only
    std::jthread                         mThread;               // must be last
};

// UDP server. Receives client position frames without blocking the game loop
// and sends frames back to the connected client.
//
// The background receiver runs immediately, binds the well-known port, learns
// the client's endpoint from the first datagram, and keeps only the newest
// frame. send() replies to that learned endpoint. Same single-socket / OS-level
// thread-safety note as Client.
class Server
{
public:
    explicit Server(std::uint16_t aLocalPort)
        : mSocket{mIoContext,
                  boost::asio::ip::udp::endpoint{boost::asio::ip::udp::v4(), aLocalPort}},
          mThread{[this](std::stop_token aStop) { recvLoop(aStop); }}  // must be last
    {}

    ~Server()
    {
        boost::system::error_code ec;
        mSocket.close(ec);  // unblock receive_from; mThread joins
    }

    // Game thread. True once a client has been seen (a frame was received).
    [[nodiscard]] bool hasClient() const noexcept
    {
        return mHasClient.load(std::memory_order_acquire);
    }

    // Game thread. Sends one frame to the connected client; no-ops until a
    // client is known. Caller owns aMsg.mOrderId — increment it per call.
    void send(const Messages::SlidePosition& aMsg)
    {
        if (!mHasClient.load(std::memory_order_acquire))  // acquire also publishes mClient
            return;
        mSocket.send_to(boost::asio::buffer(&aMsg, sizeof(aMsg)), mClient);
    }

    // Game thread, non-blocking. As Client::poll.
    bool poll(Messages::SlidePosition& aMsg)
    {
        const Messages::SlidePosition lLatest = mLatest.load(std::memory_order_acquire);
        if (lLatest.mOrderId <= mLastSeenOrderId)
            return false;
        mLastSeenOrderId = lLatest.mOrderId;
        aMsg = lLatest;
        return true;
    }

private:
    void recvLoop(std::stop_token aStop)
    {
        while (!aStop.stop_requested())
        {
            boost::asio::ip::udp::endpoint lFrom;
            std::size_t lN{0};
            try { lN = mSocket.receive_from(boost::asio::buffer(mRecvBuf), lFrom); }
            catch (const boost::system::system_error&) { break; }  // socket closed

            auto lMsg = decodeSlide(mRecvBuf, lN);
            if (!lMsg)
                continue;

            // Latch the client's full endpoint from the first valid datagram so
            // send() can reply to it. Written once; the release pairs with the
            // acquire in send()/hasClient().
            if (!mHasClient.load(std::memory_order_acquire))
            {
                mClient = lFrom;
                mHasClient.store(true, std::memory_order_release);
            }

            if (lMsg->mOrderId <= mLastOrderId)  // drop stale / duplicate
                continue;
            mLastOrderId = lMsg->mOrderId;
            mLatest.store(*lMsg, std::memory_order_release);  // keep only the newest
        }
    }

    boost::asio::io_context              mIoContext;
    boost::asio::ip::udp::endpoint       mClient;            // write-once (net thread)
    std::atomic<bool>                    mHasClient{false};  // gates cross-thread reads of mClient
    boost::asio::ip::udp::socket         mSocket;
    std::array<std::byte, 1200>          mRecvBuf{};
    std::atomic<Messages::SlidePosition> mLatest{Messages::SlidePosition{-1, 0, 0}};
    std::int32_t                         mLastOrderId{-1};      // net thread only
    std::int32_t                         mLastSeenOrderId{-1};  // game thread only
    std::jthread                         mThread;               // must be last
};

#endif  // NET_HPP
