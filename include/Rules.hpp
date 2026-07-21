#pragma once

#include <vector>

#include "GameState.hpp"

namespace gomoku
{
    bool checkDirection(const Board& board, Position pos, int dr, int dc);
    bool isWinningMove(const Board& board, Position pos);

    bool isForbiddenMoveByAdvancedRules(
        const GameState& game,
        Position pos,
        std::string& reason);

    int countCapturesIfPlaced(const Board& board, Position pos, CellState player);

    int applyCaptures(
        Board& board,
        Position pos,
        CellState player,
        std::vector<Position>* capturedOut = nullptr);
    MoveResult placeStone(GameState& game, Position pos);
}

// namespace gomoku
// {
//     bool checkDirection(const Board& board, Position pos, int dr, int dc);
//     bool isWinningMove(const Board& board, Position pos);

//     bool isForbiddenMoveByAdvancedRules(
//         const GameState& game,
//         Position pos,
//         std::string& reason);

//     int applyCaptures(Board& board, Position pos, CellState player);
//     MoveResult placeStone(GameState& game, Position pos);
// }