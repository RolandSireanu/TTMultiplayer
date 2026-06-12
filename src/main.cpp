#include "raylib.h"
#include "slide.hpp"
#include "net.hpp"
#include "messages.hpp"

#include <cstdint>
#include <string_view>
#include <type_traits>

// The server binds SERVER_PORT; the client sends there and learns nothing else
// up front. On two machines both can use the defaults; for a single-machine
// test, run the server first, then the client with SERVER_IP = "127.0.0.1".
static constexpr std::uint16_t    SERVER_PORT {5000};
static constexpr std::string_view SERVER_IP   {"localhost"};

// Diagonal-only ball direction, one sign per axis: movement is a multiply and
// bouncing is a reassignment, with no invalid (zero / non-diagonal) state.
enum class HorizontalDir : std::int8_t { LEFT = -1, RIGHT = 1 };
enum class VerticalDir   : std::int8_t { UP   = -1, DOWN  = 1 };

struct StartingConfig
{
    std::int32_t  mBallX;
    std::int32_t  mBallY;
    HorizontalDir mBallHorizontalDir;
    VerticalDir   mBallVerticalDir;
};

// The whole game mechanism. The server is authoritative: it alone runs the
// ball physics and collision logic and broadcasts the full game state
// (ServerState) every frame; the client moves its own slide locally, sends it
// (ClientSlide), and renders ball/villain/flags exactly as received. Templated
// on the net type because Server and Client share no base class; the
// role-specific paths are if constexpr branches, so each binary compiles only
// its own protocol direction.
template <typename TNet>
class Game
{
    static constexpr bool kIsServer = std::is_same_v<TNet, Server>;

public:
    Game(TNet& aNet, const StartingConfig& aConfig)
        : mNet{aNet},
          mBallX{aConfig.mBallX},
          mBallY{aConfig.mBallY},
          mBallHDir{aConfig.mBallHorizontalDir},
          mBallVDir{aConfig.mBallVerticalDir}
    {}

    void run()
    {
        InitWindow(kScreenWidth, kScreenHeight, "Multiplayer game");
        SetTargetFPS(60);

        waitForOpponent();

        while (!WindowShouldClose())
        {
            readInput();
            applyRemote();
            if constexpr (kIsServer)  // only the server simulates the ball
            {
                moveBall();
                bounce();
            }
            sendLocal();
            draw();
        }

        CloseWindow();
    }

private:
    // Synchronized start: the client announces its slide position until the
    // server answers with its own; the server waits for that first frame and
    // replies immediately. Each side enters the game loop on receipt, so both
    // start within one network round trip. The client re-sends every 0.5 s so
    // a lost datagram (or a server started later) cannot stall the handshake;
    // a lost server reply is covered by the game loop itself, which sends the
    // slide position every frame.
    void waitForOpponent()
    {
        double lLastSendTime{-1.0};
        while (!WindowShouldClose())
        {
            if constexpr (kIsServer)
            {
                if (Messages::ClientSlide lIn; mNet.poll(lIn))
                {
                    mVillainSlide.SetPosition(lIn.mSlideX, mVillainSlide.GetY());
                    sendLocal();  // our state is the client's start signal
                    return;
                }
            }
            else
            {
                if (GetTime() - lLastSendTime > 0.5)
                {
                    sendLocal();
                    lLastSendTime = GetTime();
                }
                if (Messages::ServerState lIn; mNet.poll(lIn))
                {
                    applyServerState(lIn);
                    return;
                }
            }

            BeginDrawing();
                ClearBackground(RAYWHITE);
                DrawText("Waiting for opponent...", 250, 210, 24, GRAY);
            EndDrawing();
        }
    }

    void moveBall() noexcept
    {
        mBallX += kBallVelocity * static_cast<std::int32_t>(mBallHDir);
        mBallY += kBallVelocity * static_cast<std::int32_t>(mBallVDir);
    }

    void readInput()
    {
        if (IsKeyDown(KEY_RIGHT))
            mLocalSlide.Move(Slide::Direction::RIGHT);
        if (IsKeyDown(KEY_LEFT))
            mLocalSlide.Move(Slide::Direction::LEFT);
    }

    void applyRemote()
    {
        // Only the slide's X is taken from the wire: the remote sends its own
        // bottom-paddle Y, but on this screen that paddle is the top (villain).
        if constexpr (kIsServer)
        {
            if (Messages::ClientSlide lIn; mNet.poll(lIn))
                mVillainSlide.SetPosition(lIn.mSlideX, mVillainSlide.GetY());
        }
        else
        {
            if (Messages::ServerState lIn; mNet.poll(lIn))
                applyServerState(lIn);
        }
    }

    // Client only: adopt the authoritative state wholesale — villain slide,
    // ball, and the persistent game-state flags.
    void applyServerState(const Messages::ServerState& aState)
    {
        mVillainSlide.SetPosition(aState.mSlideX, mVillainSlide.GetY());
        mBallX = aState.mBallX;
        mBallY = aState.mBallY;
        mFlags = aState.mFlags;
    }

