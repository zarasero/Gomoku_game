#pragma once

#include <cstdint>

#include "GameState.hpp"

namespace gomoku
{
    // Full recompute Zobrist hash for a position.
    std::uint64_t computeZobristHash(const GameState& game);
}
