#pragma once

#include "GameState.hpp"

namespace gomoku
{
    SuggestionResult computeSimpleSuggestion(const GameState& game, CellState player);
}
