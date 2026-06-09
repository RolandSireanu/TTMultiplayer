#include <communication.hpp>
#include <span>
    
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

//  void Client::SendFrame(const std::array<std::byte, 1200>& aBuffer)
void Client::SendFrame(std::span<const std::byte> aBuffer)
{    
    mSocket.send_to(boost::asio::buffer(aBuffer.data(), aBuffer.size_bytes()), mServer);
}