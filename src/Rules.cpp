#include "Rules.hpp"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <array>
#include <vector>

namespace gomoku
{
    bool hasAnyWinningFive(const Board& board, CellState player);

    namespace
    {
        constexpr int LINE_DIRS[4][2] = {
            {0, 1},
            {1, 0},
            {1, 1},
            {1, -1}
        };
    }

    // Counts how many stones would be captured by placing at pos.
    int countCapturesIfPlaced(const Board& board, Position pos, CellState player)
    {
        if (!board.inBounds(pos.row, pos.col) || !board.isEmpty(pos.row, pos.col))
            return 0;

        const CellState enemy = opponentOf(player);

        int removed = 0;

        for (int i = 0; i < 4; ++i)
        {
            const int dr = LINE_DIRS[i][0];
            const int dc = LINE_DIRS[i][1];

            int r1 = pos.row + dr;
            int c1 = pos.col + dc;
            int r2 = pos.row + 2 * dr;
            int c2 = pos.col + 2 * dc;
            int r3 = pos.row + 3 * dr;
            int c3 = pos.col + 3 * dc;

            if (board.inBounds(r3, c3)
                && board.at(r1, c1) == enemy
                && board.at(r2, c2) == enemy
                && board.at(r3, c3) == player)
                removed += 2;

            r1 = pos.row - dr;
            c1 = pos.col - dc;
            r2 = pos.row - 2 * dr;
            c2 = pos.col - 2 * dc;
            r3 = pos.row - 3 * dr;
            c3 = pos.col - 3 * dc;

            if (board.inBounds(r3, c3)
                && board.at(r1, c1) == enemy
                && board.at(r2, c2) == enemy
                && board.at(r3, c3) == player)
                removed += 2;
        }

        return removed;
    }

    namespace
    {
        // Maps internal stone color to UI player display name.
        std::string playerDisplayName(CellState player)
        {
            return (player == CellState::Black) ? "Blue" : "Red";
        }

        // Returns true when there are no empty intersections left.
        bool isBoardFull(const Board& board)
        {
            for (int r = 0; r < BOARD_SIZE; ++r)
            {
                for (int c = 0; c < BOARD_SIZE; ++c)
                {
                    if (board.at(r, c) == CellState::Empty)
                        return false;
                }
            }
            return true;
        }
        // Checks whether lastMove creates a valid free-three pattern on one axis.
        // Free-three = after one next move on the same axis we can form .xxxx.
        // Border '#' is treated as a hard blocker.
        bool axisHasFreeThreeThrough(
            const Board& boardAfterMove,
            Position lastMove,
            int dr,
            int dc,
            CellState player)
        {
            constexpr int CENTER = 7;
            char line[15];

            // Build 15-cell slice centered on lastMove.
            for (int t = -7; t <= 7; ++t)
            {
                const int idx = CENTER + t;
                const int r = lastMove.row + t * dr;
                const int c = lastMove.col + t * dc;

                if (!boardAfterMove.inBounds(r, c))
                {
                    line[idx] = '#'; // wall
                }
                else
                {
                    const CellState cell = boardAfterMove.at(r, c);

                    if (cell == CellState::Empty)
                        line[idx] = '.';
                    else if (cell == player)
                        line[idx] = 'x';
                    else
                        line[idx] = 'o';
                }
            }

            if (line[CENTER] != 'x')
                return false;

            // Try every possible next move on this axis only.
            for (int fill = 0; fill < 15; ++fill)
            {
                if (line[fill] != '.')
                    continue;

                char after[15];
                std::memcpy(after, line, sizeof(line));
                after[fill] = 'x';

                // Search open four .xxxx.
                for (int s = 1; s <= 10; ++s)
                {
                    // Cheap filters first.
                    if (CENTER < s || CENTER > s + 3)
                        continue;
                    if (fill < s || fill > s + 3)
                        continue;

                    // Open ends required.
                    if (after[s - 1] != '.')
                        continue;
                    if (after[s + 4] != '.')
                        continue;

                    // Four stones required.
                    bool four = true;
                    for (int k = 0; k < 4; ++k)
                    {
                        if (after[s + k] != 'x')
                        {
                            four = false;
                            break;
                        }
                    }
                    if (!four)
                        continue;

                    // Before fill there must be exactly 3 stones on this 4-segment.
                    int beforeStones = 0;
                    for (int k = 0; k < 4; ++k)
                    {
                        if (line[s + k] == 'x')
                            ++beforeStones;
                    }
                    if (beforeStones != 3)
                        continue;
                    return true;
                }
            }

            return false;
        }



