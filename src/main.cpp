#include "raylib.h"
#include "slide.hpp"
#include "net.hpp"
#include "messages.hpp"
#include <iostream>

// The server binds SERVER_PORT; the client sends there and learns nothing else
// up front. On two machines both can use the defaults; for a single-machine
// test, run the server first, then the client with SERVER_IP = "127.0.0.1".
static constexpr std::uint16_t   SERVER_PORT {5000};
static constexpr std::string_view SERVER_IP  {"localhost"};

int main()
{
    const int screenWidth  = 800;
    const int screenHeight = 450;

    Slide lLocalSlide  {screenWidth, screenHeight, 10};
    Slide lRemoteSlide {screenWidth, screenHeight, 10, /*villain=*/true};

    // Both classes expose the same send()/poll() pair and clean up their
    // background receiver thread in the destructor.
#ifdef SERVER_SIDE
    Server lNet{SERVER_PORT};            // binds, then learns the client from its first frame
    Messages::SlidePosition lTemp;
    while(1)
    {
        if(lNet.poll(lTemp))
            std::cout << lTemp.mOrderId << " , " << lTemp.mX << " , " << lTemp.mY << std::endl;
    }
#else
    Client lNet{SERVER_IP, SERVER_PORT}; // sends to the server; receives its replies
    Messages::SlidePosition lTempSlide {1,100,200};
    lNet.send(lTempSlide);
#endif

    std::int32_t lOutgoingOrderId{0};

    InitWindow(screenWidth, screenHeight, "Multiplayer game");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        // --- Input (local player) ---
        if (IsKeyDown(KEY_RIGHT))
            lLocalSlide.Move(Slide::Direction::RIGHT);
        if (IsKeyDown(KEY_LEFT))
            lLocalSlide.Move(Slide::Direction::LEFT);

        // --- Send local position; apply newest remote position if one arrived ---
        Messages::SlidePosition lOut{lOutgoingOrderId++, lLocalSlide.GetX(), lLocalSlide.GetY()};
        lNet.send(lOut);

        Messages::SlidePosition lIn;
        if (lNet.poll(lIn))
            lRemoteSlide.SetPosition(lIn.mX, lIn.mY);

        // --- Draw ---
        BeginDrawing();
            ClearBackground(RAYWHITE);
            lLocalSlide.Draw();
            lRemoteSlide.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
