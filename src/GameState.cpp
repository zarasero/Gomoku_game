#include "GameState.hpp"

namespace gomoku
{
    void resetGame(GameState& game, GameMode mode, bool keepMode)
    {
        GameMode finalMode = keepMode ? game.mode : mode;
        game = GameState{};
        game.mode = finalMode;
        game.statusMessage = "New game started.";
    }
}