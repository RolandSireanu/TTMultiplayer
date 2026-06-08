/*******************************************************************************************
*
*   raylib [core] example - basic window
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Welcome to raylib!
*
*   To test examples, just press F6 and execute 'raylib_compile_execute' script
*   Note that compiled executable is placed in the same folder as .c file
*
*   To test the examples on Web, press F6 and execute 'raylib_compile_execute_web' script
*   Web version of the program is generated in the same folder as .c file
*
*   You can find all basic examples on C:\raylib\raylib\examples folder or
*   raylib official webpage: www.raylib.com
*
*   Enjoy using raylib. :)
*
*   Example originally created with raylib 1.0, last time updated with raylib 1.0
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include <cstdint>
#include "slide.hpp"
#include <iostream>
#include "communication.hpp"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;    
    Slide lSlide {screenWidth, screenHeight, 10};
    Slide lVillainSlide {screenWidth, screenHeight, 10, true};
    #ifdef SERVER_SIDE
        Server lServer{};
        
        const auto& [lArray, lNrOfBytes] = lServer.ReadFrame();
        std::cout << std::hex;
        for(size_t i{0}; i<lNrOfBytes; ++i)
            std::cout << "Byte " << i << " : " << std::to_integer<int>(lArray[i]) << "\n";        
    #else
        Client lClient{};
        std::array<std::byte, 1200> lEmpty{std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
        while(1)
            lClient.SendFrame(lEmpty);
    #endif
    

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {

        if (IsKeyDown(KEY_RIGHT))
        {
            lSlide.Move(Slide::Direction::RIGHT);
            lVillainSlide.Move(Slide::Direction::RIGHT);
        }
        
        if(IsKeyDown(KEY_LEFT))
        {            
            lSlide.Move(Slide::Direction::LEFT);
            lVillainSlide.Move(Slide::Direction::LEFT);
        }
        
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
            lSlide.Draw();
            lVillainSlide.Draw();
        

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