        // Counts distinct axes where the move forms a free-three.
        int countFreeThreeDirections(const Board& boardAfterMove, Position lastMove, CellState player)
        {
            int count = 0;
            for (int d = 0; d < 4; ++d)
            {
                if (axisHasFreeThreeThrough(
                        boardAfterMove, lastMove, LINE_DIRS[d][0], LINE_DIRS[d][1], player))
                    ++count;
            }
            return count;
        }

        struct WinSegment
        {
            int sr;
            int sc;
            int dr;
            int dc;
        };

        // Collects canonical 5+ maximal runs for one player (one entry per contiguous run and direction).
        std::vector<WinSegment> collectWinningSegments(const Board& board, CellState pl)
        {
            std::vector<WinSegment> segments;
            for (int r = 0; r < BOARD_SIZE; ++r)
            {
                for (int c = 0; c < BOARD_SIZE; ++c)
                {
                    if (board.at(r, c) != pl)
                        continue;

                    for (int d = 0; d < 4; ++d)
                    {
                        const int dr = LINE_DIRS[d][0];
                        const int dc = LINE_DIRS[d][1];
                        const int pr = r - dr;
                        const int pc = c - dc;
                        if (board.inBounds(pr, pc) && board.at(pr, pc) == pl)
                            continue;

                        int len = 1;
                        int rr = r + dr;
                        int cc = c + dc;
                        while (board.inBounds(rr, cc) && board.at(rr, cc) == pl)
                        {
                            ++len;
                            rr += dr;
                            cc += dc;
                        }
                        if (len >= 5)
                            segments.push_back(WinSegment{r, c, dr, dc});
                    }
                }
            }
            std::sort(
                segments.begin(),
                segments.end(),
                [](const WinSegment& a, const WinSegment& b) {
                    if (a.sr != b.sr)
                        return a.sr < b.sr;
                    if (a.sc != b.sc)
                        return a.sc < b.sc;
                    if (a.dr != b.dr)
                        return a.dr < b.dr;
                    return a.dc < b.dc;
                });
            segments.erase(
                std::unique(
                    segments.begin(),
                    segments.end(),
                    [](const WinSegment& a, const WinSegment& b) {
                        return a.sr == b.sr && a.sc == b.sc && a.dr == b.dr && a.dc == b.dc;
                    }),
                segments.end());
            return segments;
        }

        // True if some stone of `pl` on the cells of the maximal run (as on `before`) still
        // participates in a 5+ along (seg.dr, seg.dc) on `after` (matches global win detection).
        static bool winningRunAlongSegStillExists(
            const Board& after,
            const Board& before,
            const WinSegment& seg,
            CellState pl)
        {
            int r = seg.sr;
            int c = seg.sc;
            while (before.inBounds(r, c) && before.at(r, c) == pl)
            {
                if (after.inBounds(r, c) && after.at(r, c) == pl)
                {
                    if (checkDirection(after, Position{r, c}, seg.dr, seg.dc))
                        return true;
                }
                r += seg.dr;
                c += seg.dc;
            }
            return false;
        }

