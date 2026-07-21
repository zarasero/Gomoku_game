#include "AI.hpp"
#include "Rules.hpp"
#include "Zobrist.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gomoku
{
    namespace
    {
        constexpr int MAX_SEARCH_DEPTH = 20;
        constexpr int SEARCH_TIME_BUDGET_MS = 465;
        constexpr int WIN_SCORE = 10000000;
        constexpr int INF_SCORE = 100000000;
        constexpr int ASPIRATION_WINDOW = 5000;
        constexpr int KILLER_BONUS = 50000;
        constexpr int LINE_DIRS[4][2] = {
            {0, 1},
            {1, 0},
            {1, 1},
            {1, -1}
        };

        enum class TTBound : std::uint8_t
        {
            Exact = 0,
            Lower = 1,
            Upper = 2
        };

        struct TTEntry
        {
            int depth = -1;
            int score = 0;
            TTBound bound = TTBound::Exact;
        };

        // Returns the shared transposition table used by all searches.
        std::unordered_map<std::uint64_t, TTEntry>& transpositionTable()
        {
            static std::unordered_map<std::uint64_t, TTEntry> table;
            return table;
        }

        // Builds a hash key from board cells, side to move, and capture counters.
        std::uint64_t hashBoard(const GameState& game)
        {
            const std::uint64_t z = computeZobristHash(game);
            return z;
        }

        struct SearchStats
        {
            int nodesExplored = 0;
            bool timedOut = false;
            std::chrono::steady_clock::time_point deadline;
            std::array<std::array<Position, 4>, MAX_SEARCH_DEPTH + 1> killerMoves{};
        };

        // Checks whether the time budget is exceeded and latches timeout state.
        bool searchTimedOut(SearchStats& stats)
        {
            if (stats.timedOut)
                return true;

            if ((stats.nodesExplored & 63) == 0 && std::chrono::steady_clock::now() >= stats.deadline)
            {
                stats.timedOut = true;
                return true;
            }

            return false;
        }

        // Returns contiguous line length if player places a stone at pos on one axis.
        int lineLengthIfPlaced(const Board& board, Position pos, CellState player, int dr, int dc)
        {
            int count = 1;

            int r = pos.row + dr;
            int c = pos.col + dc;
            while (board.inBounds(r, c) && board.at(r, c) == player)
            {
                ++count;
                r += dr;
                c += dc;
            }

            r = pos.row - dr;
            c = pos.col - dc;
            while (board.inBounds(r, c) && board.at(r, c) == player)
            {
                ++count;
                r -= dr;
                c -= dc;
            }

            return count;
        }

        // Returns contiguous line length through an already occupied position on one axis.
        int lineLengthExisting(const Board& board, Position pos, CellState player, int dr, int dc)
        {
            if (!board.inBounds(pos.row, pos.col) || board.at(pos.row, pos.col) != player)
                return 0;

            int count = 1;

            int r = pos.row + dr;
            int c = pos.col + dc;
            while (board.inBounds(r, c) && board.at(r, c) == player)
            {
                ++count;
                r += dr;
                c += dc;
            }

            r = pos.row - dr;
            c = pos.col - dc;
            while (board.inBounds(r, c) && board.at(r, c) == player)
            {
                ++count;
                r -= dr;
                c -= dc;
            }

            return count;
        }

        // Computes the best line length created by placing at pos across all axes.
        int maxLineLengthIfPlaced(const Board& board, Position pos, CellState player)
        {
            int best = 1;
            for (int i = 0; i < 4; ++i)
            {
                best = std::max(best, lineLengthIfPlaced(board, pos, player, LINE_DIRS[i][0], LINE_DIRS[i][1]));
            }
            return best;
        }

        // Converts a sequence shape (length/open ends) into a heuristic score.
        int sequenceScore(int length, int openEnds)
        {
            if (length >= 5)
                return 200000;
            if (length == 4)
                return (openEnds == 2) ? 100000 : (openEnds == 1 ? 10000 : 0);
            if (length == 3)
                return (openEnds == 2) ? 15000 : (openEnds == 1 ? 1200 : 0);
            if (length == 2)
                return (openEnds == 2) ? 700 : (openEnds == 1 ? 120 : 0);
            if (length == 1)
                return (openEnds == 2) ? 30 : 0;
            return 0;
        }

        // Evaluates all contiguous sequences for one player on the board.
        int evaluateSequences(const Board& board, CellState player)
        {
            int score = 0;

            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    if (board.at(row, col) != player)
                        continue;

                    for (int d = 0; d < 4; ++d)
                    {
                        const int dr = LINE_DIRS[d][0];
                        const int dc = LINE_DIRS[d][1];
                        const int prevRow = row - dr;
                        const int prevCol = col - dc;

                        if (board.inBounds(prevRow, prevCol) && board.at(prevRow, prevCol) == player)
                            continue;

                        int length = 0;
                        int r = row;
                        int c = col;
                        while (board.inBounds(r, c) && board.at(r, c) == player)
                        {
                            ++length;
                            r += dr;
                            c += dc;
                        }

                        int openEnds = 0;
                        if (board.inBounds(prevRow, prevCol) && board.at(prevRow, prevCol) == CellState::Empty)
                            ++openEnds;
                        if (board.inBounds(r, c) && board.at(r, c) == CellState::Empty)
                            ++openEnds;

                        score += sequenceScore(length, openEnds);
                    }
                }
            }

            return score;
        }

        // Counts player pairs that are currently exposed to flank capture patterns.
        int evaluateVulnerablePairs(const Board& board, CellState player)
        {
            CellState enemy = opponentOf(player);
            int bad = 0;
        
            for (int r=0;r<BOARD_SIZE;r++)
            for (int c=0;c<BOARD_SIZE;c++)
            {
                for (int d=0; d<4; d++)
                {
                    int dr = LINE_DIRS[d][0];
                    int dc = LINE_DIRS[d][1];
        
                    int r1=r, c1=c;
                    int r2=r+dr, c2=c+dc;
                    int r3=r+2*dr, c3=c+2*dc;
                    int r4=r+3*dr, c4=c+3*dc;
        
                    if (!board.inBounds(r4,c4))
                        continue;
        
                    if (board.at(r1,c1)==enemy &&
                        board.at(r2,c2)==player &&
                        board.at(r3,c3)==player &&
                        board.at(r4,c4)==CellState::Empty)
                        bad++;
                }
            }
        
            return bad;
        }

        // Estimates how many immediate captures are available in relevant empty cells.
        int evaluatePotentialCaptures(
            const Board& board,
            CellState player,
            int neighborhoodRadius)
        {
            int score = 0;
            bool marked[BOARD_SIZE][BOARD_SIZE] = {};

            auto markAroundStone = [&](int r, int c) {
                for (int dr = -neighborhoodRadius; dr <= neighborhoodRadius; ++dr)
                {
                    for (int dc = -neighborhoodRadius; dc <= neighborhoodRadius; ++dc)
                    {
                        const int rr = r + dr;
                        const int cc = c + dc;

                        if (!board.inBounds(rr, cc) || !board.isEmpty(rr, cc))
                            continue;

                        marked[rr][cc] = true;
                    }
                }
            };

            for (int r = 0; r < BOARD_SIZE; r++)
            {
                for (int c = 0; c < BOARD_SIZE; c++)
                {
                    if (board.at(r, c) == CellState::Empty)
                        continue;
                    markAroundStone(r, c);
                }
            }

            for (int r = 0; r < BOARD_SIZE; r++)
            {
                for (int c = 0; c < BOARD_SIZE; c++)
                {
                    if (!marked[r][c])
                        continue;

                    const int captures = countCapturesIfPlaced(board, Position{r, c}, player);
                    if (captures > 0)
                        score += captures;
                }
            }

            return score;
        }

        // Finds the maximum capture gain in one move (early exits at stopAt).
        int maxCaptureGainAnyMove(const Board& board, CellState player, int stopAt)
        {
            int best = 0;
            for (int r = 0; r < BOARD_SIZE; ++r)
            {
                for (int c = 0; c < BOARD_SIZE; ++c)
                {
                    if (!board.isEmpty(r, c))
                        continue;
                    const int gain = countCapturesIfPlaced(board, Position{r, c}, player);
                    if (gain > best)
                    {
                        best = gain;
                        if (best >= stopAt)
                            return best;
                    }
                }
            }
            return best;
        }

        struct PairOpennessStats
        {
            int doubleOpen = 0;
            int semiOpen = 0;
        };

        /// Enemy OO with two flank empties (hot capture race) vs one empty flank (pressure).
        // Counts open and semi-open adjacent pairs for a given stone color.
        PairOpennessStats countPairOpenness(const Board& board, CellState pairColor)
        {
            PairOpennessStats out;
            for (int r = 0; r < BOARD_SIZE; ++r)
            {
                for (int c = 0; c < BOARD_SIZE; ++c)
                {
                    for (int d = 0; d < 4; ++d)
                    {
                        const int dr = LINE_DIRS[d][0];
                        const int dc = LINE_DIRS[d][1];
                        const int pr = r - dr;
                        const int pc = c - dc;
                        if (board.inBounds(pr, pc) && board.at(pr, pc) == pairColor)
                            continue;

                        if (board.at(r, c) != pairColor)
                            continue;

                        const int r2 = r + dr;
                        const int c2 = c + dc;
                        if (!board.inBounds(r2, c2) || board.at(r2, c2) != pairColor)
                            continue;

                        const int el = r - dr;
                        const int ecl = c - dc;
                        const int er = r + 2 * dr;
                        const int ecr = c + 2 * dc;
                        if (!board.inBounds(el, ecl) || !board.inBounds(er, ecr))
                            continue;

                        const bool lE = board.at(el, ecl) == CellState::Empty;
                        const bool rE = board.at(er, ecr) == CellState::Empty;
                        if (lE && rE)
                            ++out.doubleOpen;
                        else if (lE ^ rE)
                            ++out.semiOpen;
                    }
                }
            }
            return out;
        }

        /// Local "potential capture" pressure: empty cell lies on an axis with adjacent enemy OO.
        // Scores latent local capture pressure around a candidate move.
        int latentCaptureProximityBonus(const Board& board, Position pos, CellState enemy)
        {
            int bonus = 0;
            const int r = pos.row;
            const int c = pos.col;

            for (int d = 0; d < 4; ++d)
            {
                const int dr = LINE_DIRS[d][0];
                const int dc = LINE_DIRS[d][1];

                auto at = [&](int rr, int cc) -> CellState {
                    if (!board.inBounds(rr, cc))
                        return CellState::Empty;
                    return board.at(rr, cc);
                };

                if (at(r + dr, c + dc) == enemy && at(r + 2 * dr, c + 2 * dc) == enemy)
                    bonus += 14000;
                if (at(r - dr, c - dc) == enemy && at(r - 2 * dr, c - 2 * dc) == enemy)
                    bonus += 14000;
                if (at(r - dr, c - dc) == enemy && at(r + dr, c + dc) == enemy)
                    bonus += 9000;
            }

            return bonus;
        }

        // Returns the strongest existing line passing through pos for a player.
        int maxLineLengthAt(const Board& board, Position pos, CellState player)
        {
            int best = 0;

            for (int i = 0; i < 4; ++i)
            {
                best = std::max(
                    best,
                    lineLengthExisting(board, pos, player, LINE_DIRS[i][0], LINE_DIRS[i][1]));
            }

            return best;
        }

        // Full static evaluation from maximizingPlayer perspective (must match alphaBeta root side).
        int evaluateBoardState(const GameState& game, CellState maximizingPlayer, int ply)
        {
            // ===== TERMINAL POSITIONS =====
            if (game.gameOver)
            {
                if (game.winner.has_value())
                {
                    return (*game.winner == maximizingPlayer)
                        ? (WIN_SCORE - ply * 50)
                        : (-WIN_SCORE + ply * 50);
                }
                return 0;
            }

            const CellState opponent = opponentOf(maximizingPlayer);

            const int myCaptures =
                (maximizingPlayer == CellState::Black) ? game.capturesBlack : game.capturesWhite;

            const int oppCaptures =
                (opponent == CellState::Black) ? game.capturesBlack : game.capturesWhite;
        
            int score = 0;
        
            // ===== CAPTURE MATERIAL =====
            score += (myCaptures - oppCaptures) * 3200;
        
            // ===== CAPTURE RACE EMERGENCY =====
            if (myCaptures >= 8 || oppCaptures >= 8)
            {
                const int myGainNow =
                    maxCaptureGainAnyMove(game.board, maximizingPlayer, 2);

                const int oppGainNow =
                    maxCaptureGainAnyMove(game.board, opponent, 2);
        
                if (myCaptures + myGainNow >= 10)
                    score += 4000000;
        
                if (oppCaptures + oppGainNow >= 10)
                    score -= 4500000;
            }
        
            // ===== POTENTIAL CAPTURE THREATS =====
            constexpr int captureNeighborhood = 2;
        
            score +=
                evaluatePotentialCaptures(
                    game.board,
                    maximizingPlayer,
                    captureNeighborhood) * 1750;

            score -=
                evaluatePotentialCaptures(
                    game.board,
                    opponent,
                    captureNeighborhood) * 3400;

            // ===== OPEN / SEMI-OPEN PAIRS =====
            const PairOpennessStats myOO =
                countPairOpenness(game.board, maximizingPlayer);

            const PairOpennessStats oppOO =
                countPairOpenness(game.board, opponent);
        
            // FIXED: signs were inverted
            score += myOO.doubleOpen * 4200;
            score += myOO.semiOpen * 2400;
        
            score -= oppOO.doubleOpen * 3200;
            score -= oppOO.semiOpen * 1900;
        
            // ===== CAPTURE MOMENTUM =====
            const int capLead = myCaptures - oppCaptures;
        
            if (myCaptures >= 6)
                score += (myCaptures - 5) * 4500;
        
            if (oppCaptures >= 6)
                score -= (oppCaptures - 5) * 4500;
        
            if (capLead >= 4)
            {
                score += (capLead - 3) * 2000;
            }
            else if (capLead <= -4)
            {
                // clearer negative formulation
                score -= (-capLead - 3) * 2000;
            }
        
            // ===== VULNERABLE PAIRS =====
            score -=
                evaluateVulnerablePairs(
                    game.board,
                    maximizingPlayer) * 2600;

            score +=
                evaluateVulnerablePairs(
                    game.board,
                    opponent) * 1400;
        
            // ===== LAST MOVE PRESSURE =====
            if (game.lastMove.has_value())
            {
                const Position p = *game.lastMove;
        
                if (game.board.at(p.row, p.col) == opponent)
                {
                    score -=
                        maxLineLengthAt(
                            game.board,
                            p,
                            opponent) * 600;
                }
            }
        
            // ===== PREVIOUS MOVE CONTEXT =====
            if (game.previousMove.has_value())
            {
                const Position p = *game.previousMove;
        
                const CellState stone =
                    game.board.at(p.row, p.col);
        
                if (stone == maximizingPlayer)
                {
                    score +=
                        maxLineLengthAt(
                            game.board,
                            p,
                            maximizingPlayer) * 220;
                }
                else if (stone == opponent)
                {
                    score -=
                        maxLineLengthAt(
                            game.board,
                            p,
                            opponent) * 220;
                }
            }
        
            // ===== MAIN POSITIONAL EVAL =====
            score += evaluateSequences(game.board, maximizingPlayer);
            score -= evaluateSequences(game.board, opponent);

            return score;
        }

        // Base branching limit per ply for candidate move generation.
        int candidateLimitForPly(int ply)
        {
            static const int limits[MAX_SEARCH_DEPTH] = {5, 4, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
            if (ply < 0)
                return limits[0];
            if (ply >= MAX_SEARCH_DEPTH)
                return limits[MAX_SEARCH_DEPTH - 1];
            return limits[ply];
        }

        // Adjusts candidate limit based on tactical capture pressure.
        int effectiveCandidateLimit(int ply, const GameState& game)
        {
            int lim = candidateLimitForPly(ply);
            const int capMax = std::max(game.capturesBlack, game.capturesWhite);
            if (capMax >= 7)
                lim += 2;
            if (lim > 8)
                lim = 8;
            return lim;
        }

        // Fast tactical score used to order candidate moves before deep search.
        int quickMoveScore(const GameState& game, Position pos, CellState player)
        {
            if (!game.board.inBounds(pos.row, pos.col) || !game.board.isEmpty(pos.row, pos.col))
                return std::numeric_limits<int>::min();

            const CellState enemy = opponentOf(player);
            int score = 0;

            if (maxLineLengthIfPlaced(game.board, pos, player) >= 5)
                score += 5000000;
            if (maxLineLengthIfPlaced(game.board, pos, enemy) >= 5)
                score += 4500000;

            const int capGain = countCapturesIfPlaced(game.board, pos, player);
            const int blockGain = countCapturesIfPlaced(game.board, pos, enemy);

            score += capGain * 22000;
            score += blockGain * 26000;

            const int myCap = (player == CellState::Black) ? game.capturesBlack : game.capturesWhite;
            const int oppCap = (enemy == CellState::Black) ? game.capturesBlack : game.capturesWhite;

            if (myCap + capGain >= 10)
                score += 7500000;
            else if (myCap >= 6 && capGain > 0)
            score += capGain * 12000 + (myCap - 5) * 8000;
            if (oppCap >= 6 && capGain > 0)
                score += capGain * 15000 + (oppCap - 4) * 6000;
            if (oppCap >= 8 && blockGain > 0)
                score += 6000000;

            const int capPressure = std::max(myCap, oppCap);
            if (capPressure >= 4)
                score += latentCaptureProximityBonus(game.board, pos, enemy);

            score += maxLineLengthIfPlaced(game.board, pos, player) * 5000;
            score += maxLineLengthIfPlaced(game.board, pos, enemy) * 3500;

            const int center = BOARD_SIZE / 2;
            score += 60 - 3 * (std::abs(pos.row - center) + std::abs(pos.col - center));

            for (int dr = -2; dr <= 2; ++dr)
            {
                for (int dc = -2; dc <= 2; ++dc)
                {
                    if (dr == 0 && dc == 0)
                        continue;

                    const int rr = pos.row + dr;
                    const int cc = pos.col + dc;
                    if (!game.board.inBounds(rr, cc))
                        continue;

                    if (game.board.at(rr, cc) == player)
                        score += 120;
                    else if (game.board.at(rr, cc) == enemy)
                        score += 90;
                }
            }

            return score;
        }

        // Counts non-empty cells on the board.
        int countStones(const Board& board)
        {
            int total = 0;
            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    if (board.at(row, col) != CellState::Empty)
                        ++total;
                }
            }
            return total;
        }

        // Returns true if a killer-move slot is not initialized.
        bool isUnsetKiller(Position p)
        {
            return p.row < 0;
        }

        // True if `player` may play at `pos` under advanced rules (empty + not forbidden).
        bool isLegalMoveForPlayer(const GameState& game, Position pos, CellState player)
        {
            if (!game.board.inBounds(pos.row, pos.col) || !game.board.isEmpty(pos.row, pos.col))
                return false;

            GameState probe = game;
            probe.currentPlayer = player;
            std::string reason;
            return !isForbiddenMoveByAdvancedRules(probe, pos, reason);
        }

        // Best empty legal intersection by quickMoveScore (full-board scan).
        std::optional<Position> findBestLegalMoveOnBoard(const GameState& game, CellState player)
        {
            std::optional<Position> best;
            int bestScore = std::numeric_limits<int>::min();

            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    const Position pos{row, col};
                    if (!isLegalMoveForPlayer(game, pos, player))
                        continue;

                    const int score = quickMoveScore(game, pos, player);
                    if (!best.has_value() || score > bestScore)
                    {
                        best = pos;
                        bestScore = score;
                    }
                }
            }

            return best;
        }
        // Detects whether a square is tactically relevant and should be searched.
        bool isTacticalHotSpot(const Board& board, Position pos, CellState player)
        {
            const CellState enemy = opponentOf(player);
            if (countCapturesIfPlaced(board, pos, player) > 0)
                return true;
            if (countCapturesIfPlaced(board, pos, enemy) > 0)
                return true;
            if (maxLineLengthIfPlaced(board, pos, player) >= 4)
                return true;
            if (maxLineLengthIfPlaced(board, pos, enemy) >= 4)
                return true;
            return false;
        }

        // Generates and orders candidate moves with tactical filtering and limits.
        std::vector<Position> generateCandidateMoves(
            const GameState& game,
            CellState player,
            int ply,
            const std::array<Position, 4>* killersAtPly)
        {
            std::vector<Position> moves;
            if (game.gameOver)
                return moves;

            if (!game.board.hasAnyStone())
            {
                moves.push_back(Position{BOARD_SIZE / 2, BOARD_SIZE / 2});
                return moves;
            }

            std::array<std::array<bool, BOARD_SIZE>, BOARD_SIZE> marked{};

            const int stoneCount = countStones(game.board);
            const int primaryRadius = stoneCount > 10 ? 1 : 2;

            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    if (game.board.at(row, col) == CellState::Empty)
                        continue;

                    for (int dr = -primaryRadius; dr <= primaryRadius; ++dr)
                    {
                        for (int dc = -primaryRadius; dc <= primaryRadius; ++dc)
                        {
                            const int rr = row + dr;
                            const int cc = col + dc;
                            if (!game.board.inBounds(rr, cc) || !game.board.isEmpty(rr, cc))
                                continue;
                            marked[rr][cc] = true;
                        }
                    }
                }
            }

            if (stoneCount > 10)
            {
                for (int row = 0; row < BOARD_SIZE; ++row)
                {
                    for (int col = 0; col < BOARD_SIZE; ++col)
                    {
                        if (game.board.at(row, col) == CellState::Empty)
                            continue;

                        for (int dr = -2; dr <= 2; ++dr)
                        {
                            for (int dc = -2; dc <= 2; ++dc)
                            {
                                if (std::max(std::abs(dr), std::abs(dc)) <= 1)
                                    continue;

                                const int rr = row + dr;
                                const int cc = col + dc;
                                if (!game.board.inBounds(rr, cc) || !game.board.isEmpty(rr, cc))
                                    continue;
                                const Position p{rr, cc};
                                if (isTacticalHotSpot(game.board, p, player))
                                    marked[rr][cc] = true;
                            }
                        }
                    }
                }
            }

            // Capture race: keep all immediate capture moves only in early/top search layers
            // or when either side is already close to capture win.
            const int capMax = std::max(game.capturesBlack, game.capturesWhite);
            if (ply == 0 || capMax >= 7)
            {
                for (int row = 0; row < BOARD_SIZE; ++row)
                {
                    for (int col = 0; col < BOARD_SIZE; ++col)
                    {
                        if (!game.board.isEmpty(row, col))
                            continue;
                        const Position p{row, col};
                        if (countCapturesIfPlaced(game.board, p, player) > 0)
                            marked[row][col] = true;
                    }
                }
            }

            // Near capture finish, always include direct blocking squares where opponent can capture now.
            if (capMax >= 8)
            {
                const CellState enemy = opponentOf(player);
                for (int row = 0; row < BOARD_SIZE; ++row)
                {
                    for (int col = 0; col < BOARD_SIZE; ++col)
                    {
                        if (!game.board.isEmpty(row, col))
                            continue;
                        const Position p{row, col};
                        if (countCapturesIfPlaced(game.board, p, enemy) > 0)
                            marked[row][col] = true;
                    }
                }
            }

            std::vector<std::pair<int, Position>> scoredMoves;
            scoredMoves.reserve(BOARD_SIZE * BOARD_SIZE);

            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    if (!marked[row][col])
                        continue;

                    Position pos{row, col};
                    int score = quickMoveScore(game, pos, player);
                    if (score == std::numeric_limits<int>::min())
                        continue;

                    const int capMaxScoring = std::max(game.capturesBlack, game.capturesWhite);
                    if (capMaxScoring >= 4 || ply <= 1)
                    {
                        const int cg = countCapturesIfPlaced(game.board, pos, player);
                        if (cg > 0)
                            score += 250000 + cg * 6000;
                    }

                    if (killersAtPly != nullptr)
                    {
                        for (int k = 0; k < 4; ++k)
                        {
                            const Position& kpos = (*killersAtPly)[k];
                            if (!isUnsetKiller(kpos) && kpos == pos)
                            {
                                score += KILLER_BONUS;
                                break;
                            }
                        }
                    }

                    scoredMoves.push_back(std::make_pair(score, pos));
                }
            }

            std::sort(
                scoredMoves.begin(),
                scoredMoves.end(),
                [](const std::pair<int, Position>& lhs, const std::pair<int, Position>& rhs)
                {
                    return lhs.first > rhs.first;
                });

            const int limit = effectiveCandidateLimit(ply, game);
            for (std::size_t i = 0; i < scoredMoves.size() && static_cast<int>(moves.size()) < limit; ++i)
                moves.push_back(scoredMoves[i].second);

            if (moves.empty())
            {
                for (int row = 0; row < BOARD_SIZE; ++row)
                {
                    for (int col = 0; col < BOARD_SIZE; ++col)
                    {
                        if (game.board.isEmpty(row, col))
                            moves.push_back(Position{row, col});
                    }
                }
            }

            return moves;
        }

        int alphaBeta(
            const GameState& game,
            int depthRemaining,
            int alpha,
            int beta,
            CellState maximizingPlayer,
            SearchStats& stats,
            int ply);

        // If set, caller must return this score immediately (exact TT hit or cutoff after bound merge).
        std::optional<int> transpositionProbe(
            std::uint64_t key,
            int depthRemaining,
            int& alpha,
            int& beta)
        {
            auto& tt = transpositionTable();
            const auto it = tt.find(key);
            if (it == tt.end() || it->second.depth < depthRemaining)
                return std::nullopt;

            const TTEntry& e = it->second;
            if (e.bound == TTBound::Exact)
                return e.score;
            if (e.bound == TTBound::Lower)
                alpha = std::max(alpha, e.score);
            else if (e.bound == TTBound::Upper)
                beta = std::min(beta, e.score);
            if (alpha >= beta)
                return e.score;
            return std::nullopt;
        }

        int reducedChildDepth(int depthRemaining, std::size_t moveIndex, bool tacticalCapture)
        {
            int childDepth = depthRemaining - 1;
            const int mi = static_cast<int>(moveIndex);
            if (!tacticalCapture && mi >= 2 && depthRemaining >= 4)
                --childDepth;
            if (!tacticalCapture && mi >= 5 && depthRemaining >= 6)
                --childDepth;
            if (childDepth < 0)
                childDepth = 0;
            return childDepth;
        }

        int alphaBetaPvsChild(
            const GameState& next,
            int childDepth,
            int alpha,
            int beta,
            bool firstLegalMove,
            bool maximizing,
            CellState maximizingPlayer,
            SearchStats& stats,
            int childPly)
        {
            if (firstLegalMove)
            {
                return alphaBeta(
                    next,
                    childDepth,
                    alpha,
                    beta,
                    maximizingPlayer,
                    stats,
                    childPly);
            }
            if (maximizing)
            {
                int value = alphaBeta(
                    next,
                    childDepth,
                    alpha,
                    alpha + 1,
                    maximizingPlayer,
                    stats,
                    childPly);
                if (value > alpha && value < beta && childDepth > 0)
                {
                    value = alphaBeta(
                        next,
                        childDepth,
                        alpha,
                        beta,
                        maximizingPlayer,
                        stats,
                        childPly);
                }
                return value;
            }
            int value = alphaBeta(
                next,
                childDepth,
                beta - 1,
                beta,
                maximizingPlayer,
                stats,
                childPly);
            if (value > alpha && value < beta && childDepth > 0)
            {
                value = alphaBeta(
                    next,
                    childDepth,
                    alpha,
                    beta,
                    maximizingPlayer,
                    stats,
                    childPly);
            }
            return value;
        }

        void recordKillerCutMove(SearchStats& stats, int ply, const Position& cutMove)
        {
            auto& slot = stats.killerMoves[static_cast<std::size_t>(ply)];
            slot[3] = slot[2];
            slot[2] = slot[1];
            slot[1] = slot[0];
            slot[0] = cutMove;
        }

        void transpositionStoreBest(
            std::uint64_t key,
            int depthRemaining,
            int bestValue,
            int origAlpha,
            int origBeta)
        {
            TTBound bound = TTBound::Exact;
            if (bestValue <= origAlpha)
                bound = TTBound::Upper;
            else if (bestValue >= origBeta)
                bound = TTBound::Lower;

            auto& ttStore = transpositionTable();
            const auto itStore = ttStore.find(key);
            if (itStore == ttStore.end() || itStore->second.depth <= depthRemaining)
                ttStore[key] = TTEntry{depthRemaining, bestValue, bound};
        }

        // Principal alpha-beta search with TT, killer moves, and reductions.
        int alphaBeta(
            const GameState& game,
            int depthRemaining,
            int alpha,
            int beta,
            CellState maximizingPlayer,
            SearchStats& stats,
            int ply)
        {
            if (depthRemaining == 0 || game.gameOver)
                return evaluateBoardState(game, maximizingPlayer, ply);

            if (searchTimedOut(stats))
                return evaluateBoardState(game, maximizingPlayer, ply);

            const int origAlpha = alpha;
            const int origBeta = beta;
            const std::uint64_t key =
                hashBoard(game) ^ (static_cast<std::uint64_t>(static_cast<int>(maximizingPlayer)) << 56);

            if (const std::optional<int> ttHit = transpositionProbe(key, depthRemaining, alpha, beta))
                return *ttHit;

            const CellState toPlay = game.currentPlayer;
            const bool maximizing = (toPlay == maximizingPlayer);
            const std::vector<Position> moves =
                generateCandidateMoves(game, toPlay, ply, &stats.killerMoves[static_cast<std::size_t>(ply)]);

            if (moves.empty())
                return evaluateBoardState(game, maximizingPlayer, ply);

            bool hasLegalMove = false;
            int bestValue = maximizing ? -INF_SCORE : INF_SCORE;
            bool firstLegalMove = true;

            for (std::size_t i = 0; i < moves.size(); ++i)
            {
                if (searchTimedOut(stats))
                    return evaluateBoardState(game, maximizingPlayer, ply);

                GameState next = game;
                next.currentPlayer = toPlay;
                const MoveResult move = placeStone(next, moves[i]);
                if (!move.ok)
                    continue;

                hasLegalMove = true;
                ++stats.nodesExplored;

                const int capGainHere = countCapturesIfPlaced(game.board, moves[i], toPlay);
                const int capMaxBoard = std::max(game.capturesBlack, game.capturesWhite);
                const bool tacticalCapture =
                    capGainHere > 0 || capMaxBoard >= 8 || (capMaxBoard >= 6 && depthRemaining <= 3);
                const int childDepth = reducedChildDepth(depthRemaining, i, tacticalCapture);

                const int value = alphaBetaPvsChild(
                    next,
                    childDepth,
                    alpha,
                    beta,
                    firstLegalMove,
                    maximizing,
                    maximizingPlayer,
                    stats,
                    ply + 1);

                firstLegalMove = false;

                if (stats.timedOut)
                    return evaluateBoardState(game, maximizingPlayer, ply);

                if (maximizing)
                {
                    bestValue = std::max(bestValue, value);
                    alpha = std::max(alpha, bestValue);
                }
                else
                {
                    bestValue = std::min(bestValue, value);
                    beta = std::min(beta, bestValue);
                }

                if (beta <= alpha)
                {
                    recordKillerCutMove(stats, ply, moves[i]);
                    break;
                }
            }

            if (!hasLegalMove)
                return evaluateBoardState(game, maximizingPlayer, ply);

            transpositionStoreBest(key, depthRemaining, bestValue, origAlpha, origBeta);
            return bestValue;
        }

        // Returns predefined opening choices to avoid expensive early search.
        std::optional<Position> fastOpeningMove(const GameState& game)
        {
            const int stones = countStones(game.board);
            const int center = BOARD_SIZE / 2;

            if (stones == 0 && game.board.isEmpty(center, center))
                return Position{center, center};

            if (stones == 1)
            {
                static const Position choices[] = {
                    {center, center},
                    {center, center + 1},
                    {center + 1, center},
                    {center, center - 1},
                    {center - 1, center},
                    {center + 1, center + 1},
                    {center - 1, center - 1},
                    {center + 1, center - 1},
                    {center - 1, center + 1}
                };

                for (const Position& pos : choices)
                {
                    if (game.board.inBounds(pos.row, pos.col) && game.board.isEmpty(pos.row, pos.col))
                        return pos;
                }
            }

            return std::nullopt;
        }

        void ensureTranspositionTableReserved()
        {
            static bool ttReserved = false;
            if (!ttReserved)
            {
                transpositionTable().reserve(500000);
                ttReserved = true;
            }
        }

        std::optional<SuggestionResult> tryOpeningHeuristicResult(
            const GameState& root,
            CellState player,
            const char* perspectiveLabel)
        {
            if (std::optional<Position> opening = fastOpeningMove(root))
            {
                SuggestionResult r;
                r.depth = 1;
                r.perspective = perspectiveLabel;
                r.scoreSource = "opening-heuristic";
                r.move = *opening;
                r.score = quickMoveScore(root, *opening, player);
                r.nodesExplored = 1;
                return r;
            }
            return std::nullopt;
        }

        void resetKillerSlots(SearchStats& stats)
        {
            for (std::size_t pi = 0; pi < stats.killerMoves.size(); ++pi)
            {
                stats.killerMoves[pi][0] = Position{-1, -1};
                stats.killerMoves[pi][1] = Position{-1, -1};
                stats.killerMoves[pi][2] = Position{-1, -1};
                stats.killerMoves[pi][3] = Position{-1, -1};
            }
        }

        std::vector<std::pair<Position, int>> buildRootMovesWithQuickScores(
            const GameState& root,
            CellState player,
            const std::vector<Position>& generatedRootMoves)
        {
            std::vector<std::pair<Position, int>> rootMoves;
            rootMoves.reserve(generatedRootMoves.size());
            for (const Position& pos : generatedRootMoves)
            {
                if (!isLegalMoveForPlayer(root, pos, player))
                    continue;
                rootMoves.push_back(std::make_pair(pos, quickMoveScore(root, pos, player)));
            }
            return rootMoves;
        }

        SuggestionResult runIterativeDeepeningSearch(
            const GameState& root,
            CellState player,
            std::vector<std::pair<Position, int>> rootMoves,
            SearchStats& stats,
            const char* perspectiveLabel)
        {
            SuggestionResult result;
            result.perspective = perspectiveLabel;
            result.scoreSource = "search";
            result.depth = 0;

            std::optional<Position> bestMove = rootMoves.front().first;
            int bestScore = quickMoveScore(root, *bestMove, player);

            for (int depth = 1; depth <= MAX_SEARCH_DEPTH; ++depth)
            {
                if (searchTimedOut(stats))
                    break;

                const int windowAlpha = (depth > 1) ? (bestScore - ASPIRATION_WINDOW) : -INF_SCORE;
                const int windowBeta = (depth > 1) ? (bestScore + ASPIRATION_WINDOW) : INF_SCORE;

                bool completedDepth = true;
                int depthBestScore = -INF_SCORE;
                std::optional<Position> depthBestMove;

                auto runRootMoves = [&](int wA, int wB) {
                    int alpha = wA;
                    const int beta = wB;

                    std::stable_sort(
                        rootMoves.begin(),
                        rootMoves.end(),
                        [](const std::pair<Position, int>& a, const std::pair<Position, int>& b) {
                            return a.second > b.second;
                        });

                    for (std::size_t i = 0; i < rootMoves.size(); ++i)
                    {
                        if (searchTimedOut(stats))
                        {
                            completedDepth = false;
                            return;
                        }

                        GameState next = root;
                        next.currentPlayer = player;
                        const MoveResult move = placeStone(next, rootMoves[i].first);
                        if (!move.ok)
                            continue;

                        ++stats.nodesExplored;

                        const int score = alphaBeta(
                            next,
                            depth - 1,
                            alpha,
                            beta,
                            player,
                            stats,
                            1);

                        if (stats.timedOut)
                        {
                            completedDepth = false;
                            return;
                        }

                        if (!depthBestMove.has_value() || score > depthBestScore)
                        {
                            depthBestScore = score;
                            depthBestMove = rootMoves[i].first;
                        }

                        rootMoves[i].second = score;
                        alpha = std::max(alpha, depthBestScore);
                    }
                };

                runRootMoves(windowAlpha, windowBeta);

                if (depth > 1 && completedDepth && depthBestMove.has_value()
                    && (depthBestScore <= windowAlpha || depthBestScore >= windowBeta) && !stats.timedOut)
                {
                    completedDepth = true;
                    depthBestScore = -INF_SCORE;
                    depthBestMove.reset();
                    runRootMoves(-INF_SCORE, INF_SCORE);
                }

                if (!completedDepth || !depthBestMove.has_value())
                    break;

                bestMove = depthBestMove;
                bestScore = depthBestScore;
                result.depth = depth;
            }

            if (!bestMove.has_value())
            {
                if (const std::optional<Position> fallback = findBestLegalMoveOnBoard(root, player))
                {
                    result.move = *fallback;
                    result.score = quickMoveScore(root, *fallback, player);
                    result.nodesExplored = stats.nodesExplored;
                    result.depth = (result.depth > 0) ? result.depth : 1;
                    result.scoreSource = "legal-fallback";
                    return result;
                }

                result.score = 0;
                result.nodesExplored = stats.nodesExplored;
                result.scoreSource = "no-legal-moves";
                return result;
            }

            result.move = bestMove;
            result.score = bestScore;
            result.nodesExplored = stats.nodesExplored;
            result.scoreSource = "search";
            if (result.depth == 0)
                result.depth = 1;
            return result;
        }
    }


    // Public entry point: iterative deepening search that returns best move and debug data.
    SuggestionResult computeSimpleSuggestion(const GameState& game, CellState player)
    {
        SuggestionResult result;
        result.depth = 0;
        const char* const perspectiveLabel = (player == CellState::Black) ? "Blue" : "Red";
        result.perspective = perspectiveLabel;
        result.scoreSource = "search";

        if (game.gameOver)
        {
            result.score = 0;
            result.scoreSource = "terminal";
            return result;
        }

        GameState root = game;
        root.currentPlayer = player;

        ensureTranspositionTableReserved();

        if (std::optional<SuggestionResult> opening = tryOpeningHeuristicResult(root, player, perspectiveLabel))
            return *opening;

        SearchStats stats;
        stats.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(SEARCH_TIME_BUDGET_MS);
        resetKillerSlots(stats);

        const std::vector<Position> generatedRootMoves = generateCandidateMoves(root, player, 0, nullptr);
        if (generatedRootMoves.empty())
        {
            result.score = 0;
            result.nodesExplored = 0;
            result.scoreSource = "no-legal-moves";
            return result;
        }

        std::vector<std::pair<Position, int>> rootMoves =
            buildRootMovesWithQuickScores(root, player, generatedRootMoves);

        return runIterativeDeepeningSearch(
            root,
            player,
            std::move(rootMoves),
            stats,
            perspectiveLabel);
    }
}
