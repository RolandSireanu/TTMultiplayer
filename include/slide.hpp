#ifndef SLIDE_HPP
#define SLIDE_HPP
#include <cstdint>


class Slide
{
public:   

    enum class Direction : std::uint8_t
    {
        LEFT, 
        RIGHT
    };

    Slide(std::int32_t aXWindowSize, std::int32_t aYWindowSize, std::int32_t aV, bool aVillain=false) :
        mSlideHeight{aYWindowSize/20}, 
        mSlideWidth{aXWindowSize / 5}, 
        mX{aXWindowSize/2}, 
        mY{aVillain ? 0 : aYWindowSize - mSlideHeight}, 
        mVelocity{aV},
        mWindowWidth {aXWindowSize},
        mWindowHeight {aYWindowSize}
        {}
    
    void Move(Direction aDirection) noexcept;
    void Draw() const;
    void Resize(std::int32_t aXWindowSize, std::int32_t aYWindowSize) noexcept;
    void SetPosition(std::int32_t aX, std::int32_t aY) noexcept;
    [[nodiscard]] constexpr std::int32_t GetX() const noexcept { return mX; }
    [[nodiscard]] constexpr std::int32_t GetY() const noexcept { return mY; }

private:
    std::int32_t mSlideHeight;
    std::int32_t mSlideWidth;
    std::int32_t mX;
    std::int32_t mY;
    std::int32_t mVelocity;
    std::int32_t mWindowWidth;
    std::int32_t mWindowHeight;
};


#endif  //SLIDE_HPP