        // Empty intersections within Chebyshev `radius` of any cell in a winning run for `pl`.
        static std::array<std::array<bool, BOARD_SIZE>, BOARD_SIZE> captureSearchMaskNearThreats(
            const Board& b,
            CellState pl,
            int radius)
        {
            std::array<std::array<bool, BOARD_SIZE>, BOARD_SIZE> mask{};
            for (int r = 0; r < BOARD_SIZE; ++r)
                mask[r].fill(false);

            const std::vector<WinSegment> threatRuns = collectWinningSegments(b, pl);
            for (const WinSegment& seg : threatRuns)
            {
                int r = seg.sr;
                int c = seg.sc;
                while (b.inBounds(r, c) && b.at(r, c) == pl)
                {
                    for (int dr = -radius; dr <= radius; ++dr)
                    {
                        for (int dc = -radius; dc <= radius; ++dc)
                        {
                            const int rr = r + dr;
                            const int cc = c + dc;
                            if (b.inBounds(rr, cc))
                                mask[rr][cc] = true;
                        }
                    }
                    r += seg.dr;
                    c += seg.dc;
                }
            }
            return mask;
        }

        // For each maximal 5+ run: exists one capturing reply that removes that run's threat.
        // Only empty cells marked in `candidateMask` are tried (near the winning runs).
        static bool opponentCanBreakEveryWinningRunWithSingleCapture(
            const Board& b,
            CellState playerWhoAlignedFive,
            const std::vector<WinSegment>& runs,
            const std::array<std::array<bool, BOARD_SIZE>, BOARD_SIZE>& candidateMask)
        {
            const CellState opponent = opponentOf(playerWhoAlignedFive);

            for (const WinSegment& seg : runs)
            {
                bool brokenBySomeCapture = false;

                for (int r = 0; r < BOARD_SIZE; ++r)
                {
                    for (int c = 0; c < BOARD_SIZE; ++c)
                    {
                        if (!candidateMask[r][c])
                            continue;

                        if (!b.isEmpty(r, c))
                            continue;

                        Board tmp = b;
                        const Position move{r, c};
                        tmp.set(r, c, opponent);
                        const int removed = applyCaptures(tmp, move, opponent);
                        if (removed <= 0)
                            continue;

                        if (!winningRunAlongSegStillExists(tmp, b, seg, playerWhoAlignedFive))
                        {
                            brokenBySomeCapture = true;
                            break;
                        }
                    }
                    if (brokenBySomeCapture)
                        break;
                }

                if (!brokenBySomeCapture)
                    return false;
            }

            return true;
        }

        // Returns true if opponent has a flank-capture reply that breaks every maximal 5+ run
        // (search limited to empty cells near those runs).
        bool opponentCanBreakByCapture(
            const GameState& gameAfterPlayerMove,
            CellState playerWhoAlignedFive)
        {
            const Board& b = gameAfterPlayerMove.board;

            const std::vector<WinSegment> runs = collectWinningSegments(b, playerWhoAlignedFive);
            if (runs.empty())
                return false;

            constexpr int kThreatNeighborhoodCheb = 4;
            const std::array<std::array<bool, BOARD_SIZE>, BOARD_SIZE> mask =
                captureSearchMaskNearThreats(b, playerWhoAlignedFive, kThreatNeighborhoodCheb);
            return opponentCanBreakEveryWinningRunWithSingleCapture(
                b, playerWhoAlignedFive, runs, mask);
        }

        /// After a move, five-in-a-row win is not just `isWinningMove(last)` — any 5+ for the mover
        /// must be unbreakable by flank capture on every maximal winning run (each checked separately).
        enum class FiveInRowResolution
        {
            NotApplicable,
            DeferredByCapture,
            MoverWinsImmediately
        };

        // Resolves five-in-a-row outcome after move under capture-break rules.
        FiveInRowResolution resolveFiveInRowAfterMove(
            const GameState& gameAfterMove,
            CellState mover,
            [[maybe_unused]] Position lastMove)
        {
            // Any 5+ for mover counts (e.g. securing an existing line without placing on it).
            if (!hasAnyWinningFive(gameAfterMove.board, mover))
                return FiveInRowResolution::NotApplicable;

            if (!opponentCanBreakByCapture(gameAfterMove, mover))
                return FiveInRowResolution::MoverWinsImmediately;

            return FiveInRowResolution::DeferredByCapture;
        }
    }

