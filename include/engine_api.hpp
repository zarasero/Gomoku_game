#pragma once

#include "Move.hpp"
#include "Board.hpp"
#include "GameState.hpp"
#include "Rules.hpp"
#include "AI.hpp"

namespace gomoku
{
    class EngineAPI
    {
    public:
        EngineAPI() = default;

        const GameState& state() const
        {
            return state_;
        }

        GameState& state()
        {
            return state_;
        }

        void restart(GameMode mode = GameMode::Hotseat)
        {
            resetGame(state_, mode, false);
        }

        MoveResult play(Position pos)
        {
            return placeStone(state_, pos);
        }

        SuggestionResult suggest(CellState player) const
        {
            return computeSimpleSuggestion(state_, player);
        }

    private:
        GameState state_;
    };
}