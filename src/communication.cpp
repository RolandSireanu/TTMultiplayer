#include <communication.hpp>

    
 const std::array<std::byte, 1200>& Server::ReadFrame()
 {        
    boost::asio::ip::udp::endpoint lSender;
    try
    {
        std::size_t lNrOfBytes = mSocket.receive_from(boost::asio::buffer(mBuffer), lSender);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
    }
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
    std::string msg = "hello";
    mSocket.send_to(boost::asio::buffer(msg), mServer);
 }