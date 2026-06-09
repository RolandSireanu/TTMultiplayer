#ifndef COMMUNICATION_HPP
#define COMMUNICATION_HPP

#include <boost/asio.hpp>
#include <iostream>
#include <array>
#include <cstdint>
#include <span>

class Server
{
public:
    Server() = default;

    std::pair<const std::array<std::byte, 1200>&, std::size_t> ReadFrame();
    void SendFrame(const std::array<std::byte, 1200>& aBuffer);

private:
    std::array<std::byte, 1200> mBuffer{};
    static constexpr std::int32_t BIND_PORT {5000};
    boost::asio::io_context mIoContext;
    boost::asio::ip::udp::socket mSocket{mIoContext, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), BIND_PORT)};  // bind on port 5000
};


//  boost::asio::io_context io;

//     // Resolve "localhost" port 5000 (or use make_address directly).
//     udp::resolver resolver(io);
//     udp::endpoint server = *resolver.resolve(udp::v4(), "localhost", "5000").begin();

//     udp::socket socket(io);
//     socket.open(udp::v4());                 // no bind — kernel picks an ephemeral port

//     std::string msg = "hello";
//     socket.send_to(boost::asio::buffer(msg), server);

//     std::array<char, 1500> reply;
//     udp::endpoint from;
//     std::size_t n = socket.receive_from(boost::asio::buffer(reply), from);
//     std::cout << "Got: " << std::string(reply.data(), n) << '\n';

class Client
{
public:
    Client();

    const std::array<std::byte, 1200>& ReadFrame();
    void SendFrame(std::span<const std::byte> aBuffer);

private:
    boost::asio::io_context mIoContext;
    boost::asio::ip::udp::resolver mResolver{mIoContext};
    boost::asio::ip::udp::endpoint mServer {*mResolver.resolve(boost::asio::ip::udp::v4(), "192.168.56.102", "5000").begin()};
    boost::asio::ip::udp::socket mSocket{mIoContext};
};


#endif