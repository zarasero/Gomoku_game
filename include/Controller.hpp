#pragma once

#include <SFML/Graphics.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine_api.hpp"

namespace gomoku
{
    inline constexpr float CELL = 36.f;
    inline constexpr float PADDING = 40.f;
    inline constexpr float LINE_THICKNESS = 2.f;

    inline constexpr float STONE_RADIUS = CELL * 0.42f;
    inline constexpr float STONE_SCALE_X = 1.18f;
    inline constexpr float STONE_SCALE_Y = 0.82f;

    inline constexpr float BOARD_PIXELS = (BOARD_SIZE - 1) * CELL;
    inline constexpr float BOARD_AREA_W = 2.f * PADDING + BOARD_PIXELS;
    inline constexpr float BOARD_AREA_H = 2.f * PADDING + BOARD_PIXELS;

    inline constexpr float PANEL_W = 320.f;
    inline constexpr float PANEL_MARGIN = 18.f;

    inline constexpr unsigned WINDOW_W =
        static_cast<unsigned>(BOARD_AREA_W + PANEL_W + PANEL_MARGIN * 2.f);
    inline constexpr unsigned WINDOW_H =
        static_cast<unsigned>(BOARD_AREA_H + 40.f);

    inline constexpr float PANEL_X = BOARD_AREA_W + PANEL_MARGIN;
    inline constexpr float PANEL_Y = 18.f;
    inline constexpr float BUTTON_W = PANEL_W - 40.f;
    inline constexpr float BUTTON_H = 34.f;

    enum class ButtonId
    {
        Restart,
        Reset,
        ToggleMode,
        Suggest
    };

    struct UIButton
    {
        sf::FloatRect rect;
        std::string label;
    };

    struct UIState
    {
        bool aiThinking = false;
        sf::Clock aiDelayClock;
    };

    class Controller
    {
    public:
        Controller() = default;

        void handleMouseClick(EngineAPI& engine, UIState& uiState, sf::Vector2f clickPos);
        void update(EngineAPI& engine, UIState& uiState);

        std::optional<Position> mouseToCell(sf::Vector2i mouse) const;
        std::vector<std::pair<ButtonId, UIButton>> buildButtons(const GameState& game) const;
        std::optional<ButtonId> buttonAtPosition(const GameState& game, sf::Vector2f pos) const;

    private:
        void handleButton(EngineAPI& engine, UIState& uiState, ButtonId button);
        void triggerSuggestion(EngineAPI& engine);
        void updateAiTurn(EngineAPI& engine, UIState& uiState);
        void tryHumanBoardClick(EngineAPI& engine, UIState& uiState, Position pos);
    };
}