    // Counts contiguous stones in both directions and checks for length >= 5.
    bool checkDirection(const Board& board, Position pos, int dr, int dc)
    {
        CellState me = board.at(pos.row, pos.col);
        if (me == CellState::Empty)
            return false;

        int count = 1;

        int r = pos.row + dr;
        int c = pos.col + dc;
        while (board.inBounds(r, c) && board.at(r, c) == me)
        {
            ++count;
            r += dr;
            c += dc;
        }

        r = pos.row - dr;
        c = pos.col - dc;
        while (board.inBounds(r, c) && board.at(r, c) == me)
        {
            ++count;
            r -= dr;
            c -= dc;
        }

        return count >= 5;
    }

    // Checks whether the move at pos creates a five-in-a-row on any axis.
    bool isWinningMove(const Board& board, Position pos)
    {
        for (int d = 0; d < 4; ++d)
        {
            if (checkDirection(board, pos, LINE_DIRS[d][0], LINE_DIRS[d][1]))
                return true;
        }
        return false;
    }

    // Applies forbidden-move rules (currently double-three with capture exception).
    bool isForbiddenMoveByAdvancedRules(
        const GameState& game,
        Position pos,
        std::string& reason)
    {
        reason.clear();

        if (!game.board.inBounds(pos.row, pos.col) || !game.board.isEmpty(pos.row, pos.col))
        {
            reason = "Illegal move.";
            return true;
        }

        if (countCapturesIfPlaced(game.board, pos, game.currentPlayer) > 0)
            return false;

        Board temp = game.board;
        temp.set(pos.row, pos.col, game.currentPlayer);
        const int freeThreeDirs = countFreeThreeDirections(temp, pos, game.currentPlayer);
        if (freeThreeDirs >= 2)
        {
            reason = "Forbidden move (double-three).";
            return true;
        }

        return false;
    }

    // Applies capture removals after a move and returns removed stone count.
    int applyCaptures(
        Board& board,
        Position pos,
        CellState player,
        std::vector<Position>* capturedOut)
    {
        const CellState enemy = opponentOf(player);

        int removed = 0;

        auto capturePair = [&](int r1, int c1, int r2, int c2)
        {
            if (capturedOut != nullptr)
            {
                capturedOut->push_back(Position{r1, c1});
                capturedOut->push_back(Position{r2, c2});
            }
            board.set(r1, c1, CellState::Empty);
            board.set(r2, c2, CellState::Empty);
            removed += 2;
        };

        for (int i = 0; i < 4; ++i)
        {
            const int dr = LINE_DIRS[i][0];
            const int dc = LINE_DIRS[i][1];

            int r1 = pos.row + dr;
            int c1 = pos.col + dc;
            int r2 = pos.row + 2 * dr;
            int c2 = pos.col + 2 * dc;
            int r3 = pos.row + 3 * dr;
            int c3 = pos.col + 3 * dc;

            if (board.inBounds(r3, c3)
                && board.at(r1, c1) == enemy
                && board.at(r2, c2) == enemy
                && board.at(r3, c3) == player)
                capturePair(r1, c1, r2, c2);

            r1 = pos.row - dr;
            c1 = pos.col - dc;
            r2 = pos.row - 2 * dr;
            c2 = pos.col - 2 * dc;
            r3 = pos.row - 3 * dr;
            c3 = pos.col - 3 * dc;

            if (board.inBounds(r3, c3)
                && board.at(r1, c1) == enemy
                && board.at(r2, c2) == enemy
                && board.at(r3, c3) == player)
                capturePair(r1, c1, r2, c2);
        }

        return removed;
    }