    void bounce() noexcept
    {
        // Side walls.
        if (mBallX - kBallRadius <= 0)
            mBallHDir = HorizontalDir::RIGHT;
        if (mBallX + kBallRadius >= kScreenWidth)
            mBallHDir = HorizontalDir::LEFT;

        // Paddles: coarse AABB check against the ball centre's X, only when
        // the ball is heading toward the paddle so it cannot get stuck inside.
        if (mBallVDir == VerticalDir::DOWN && overlapsPaddleX(mLocalSlide)
            && mBallY + kBallRadius >= mLocalSlide.GetY())
            mBallVDir = VerticalDir::UP;
        if (mBallVDir == VerticalDir::UP && overlapsPaddleX(mVillainSlide)
            && mBallY - kBallRadius <= mVillainSlide.GetY() + kSlideHeight)
            mBallVDir = VerticalDir::DOWN;

        // Top/bottom walls — no scoring yet, the ball just bounces back.
        if (mBallY - kBallRadius <= 0)
            mBallVDir = VerticalDir::DOWN;
        if (mBallY + kBallRadius >= kScreenHeight)
            mBallVDir = VerticalDir::UP;
    }

    [[nodiscard]] bool overlapsPaddleX(const Slide& aSlide) const noexcept
    {
        return mBallX >= aSlide.GetX() && mBallX <= aSlide.GetX() + kSlideWidth;
    }

    void sendLocal()
    {
        if constexpr (kIsServer)
            mNet.send(Messages::ServerState{
                mNextOrderId++, mFlags,
                static_cast<std::int16_t>(mBallX),
                static_cast<std::int16_t>(mBallY),
                static_cast<std::int16_t>(mLocalSlide.GetX()),
                static_cast<std::int16_t>(mLocalSlide.GetY())});
        else
            mNet.send(Messages::ClientSlide{
                mNextOrderId++,
                static_cast<std::int16_t>(mLocalSlide.GetX()),
                static_cast<std::int16_t>(mLocalSlide.GetY())});
    }

    void draw()
    {
        BeginDrawing();
            ClearBackground(RAYWHITE);
            mLocalSlide.Draw();
            mVillainSlide.Draw();
            DrawCircle(mBallX, mBallY, kBallRadius, BLUE);
            drawOutcome();
        EndDrawing();
    }

    // Flags are defined from the client's perspective, so each binary maps
    // them onto "you" the opposite way. No scoring sets them yet; this is the
    // display half of the mechanism.
    void drawOutcome() const
    {
        const bool lLocalWon  = mFlags & (kIsServer ? Messages::Flags::kClientLost
                                                    : Messages::Flags::kClientWon);
        const bool lLocalLost = mFlags & (kIsServer ? Messages::Flags::kClientWon
                                                    : Messages::Flags::kClientLost);
        if (lLocalWon)
            DrawText("You won!", 320, 100, 40, DARKGREEN);
        else if (lLocalLost)
            DrawText("You lost!", 320, 100, 40, RED);
    }

    static constexpr std::int32_t kScreenWidth   {800};
    static constexpr std::int32_t kScreenHeight  {450};
    static constexpr std::int32_t kBallVelocity  {10};
    static constexpr std::int32_t kSlideVelocity {10};
    static constexpr std::int32_t kBallRadius    {30};
    // Mirrors Slide's internal geometry (it exposes no size getters yet).
    static constexpr std::int32_t kSlideWidth    {kScreenWidth / 5};
    static constexpr std::int32_t kSlideHeight   {kScreenHeight / 20};

    TNet&         mNet;
    Slide         mLocalSlide   {kScreenWidth, kScreenHeight, kSlideVelocity};
    Slide         mVillainSlide {kScreenWidth, kScreenHeight, kSlideVelocity, /*villain=*/true};
    std::int32_t  mBallX;
    std::int32_t  mBallY;
    HorizontalDir mBallHDir;      // server only: the client never simulates
    VerticalDir   mBallVDir;      // server only
    std::int8_t   mFlags{Messages::Flags::kNone};  // server owns; client mirrors
    std::int32_t  mNextOrderId{0};
};

int main()
{
    // Both net classes own an io_context, socket, and receiver thread, so they
    // are non-movable: construct here and hand a reference to Game.
#ifdef SERVER_SIDE
    Server lNet{SERVER_PORT};            // binds, then learns the client from its first frame
#else
    Client lNet{SERVER_IP, SERVER_PORT}; // sends to the server; receives its replies
#endif

    Game lGame{lNet, StartingConfig{
        .mBallX             = 400,
        .mBallY             = 225,
        .mBallHorizontalDir = HorizontalDir::RIGHT,
        .mBallVerticalDir   = VerticalDir::DOWN,
    }};
    lGame.run();

    return 0;
}
