#pragma once

#include <SFML/Graphics.hpp>

#include <string>
#include <vector>

#include "Controller.hpp"
#include "engine_api.hpp"

namespace gomoku
{
    class Renderer
    {
    public:
        Renderer();

        bool loadFont();
        void draw(sf::RenderWindow& window, const EngineAPI& engine, const UIState& uiState) const;

    private:
        sf::Font font_;
        bool fontLoaded_;

        void drawText(
            sf::RenderWindow& window,
            const std::string& str,
            unsigned size,
            float x,
            float y,
            sf::Color color = sf::Color::Black,
            bool bold = false) const;

        void drawBoardBackground(sf::RenderWindow& window) const;
        void drawGrid(sf::RenderWindow& window) const;
        void drawBoardIndices(sf::RenderWindow& window) const;
        void drawOvalStone(sf::RenderWindow& window, float x, float y, CellState state) const;
        void drawMoveMarker(
            sf::RenderWindow& window,
            Position pos,
            sf::Color color,
            float thickness,
            float scale) const;

        void drawStones(sf::RenderWindow& window, const GameState& game) const;
        void drawPanelBackground(sf::RenderWindow& window) const;
        void drawButton(sf::RenderWindow& window, const UIButton& button, bool active = false) const;
        void drawPanel(sf::RenderWindow& window, const EngineAPI& engine, const UIState& uiState) const;
    };
}