#pragma once

#include <string>

namespace gomoku
{
    struct Position
    {
        int row = 0;
        int col = 0;
    };

    inline bool operator==(const Position& a, const Position& b)
    {
        return a.row == b.row && a.col == b.col;
    }

    std::string toCellString(Position pos);
}