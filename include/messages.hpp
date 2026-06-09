#ifndef MESSAGES_HPP
#define  MESSAGES_HPP
#include <cstdint>
namespace Messages
{
    
    struct SlidePosition
    {
        std::int32_t mOrderId;
        std::int32_t mX;
        std::int32_t mY;
    } __attribute__((__packed__));

}


#endif