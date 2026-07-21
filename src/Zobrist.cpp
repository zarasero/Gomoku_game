#include "Zobrist.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>

namespace gomoku
{
    namespace
    {
        struct ZobristKeys
        {
            // [row][col][0=black, 1=white]
            std::array<std::array<std::array<std::uint64_t, 2>, BOARD_SIZE>, BOARD_SIZE> piece{};
            std::uint64_t sideToMove = 0;
            std::array<std::uint64_t, 11> capBlack{};
            std::array<std::uint64_t, 11> capWhite{};
        };

        ZobristKeys& zobrist()
        {
            static ZobristKeys keys;
            static bool initialized = false;

            if (!initialized)
            {
                std::mt19937_64 rng(0x9E3779B97F4A7C15ULL);
                auto rnd = [&]() -> std::uint64_t { return rng(); };

                for (int r = 0; r < BOARD_SIZE; ++r)
                {
                    for (int c = 0; c < BOARD_SIZE; ++c)
                    {
                        keys.piece[r][c][0] = rnd();
                        keys.piece[r][c][1] = rnd();
                    }
                }

                keys.sideToMove = rnd();

                for (int i = 0; i <= 10; ++i)
                {
                    keys.capBlack[i] = rnd();
                    keys.capWhite[i] = rnd();
                }


                initialized = true;
            }

            return keys;
        }
    } // namespace

    std::uint64_t computeZobristHash(const GameState& game)
    {

        const ZobristKeys& z = zobrist();
        std::uint64_t h = 0;

        for (int r = 0; r < BOARD_SIZE; ++r)
        {
            for (int c = 0; c < BOARD_SIZE; ++c)
            {
                const CellState cell = game.board.at(r, c);
                if (cell == CellState::Black)
                    h ^= z.piece[r][c][0];
                else if (cell == CellState::White)
                    h ^= z.piece[r][c][1];
            }
        }

        if (game.currentPlayer == CellState::Black)
            h ^= z.sideToMove;

        const int cb = std::clamp(game.capturesBlack, 0, 10);
        const int cw = std::clamp(game.capturesWhite, 0, 10);
        h ^= z.capBlack[static_cast<std::size_t>(cb)];
        h ^= z.capWhite[static_cast<std::size_t>(cw)];

        return h;
    }
} // namespace gomoku
