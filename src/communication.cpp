#include <communication.hpp>

    
 std::pair<const std::array<std::byte, 1200>&, std::size_t> Server::ReadFrame()
 {
    boost::asio::ip::udp::endpoint lSender;
    std::size_t lNBytes{0};
    try
    {
        lNBytes = mSocket.receive_from(boost::asio::buffer(mBuffer), lSender);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
    return {mBuffer, lNBytes};
 }

 void Server::SendFrame(const std::array<std::byte, 1200>& aBuffer)
 {

 }

 Client::Client()
 {
    mSocket.open(boost::asio::ip::udp::v4());
 }

 void Client::SendFrame(const std::array<std::byte, 1200>& aBuffer)
 {    
    mSocket.send_to(boost::asio::buffer(aBuffer), mServer);
 }