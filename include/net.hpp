#ifndef NET_HPP
#define NET_HPP

#include <boost/asio.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#include "messages.hpp"

// Decodes a datagram into a wire message, but only when it is exactly the right
// size — which also discriminates the two message types, since ServerState and
// ClientSlide have different wire sizes. fromWire() reads each field big-endian
// one byte at a time, so decoding does not depend on this host's byte order.
template <typename TMsg>
std::optional<TMsg> decodeMsg(const std::array<std::byte, 1200>& aBuf, std::size_t aN)
{
    if (aN != TMsg::kWireSize)
        return std::nullopt;
    return Messages::fromWire<TMsg>(aBuf.data());
}

// Which end of the connection a NetChannel is. Selects the send/receive message
// directions and the handful of behavioural asymmetries between the two roles.
enum class Role { Client, Server };

// One UDP endpoint, parameterised by Role. Client and Server share almost
// everything — a single socket used for both directions, a background receiver
// thread that keeps only the newest decoded frame in an atomic mailbox, and
// sequence-number de-duplication — so this is one class template and the few
// differences are resolved at compile time (if constexpr / constrained members)
// rather than duplicated into two near-identical classes:
//
//   * direction      — Client sends ClientSlide / receives ServerState; Server
//                      is the mirror. TxMsg/RxMsg are selected from Role.
//   * peer endpoint  — Client resolves a fixed server address up front; Server
//                      has none at startup and latches the client's address from
//                      the first datagram it receives.
//   * socket setup   — Client opens an unbound socket (the kernel picks the local
//                      port on first send); Server binds the well-known port.
//   * receiver start — Server starts the thread immediately (it must be ready to
//                      learn the client); Client starts it on the first send.
//   * send gating    — Server no-ops until it has learned a client; Client can
//                      always send to its fixed server.
//
// The game thread only calls send()/poll(); the receiver thread only calls
// receive_from. Concurrent send_to + receive_from on one UDP socket is safe at
// the OS level — they are independent syscalls on the same descriptor.
template <Role R>
class NetChannel
{
    static constexpr bool kIsServer = (R == Role::Server);

    // The server sends authoritative state and receives the client's slide; the
    // client is the mirror image. Everything else is symmetric in these aliases.
    using TxMsg = std::conditional_t<kIsServer, Messages::ServerState, Messages::ClientSlide>;
    using RxMsg = std::conditional_t<kIsServer, Messages::ClientSlide, Messages::ServerState>;

public:
    // Client: resolve and remember the server endpoint; open an unbound socket.
    NetChannel(std::string_view aServerHost, std::uint16_t aServerPort)
        requires (!kIsServer)
        : mPeer{resolvePeer(mIoContext, aServerHost, aServerPort)},
          mSocket{mIoContext}
    {
        mSocket.open(boost::asio::ip::udp::v4());  // kernel assigns a local port on first send
    }

    // Server: bind the well-known port and start receiving immediately, so the
    // client's very first datagram is captured.
    explicit NetChannel(std::uint16_t aLocalPort)
        requires (kIsServer)
        : mSocket{mIoContext,
                  boost::asio::ip::udp::endpoint{boost::asio::ip::udp::v4(), aLocalPort}},
          mThread{[this](std::stop_token aStop) { recvLoop(aStop); }}  // mThread is last
    {}

    ~NetChannel()
    {
        boost::system::error_code ec;
        mSocket.close(ec);  // unblock receive_from so mThread can join (no-op if never started)
    }

    NetChannel(const NetChannel&)            = delete;  // owns a socket + thread
    NetChannel& operator=(const NetChannel&) = delete;

    // Game thread. Serialises and sends one frame to the peer. The server no-ops
    // until it has learned a client; the client lazily starts its receiver on the
    // first send. Caller owns aMsg.mOrderId — increment it per call.
    void send(const TxMsg& aMsg)
    {
        if constexpr (kIsServer)
        {
            if (!mHasPeer.load(std::memory_order_acquire))  // acquire also publishes mPeer
                return;
        }

        std::array<std::byte, TxMsg::kWireSize> lOut;
        Messages::toWire(aMsg, lOut.data());
        mSocket.send_to(boost::asio::buffer(lOut.data(), lOut.size()), mPeer);

        if constexpr (!kIsServer)
        {
            if (!mThread.joinable())
                mThread = std::jthread{[this](std::stop_token aStop) { recvLoop(aStop); }};
        }
    }

    // Game thread, non-blocking. Writes the newest received frame into aMsg and
    // returns true only when one newer than the last poll has arrived; otherwise
    // leaves aMsg untouched and returns false.
    bool poll(RxMsg& aMsg)
    {
        const RxMsg lLatest = mLatest.load(std::memory_order_acquire);
        if (lLatest.mOrderId <= mLastSeenOrderId)
            return false;
        mLastSeenOrderId = lLatest.mOrderId;
        aMsg = lLatest;
        return true;
    }

    // Game thread. Server only: true once the client has been seen (a frame was
    // received), after which send() will deliver to it.
    [[nodiscard]] bool hasClient() const noexcept
        requires (kIsServer)
    {
        return mHasPeer.load(std::memory_order_acquire);
    }

private:
    // Resolves host (numeric IP *or* hostname like "localhost") + port to a v4
    // UDP endpoint. make_address only parses numeric IPs; a resolver also handles
    // names. Throws if the host cannot be resolved. (Client role only.)
    static boost::asio::ip::udp::endpoint resolvePeer(
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

            auto lMsg = decodeMsg<RxMsg>(mRecvBuf, lN);
            if (!lMsg)
                continue;

            if constexpr (kIsServer)
            {
                // Latch the client's endpoint from the first valid datagram so
                // send() can reply to it. Written once; the release pairs with
                // the acquire in send()/hasClient().
                if (!mHasPeer.load(std::memory_order_acquire))
                {
                    mPeer = lFrom;
                    mHasPeer.store(true, std::memory_order_release);
                }
            }

            if (lMsg->mOrderId <= mLastOrderId)  // drop stale / duplicate
                continue;
            mLastOrderId = lMsg->mOrderId;
            mLatest.store(*lMsg, std::memory_order_release);  // keep only the newest
        }
    }

    boost::asio::io_context        mIoContext;
    // Client: the fixed server address, set in the ctor. Server: the client
    // address, learned once by the receiver thread and published via mHasPeer.
    boost::asio::ip::udp::endpoint mPeer;
    std::atomic<bool>              mHasPeer{false};  // server only; unused on the client
    boost::asio::ip::udp::socket   mSocket;
    std::array<std::byte, 1200>    mRecvBuf{};
    // Single-slot mailbox. orderId -1 means "nothing received yet".
    std::atomic<RxMsg>             mLatest{RxMsg{.mOrderId = -1}};
    std::int32_t                   mLastOrderId{-1};      // net thread only
    std::int32_t                   mLastSeenOrderId{-1};  // game thread only
    std::jthread                   mThread;               // must be last
};

// The two roles. Client must always be given a server address (no default
// constructor exists, since the constructors above are user-declared).
using Client = NetChannel<Role::Client>;
using Server = NetChannel<Role::Server>;

#endif  // NET_HPP
