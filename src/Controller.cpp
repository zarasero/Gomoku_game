#include "Controller.hpp"

#include <chrono>
#include <cstdint>
#include <cmath>
#include <limits>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace gomoku
{
    namespace
    {
        struct AsyncAiSearch
        {
            std::mutex mutex;
            bool running = false;
            bool ready = false;
            std::uint64_t generation = 0;
            std::uint64_t readyGeneration = 0;
            SuggestionResult result;
        };

        AsyncAiSearch& asyncAiSearch()
        {
            static AsyncAiSearch search;
            return search;
        }

        std::optional<Position> firstEmptyCell(const Board& board)
        {
            for (int row = 0; row < BOARD_SIZE; ++row)
            {
                for (int col = 0; col < BOARD_SIZE; ++col)
                {
                    if (board.isEmpty(row, col))
                        return Position{row, col};
                }
            }
            return std::nullopt;
        }

        std::string modeName(GameMode mode)
        {
            return (mode == GameMode::Hotseat) ? "Hotseat" : "Vs AI";
        }

        void cancelPendingAiSearch()
        {
            AsyncAiSearch& search = asyncAiSearch();
            std::lock_guard<std::mutex> lock(search.mutex);
            ++search.generation;
            search.running = false;
            search.ready = false;
            search.readyGeneration = 0;
            search.result = SuggestionResult{};
        }

        void launchAiSearch(const GameState& snapshot, CellState player)
        {
            AsyncAiSearch& search = asyncAiSearch();
            std::uint64_t generation = 0;

            {
                std::lock_guard<std::mutex> lock(search.mutex);
                ++search.generation;
                generation = search.generation;
                search.running = true;
                search.ready = false;
                search.readyGeneration = 0;
                search.result = SuggestionResult{};
            }

            std::thread([snapshot, player, generation]() {
                SuggestionResult suggestion = computeSimpleSuggestion(snapshot, player);

                AsyncAiSearch& shared = asyncAiSearch();
                std::lock_guard<std::mutex> lock(shared.mutex);
                if (shared.generation != generation)
                    return;

                shared.result = suggestion;
                shared.ready = true;
                shared.readyGeneration = generation;
                shared.running = false;
            }).detach();
        }

        bool consumeAiSearchResult(SuggestionResult& result)
        {
            AsyncAiSearch& search = asyncAiSearch();
            std::lock_guard<std::mutex> lock(search.mutex);
            if (!search.ready || search.readyGeneration != search.generation)
                return false;

            result = search.result;
            search.ready = false;
            search.readyGeneration = 0;
            return true;
        }
    }

    std::optional<Position> Controller::mouseToCell(sf::Vector2i mouse) const
    {
        const float fx = static_cast<float>(mouse.x);
        const float fy = static_cast<float>(mouse.y);

        const float gx = (fx - PADDING) / CELL;
        const float gy = (fy - PADDING) / CELL;

        const int col = static_cast<int>(std::round(gx));
        const int row = static_cast<int>(std::round(gy));

        if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE)
            return std::nullopt;

        const float cx = PADDING + col * CELL;
        const float cy = PADDING + row * CELL;

        if (std::abs(fx - cx) > CELL * 0.45f || std::abs(fy - cy) > CELL * 0.45f)
            return std::nullopt;

        return Position{row, col};
    }

    std::vector<std::pair<ButtonId, UIButton>> Controller::buildButtons(const GameState& game) const
    {
        std::vector<std::pair<ButtonId, UIButton>> buttons;

        float y = PANEL_Y + 132.f;

        UIButton restart;
        restart.rect = sf::FloatRect(PANEL_X + 20.f, y, BUTTON_W, BUTTON_H);
        restart.label = "Restart";
        buttons.push_back(std::make_pair(ButtonId::Restart, restart));
        y += 44.f;

        UIButton reset;
        reset.rect = sf::FloatRect(PANEL_X + 20.f, y, BUTTON_W, BUTTON_H);
        reset.label = "Full Reset";
        buttons.push_back(std::make_pair(ButtonId::Reset, reset));
        y += 44.f;

        UIButton toggle;
        toggle.rect = sf::FloatRect(PANEL_X + 20.f, y, BUTTON_W, BUTTON_H);
        toggle.label = (game.mode == GameMode::Hotseat) ? "Switch to Vs AI" : "Switch to Hotseat";
        buttons.push_back(std::make_pair(ButtonId::ToggleMode, toggle));
        y += 44.f;

        UIButton suggest;
        suggest.rect = sf::FloatRect(PANEL_X + 20.f, y, BUTTON_W, BUTTON_H);
        suggest.label = "Suggestion";
        buttons.push_back(std::make_pair(ButtonId::Suggest, suggest));

        return buttons;
    }

    std::optional<ButtonId> Controller::buttonAtPosition(const GameState& game, sf::Vector2f pos) const
    {
        const std::vector<std::pair<ButtonId, UIButton>> buttons = buildButtons(game);

        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            if (buttons[i].second.rect.contains(pos))
                return buttons[i].first;
        }

        return std::nullopt;
    }

    void Controller::triggerSuggestion(EngineAPI& engine)
    {
        auto start = std::chrono::high_resolution_clock::now();
        SuggestionResult suggestion = engine.suggest(engine.state().currentPlayer);
        auto end = std::chrono::high_resolution_clock::now();

        GameState& game = engine.state();
        game.suggestionMove = suggestion.move;
        game.debug.depthReached = suggestion.depth;
        game.debug.nodesExplored = suggestion.nodesExplored;
        game.debug.score = (suggestion.score == std::numeric_limits<int>::min()) ? 0 : suggestion.score;
        game.debug.evaluatedMove = suggestion.move.has_value() ? toCellString(*suggestion.move) : "-";
        game.debug.thinkingTimeMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (suggestion.move.has_value())
            game.statusMessage = "Suggestion ready: " + toCellString(*suggestion.move);
        else
            game.statusMessage = "No legal suggestion found.";
    }

    void Controller::updateAiTurn(EngineAPI& engine, UIState& uiState)
    {
        GameState& game = engine.state();

        if (game.mode != GameMode::VsAI || game.gameOver)
        {
            if (uiState.aiThinking)
            {
                cancelPendingAiSearch();
                uiState.aiThinking = false;
            }
            return;
        }

        if (game.currentPlayer != CellState::White)
        {
            if (uiState.aiThinking)
            {
                cancelPendingAiSearch();
                uiState.aiThinking = false;
            }
            return;
        }

        if (!uiState.aiThinking)
        {
            uiState.aiThinking = true;
            uiState.aiDelayClock.restart();
            game.debug.thinkingTimeMs = 0;
            game.statusMessage = "AI is thinking...";

            GameState snapshot = game;
            snapshot.currentPlayer = CellState::White;
            launchAiSearch(snapshot, CellState::White);
            return;
        }

        game.debug.thinkingTimeMs = uiState.aiDelayClock.getElapsedTime().asMilliseconds();

        SuggestionResult suggestion;
        if (!consumeAiSearchResult(suggestion))
            return;

        std::optional<Position> chosenMove = suggestion.move;
        if (!chosenMove.has_value())
            chosenMove = firstEmptyCell(game.board);

        game.debug.depthReached = suggestion.depth;
        game.debug.nodesExplored = suggestion.nodesExplored;
        game.debug.score = (suggestion.score == std::numeric_limits<int>::min()) ? 0 : suggestion.score;
        game.debug.evaluatedMove = chosenMove.has_value() ? toCellString(*chosenMove) : "-";
        game.debug.thinkingTimeMs = uiState.aiDelayClock.getElapsedTime().asMilliseconds();

        if (chosenMove.has_value())
        {
            const MoveResult move = engine.play(*chosenMove);
            game.statusMessage = move.message;
        }
        else
        {
            game.statusMessage = "AI found no legal move.";
        }

        uiState.aiThinking = false;
    }

    void Controller::handleButton(EngineAPI& engine, UIState& uiState, ButtonId button)
    {
        GameState& game = engine.state();

        switch (button)
        {
            case ButtonId::Restart:
                cancelPendingAiSearch();
                resetGame(game, game.mode, true);
                uiState.aiThinking = false;
                break;

            case ButtonId::Reset:
                cancelPendingAiSearch();
                resetGame(game, GameMode::Hotseat, false);
                uiState.aiThinking = false;
                break;

            case ButtonId::ToggleMode:
                cancelPendingAiSearch();
                resetGame(
                    game,
                    (game.mode == GameMode::Hotseat) ? GameMode::VsAI : GameMode::Hotseat,
                    false);
                uiState.aiThinking = false;
                game.statusMessage = "Mode changed to " + modeName(game.mode) + ".";
                break;

            case ButtonId::Suggest:
                if (game.gameOver)
                {
                    game.statusMessage = "Game is over. Restart for a new suggestion.";
                    return;
                }
                triggerSuggestion(engine);
                break;
        }
    }

    void Controller::tryHumanBoardClick(EngineAPI& engine, UIState& uiState, Position pos)
    {
        GameState& game = engine.state();

        if (game.gameOver)
        {
            game.statusMessage = "Game is over. Restart to play again.";
            return;
        }

        if (game.mode == GameMode::VsAI && game.currentPlayer == CellState::White)
        {
            game.statusMessage = "Wait for the AI move.";
            return;
        }

        const MoveResult move = engine.play(pos);
        game.statusMessage = move.message;

        if (move.ok)
            uiState.aiThinking = false;
    }

    void Controller::handleMouseClick(EngineAPI& engine, UIState& uiState, sf::Vector2f clickPos)
    {
        if (std::optional<ButtonId> button = buttonAtPosition(engine.state(), clickPos))
        {
            handleButton(engine, uiState, *button);
            return;
        }

        std::optional<Position> cell = mouseToCell(
            sf::Vector2i(static_cast<int>(clickPos.x), static_cast<int>(clickPos.y)));

        if (cell.has_value())
            tryHumanBoardClick(engine, uiState, *cell);
    }

    void Controller::update(EngineAPI& engine, UIState& uiState)
    {
        updateAiTurn(engine, uiState);
    }
}
