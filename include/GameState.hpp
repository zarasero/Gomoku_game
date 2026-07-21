#pragma once

#include <limits>
#include <optional>
#include <string>
#include "Board.hpp"
#include "Move.hpp"

namespace gomoku
{
    enum class GameMode
    {
        Hotseat,
        VsAI
    };

    struct DebugInfo
    {
        std::string evaluatedMove = "-";
        int score = 0;
        int depthReached = 0;
        int nodesExplored = 0;
        long long thinkingTimeMs = 0;
    };

    struct MoveResult
    {
        bool ok = false;
        std::string message;
        int capturedStones = 0;
        bool hasFiveInARow = false;
        bool wonByFive = false;
        bool wonByCapture = false;
    };

    struct SuggestionResult
    {
        std::optional<Position> move;
        int score = std::numeric_limits<int>::min();
        int depth = 1;
        int nodesExplored = 0;
        std::string perspective = "Blue";
        std::string scoreSource = "search";
    };

    struct GameState
    {
        Board board;
        CellState currentPlayer = CellState::Black;
        GameMode mode = GameMode::Hotseat;

        bool gameOver = false;
        std::optional<CellState> winner;

        int capturesBlack = 0;
        int capturesWhite = 0;

        std::string statusMessage = "Welcome to Gomoku.";
        std::string endgameMessage;

        std::optional<Position> previousMove;
        std::optional<Position> lastMove;
        std::optional<Position> suggestionMove;
        DebugInfo debug;
    };

    void resetGame(GameState& game, GameMode mode, bool keepMode);
}