    // Scans the whole board for any existing five-in-a-row for the player.
    bool hasAnyWinningFive(const Board& board, CellState player)
{
    for (int r = 0; r < BOARD_SIZE; r++)
    for (int c = 0; c < BOARD_SIZE; c++)
    {
        if (board.at(r,c) != player)
            continue;

        Position p{r,c};

        for (int d=0; d<4; d++)
        {
            if (checkDirection(board, p,
                LINE_DIRS[d][0],
                LINE_DIRS[d][1]))
                return true;
        }
    }
    return false;
}
    
    // Main move entry point: validates, applies captures, resolves endgame, and switches turn.
    MoveResult placeStone(GameState& game, Position pos)
    {
        MoveResult result;

        if (game.gameOver)
        {
            result.message = "Game is already over. Restart to play again.";
            return result;
        }

        if (!game.board.inBounds(pos.row, pos.col))
        {
            result.message = "Out of bounds.";
            return result;
        }

        if (!game.board.isEmpty(pos.row, pos.col))
        {
            result.message = "This intersection is already occupied.";
            return result;
        }

        std::string forbiddenReason;
        if (isForbiddenMoveByAdvancedRules(game, pos, forbiddenReason))
        {
            result.message = forbiddenReason.empty() ? "Forbidden move." : forbiddenReason;
            return result;
        }

        const CellState player = game.currentPlayer;
        const CellState opp = opponentOf(player);

        game.board.set(pos.row, pos.col, player);
        game.previousMove = game.lastMove;
        game.lastMove = pos;
        game.suggestionMove.reset();

        result.capturedStones = applyCaptures(game.board, pos, player);

        if (player == CellState::Black)
            game.capturesBlack += result.capturedStones;
        else
            game.capturesWhite += result.capturedStones;

        // Structural: mover has at least one 5+ line on the board (includes pre-existing lines, not only through `pos`).
        result.hasFiveInARow = hasAnyWinningFive(game.board, player);
        result.wonByFive = false;
        result.wonByCapture =
            (player == CellState::Black)
                ? (game.capturesBlack >= 10)
                : (game.capturesWhite >= 10);

        result.ok = true;

        if (result.wonByCapture)
        {
            game.gameOver = true;
            game.winner = player;
            game.endgameMessage = playerDisplayName(player) + " wins by capture.";
            result.message = game.endgameMessage;
            return result;
        }

        const FiveInRowResolution fiveRes =
            resolveFiveInRowAfterMove(game, player, pos);

        if (fiveRes == FiveInRowResolution::MoverWinsImmediately)
        {
            game.gameOver = true;
            game.winner = player;
            game.endgameMessage = playerDisplayName(player) + " wins with five in a row.";
            result.wonByFive = true;
            result.message = game.endgameMessage;
            return result;
        }

        // Opponent had 5+ before this ply and it still stands: win only if mover cannot break
        // every such run with one flank capture (same predicate as `resolveFiveInRowAfterMove`).
        const FiveInRowResolution oppRes = resolveFiveInRowAfterMove(game, opp, pos);

        if (!game.gameOver && oppRes == FiveInRowResolution::MoverWinsImmediately)
        {
            game.gameOver = true;
            game.winner = opp;
            game.endgameMessage = playerDisplayName(opp) + " wins with five in a row.";
            result.wonByFive = false;
            result.message = game.endgameMessage;
            return result;
        }

        if (!game.gameOver && isBoardFull(game.board))
        {
            game.gameOver = true;
            game.winner.reset();
            game.endgameMessage = "Draw: board is full.";
            result.message = game.endgameMessage;
            return result;
        }

        game.currentPlayer = opp;

        std::ostringstream oss;
        oss << playerDisplayName(player) << " played " << toCellString(pos) << "\n";      
        if (result.capturedStones > 0)
            oss << " and captured " << result.capturedStones << " stone(s).";
        if (result.hasFiveInARow && !game.gameOver)
            {
                oss << "Five in a row formed;\n"
                    << "opponent may still break\n"
                    << "by capture - play continues.";
            }
        result.message = oss.str();
        return result;
    }    
}
