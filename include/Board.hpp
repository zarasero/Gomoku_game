#pragma once

#include <array>
#include "Move.hpp"

namespace gomoku
{
    constexpr int BOARD_SIZE = 19;

    enum class CellState
    {
        Empty,
        Black,
        White
    };

    class Board
    {
    public:
        Board();

        void clear();

        bool inBounds(int row, int col) const;
        bool isEmpty(int row, int col) const;

        CellState at(int row, int col) const;
        void set(int row, int col, CellState value);

        bool hasAnyStone() const;

    private:
        std::array<std::array<CellState, BOARD_SIZE>, BOARD_SIZE> cells_;
    };

    CellState opponentOf(CellState player);
}