#ifndef MESSAGES_HPP
#define  MESSAGES_HPP
#include <arpa/inet.h>  // htonl/htons/ntohl/ntohs (POSIX byte-order macros)
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>      // std::memcpy
namespace Messages
{
    // Game-state flags carried in ServerState. These are PERSISTENT STATE, not
    // one-frame events: the transport keeps only the newest frame, so a flag
    // raised for a single message could be dropped or overwritten unseen. Once
    // a flag becomes true the server must keep setting it in every message.
    // Perspective is fixed: "client" always means the //:client binary.
    namespace Flags
    {
        inline constexpr std::int8_t kNone       {0};
        inline constexpr std::int8_t kClientWon  {1 << 0};
        inline constexpr std::int8_t kClientLost {1 << 1};
    }

    // The structs below are plain HOST-ORDER, in-memory messages — NOT the wire
    // layout. The bytes that actually travel are produced by toWire() and parsed
    // by fromWire(), which encode every multi-byte field big-endian one byte at
    // a time. That makes the on-wire form independent of host endianness, so a
    // little-endian and a big-endian peer interoperate. (These used to be packed
    // structs copied verbatim with memcpy, which silently assumed both ends
    // shared the host byte order — fine on x86/ARM-LE, wrong against a BE peer.)
    // kWireSize is the encoded size, which differs from sizeof() now that the
    // structs are no longer packed; it is what decodeMsg() validates against.

    // Server -> client: full authoritative state, sent every frame.
    struct ServerState
    {
        static constexpr std::size_t kWireSize{13};  // 4 + 1 + 2 + 2 + 2 + 2
        std::int32_t mOrderId;
        std::int8_t  mFlags;   // Flags::*, persistent state
        std::int16_t mBallX;
        std::int16_t mBallY;
        std::int16_t mSlideX;  // the server's own slide
        std::int16_t mSlideY;
    };

    // Client -> server: the only thing the client controls, its slide.
    struct ClientSlide
    {
        static constexpr std::size_t kWireSize{8};  // 4 + 2 + 2
        std::int32_t mOrderId;
        std::int16_t mSlideX;
        std::int16_t mSlideY;
    };

    static_assert(ServerState::kWireSize ==
                  sizeof(std::int32_t) + sizeof(std::int8_t) + 4 * sizeof(std::int16_t));
    static_assert(ClientSlide::kWireSize ==
                  sizeof(std::int32_t) + 2 * sizeof(std::int16_t));

    // Network-order (big-endian) read/write of one integral field over a moving
    // byte cursor. A single templated put/get dispatches on sizeof(T) with
    // if constexpr: 2- and 4-byte fields go through htons/htonl / ntohs/ntohl
    // plus memcpy (which moves the already byte-ordered value without aliasing
    // UB); a 1-byte field has no byte order and is copied straight. The
    // signed<->unsigned casts are deliberate and well-defined in C++20 (two's
    // complement is mandated), so values round-trip exactly. T is deduced from
    // the struct field, so call sites name no sizes; an unsupported width is a
    // compile error rather than a silent truncation.
    namespace Wire
    {
        template <std::integral T>
        inline void put(std::byte*& aCur, T aValue) noexcept
        {
            if constexpr (sizeof(T) == 1)
                *aCur++ = static_cast<std::byte>(static_cast<std::uint8_t>(aValue));
            else if constexpr (sizeof(T) == 2)
            {
                const std::uint16_t lNet = htons(static_cast<std::uint16_t>(aValue));
                std::memcpy(aCur, &lNet, sizeof(lNet));
                aCur += sizeof(lNet);
            }
            else if constexpr (sizeof(T) == 4)
            {
                const std::uint32_t lNet = htonl(static_cast<std::uint32_t>(aValue));
                std::memcpy(aCur, &lNet, sizeof(lNet));
                aCur += sizeof(lNet);
            }
            else
                static_assert(sizeof(T) == 0, "Wire::put: field must be 1/2/4 bytes");
        }

        template <std::integral T>
        inline void get(const std::byte*& aCur, T& aValue) noexcept
        {
            if constexpr (sizeof(T) == 1)
                aValue = static_cast<T>(std::to_integer<std::uint8_t>(*aCur++));
            else if constexpr (sizeof(T) == 2)
            {
                std::uint16_t lNet;
                std::memcpy(&lNet, aCur, sizeof(lNet));
                aCur += sizeof(lNet);
                aValue = static_cast<T>(ntohs(lNet));
            }
            else if constexpr (sizeof(T) == 4)
            {
                std::uint32_t lNet;
                std::memcpy(&lNet, aCur, sizeof(lNet));
                aCur += sizeof(lNet);
                aValue = static_cast<T>(ntohl(lNet));
            }
            else
                static_assert(sizeof(T) == 0, "Wire::get: field must be 1/2/4 bytes");
        }
    }

    // Encode into a caller-owned buffer of at least kWireSize bytes.
    inline void toWire(const ServerState& aMsg, std::byte* aOut) noexcept
    {
        std::byte* lCur = aOut;
        Wire::put(lCur, aMsg.mOrderId);
        Wire::put(lCur, aMsg.mFlags);
        Wire::put(lCur, aMsg.mBallX);
        Wire::put(lCur, aMsg.mBallY);
        Wire::put(lCur, aMsg.mSlideX);
        Wire::put(lCur, aMsg.mSlideY);
    }
    inline void toWire(const ClientSlide& aMsg, std::byte* aOut) noexcept
    {
        std::byte* lCur = aOut;
        Wire::put(lCur, aMsg.mOrderId);
        Wire::put(lCur, aMsg.mSlideX);
        Wire::put(lCur, aMsg.mSlideY);
    }

    // Decode from a buffer of at least kWireSize bytes (the caller checks size).
    template <typename TMsg>
    TMsg fromWire(const std::byte* aIn) noexcept;

    template <>
    inline ServerState fromWire<ServerState>(const std::byte* aIn) noexcept
    {
        const std::byte* lCur = aIn;
        ServerState lMsg;
        Wire::get(lCur, lMsg.mOrderId);
        Wire::get(lCur, lMsg.mFlags);
        Wire::get(lCur, lMsg.mBallX);
        Wire::get(lCur, lMsg.mBallY);
        Wire::get(lCur, lMsg.mSlideX);
        Wire::get(lCur, lMsg.mSlideY);
        return lMsg;
    }
    template <>
    inline ClientSlide fromWire<ClientSlide>(const std::byte* aIn) noexcept
    {
        const std::byte* lCur = aIn;
        ClientSlide lMsg;
        Wire::get(lCur, lMsg.mOrderId);
        Wire::get(lCur, lMsg.mSlideX);
        Wire::get(lCur, lMsg.mSlideY);
        return lMsg;
    }

}


#endif
