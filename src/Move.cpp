#include "Move.hpp"

#include <sstream>

namespace gomoku
{
    std::string toCellString(Position pos)
    {
        char file = static_cast<char>('A' + pos.col);
        std::ostringstream oss;
        oss << file << (pos.row + 1);
        return oss.str();
    }
}