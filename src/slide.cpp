#include "slide.hpp"
#include "raylib.h"
#include <algorithm>
#include <iostream>

void Slide::Move(Direction aDirection) noexcept
{
    switch(aDirection)
    {
        case Direction::LEFT:   mX -= mVelocity; break;
        case Direction::RIGHT:  mX += mVelocity; break;
    }
    
    std::cout << "Old mX = " << mX << std::endl;
    mX = std::clamp(mX, 0, mWindowWidth - mSlideWidth);
    std::cout << "After clamp mX = " << mX << std::endl;
}

void Slide::Draw() const
{
    DrawRectangle(mX,mY,mSlideWidth,mSlideHeight,DARKBLUE);
}

void Slide::SetPosition(std::int32_t aX, std::int32_t aY) noexcept
{
    mX = std::clamp(aX, 0, mWindowWidth - mSlideWidth);
    mY = aY;
}

void Slide::Resize(std::int32_t aXWindowSize, std::int32_t aYWindowSize) noexcept
{
    mSlideHeight = aYWindowSize/20; 
    mSlideWidth = aXWindowSize / 5;
    mX = aXWindowSize/2;
    mY = aYWindowSize - mSlideHeight;    
    mWindowWidth = aXWindowSize;
    mWindowHeight = aYWindowSize;
}