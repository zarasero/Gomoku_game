#include "Renderer.hpp"

#include <vector>

namespace gomoku
{
    namespace
    {
        std::string playerDisplayName(CellState player)
        {
            return (player == CellState::Black) ? "Blue" : "Red";
        }

        sf::Color playerDisplayColor(CellState player)
        {
            return (player == CellState::Black)
                ? sf::Color(70, 90, 255)
                : sf::Color(255, 70, 70);
        }

        std::string modeName(GameMode mode)
        {
            return (mode == GameMode::Hotseat) ? "Hotseat" : "Vs AI";
        }

        std::string boardColumnLabel(int index)
        {
            return std::string(1, static_cast<char>('A' + index));
        }
    }

    Renderer::Renderer()
        : fontLoaded_(false)
    {
    }

    bool Renderer::loadFont()
    {
        const std::vector<std::string> candidates = {
            "./assets/font.ttf",
            "./assets/DejaVuSans.ttf",
            "./assets/arial.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/freefont/FreeSans.ttf"
        };

        for (std::vector<std::string>::const_iterator it = candidates.begin(); it != candidates.end(); ++it)
        {
            if (font_.loadFromFile(*it))
            {
                fontLoaded_ = true;
                return true;
            }
        }

        fontLoaded_ = false;
        return false;
    }

    void Renderer::drawText(
        sf::RenderWindow& window,
        const std::string& str,
        unsigned size,
        float x,
        float y,
        sf::Color color,
        bool bold) const
    {
        if (!fontLoaded_)
            return;

        sf::Text text;
        text.setFont(font_);
        text.setString(str);
        text.setCharacterSize(size);
        text.setFillColor(color);
        text.setStyle(bold ? sf::Text::Bold : sf::Text::Regular);
        text.setPosition(x, y);
        window.draw(text);
    }

    void Renderer::drawBoardBackground(sf::RenderWindow& window) const
    {
        sf::RectangleShape outer(sf::Vector2f(BOARD_AREA_W, BOARD_AREA_H));
        outer.setFillColor(sf::Color(225, 195, 140));
        outer.setPosition(0.f, 0.f);
        window.draw(outer);

        sf::RectangleShape background(sf::Vector2f(BOARD_PIXELS + CELL, BOARD_PIXELS + CELL));
        background.setFillColor(sf::Color(210, 180, 120));
        background.setPosition(PADDING - CELL / 2.f, PADDING - CELL / 2.f);
        window.draw(background);
    }

    void Renderer::drawGrid(sf::RenderWindow& window) const
    {
        for (int i = 0; i < BOARD_SIZE; ++i)
        {
            const float x = PADDING + i * CELL;
            const float y = PADDING + i * CELL;

            sf::RectangleShape vertical(sf::Vector2f(LINE_THICKNESS, BOARD_PIXELS));
            vertical.setFillColor(sf::Color::Black);
            vertical.setPosition(x - LINE_THICKNESS / 2.f, PADDING);
            window.draw(vertical);

            sf::RectangleShape horizontal(sf::Vector2f(BOARD_PIXELS, LINE_THICKNESS));
            horizontal.setFillColor(sf::Color::Black);
            horizontal.setPosition(PADDING, y - LINE_THICKNESS / 2.f);
            window.draw(horizontal);
        }

        const int stars[] = {3, 9, 15};
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                sf::CircleShape star(4.f);
                star.setOrigin(4.f, 4.f);
                star.setPosition(PADDING + stars[j] * CELL, PADDING + stars[i] * CELL);
                star.setFillColor(sf::Color::Black);
                window.draw(star);
            }
        }

        drawBoardIndices(window);
    }

    void Renderer::drawBoardIndices(sf::RenderWindow& window) const
    {
        if (!fontLoaded_)
            return;

        for (int i = 0; i < BOARD_SIZE; ++i)
        {
            sf::Text horizontalText;
            horizontalText.setFont(font_);
            horizontalText.setString(boardColumnLabel(i));
            horizontalText.setCharacterSize(15);
            horizontalText.setFillColor(sf::Color(45, 35, 20));

            sf::FloatRect horizontalBounds = horizontalText.getLocalBounds();
            horizontalText.setOrigin(
                horizontalBounds.left + horizontalBounds.width / 2.f,
                horizontalBounds.top + horizontalBounds.height / 2.f);

            sf::Text verticalText;
            verticalText.setFont(font_);
            verticalText.setString(std::to_string(i + 1));
            verticalText.setCharacterSize(15);
            verticalText.setFillColor(sf::Color(45, 35, 20));

            sf::FloatRect verticalBounds = verticalText.getLocalBounds();
            verticalText.setOrigin(
                verticalBounds.left + verticalBounds.width / 2.f,
                verticalBounds.top + verticalBounds.height / 2.f);

            const float x = PADDING + i * CELL;
            const float y = PADDING + i * CELL;
            const float topY = PADDING - CELL * 0.68f;
            const float bottomY = PADDING + BOARD_PIXELS + CELL * 0.30f;
            const float leftX = PADDING - CELL * 0.78f;
            const float rightX = PADDING + BOARD_PIXELS + CELL * 0.42f;

            horizontalText.setPosition(x, topY);
            window.draw(horizontalText);

            horizontalText.setPosition(x, bottomY);
            window.draw(horizontalText);

            verticalText.setPosition(leftX, y);
            window.draw(verticalText);

            verticalText.setPosition(rightX, y);
            window.draw(verticalText);
        }
    }

    void Renderer::drawOvalStone(sf::RenderWindow& window, float x, float y, CellState state) const
    {
        sf::Color base;
        sf::Color edge;
        sf::Color gloss;
        sf::Color inner;
        sf::Color shadow(0, 0, 0, 70);

        if (state == CellState::Black)
        {
            base = sf::Color(70, 90, 255);
            edge = sf::Color(40, 50, 170);
            gloss = sf::Color(255, 255, 255, 120);
            inner = sf::Color(120, 135, 255);
        }
        else
        {
            base = sf::Color(255, 70, 70);
            edge = sf::Color(170, 35, 35);
            gloss = sf::Color(255, 255, 255, 140);
            inner = sf::Color(255, 130, 130);
        }

        sf::CircleShape shadowShape(STONE_RADIUS);
        shadowShape.setOrigin(STONE_RADIUS, STONE_RADIUS);
        shadowShape.setScale(STONE_SCALE_X, STONE_SCALE_Y);
        shadowShape.setPosition(x + 2.f, y + 3.f);
        shadowShape.setFillColor(shadow);
        window.draw(shadowShape);

        sf::CircleShape outer(STONE_RADIUS);
        outer.setOrigin(STONE_RADIUS, STONE_RADIUS);
        outer.setScale(STONE_SCALE_X, STONE_SCALE_Y);
        outer.setPosition(x, y);
        outer.setFillColor(base);
        outer.setOutlineThickness(1.5f);
        outer.setOutlineColor(edge);
        window.draw(outer);

        sf::CircleShape innerShape(STONE_RADIUS * 0.82f);
        innerShape.setOrigin(STONE_RADIUS * 0.82f, STONE_RADIUS * 0.82f);
        innerShape.setScale(STONE_SCALE_X, STONE_SCALE_Y * 0.92f);
        innerShape.setPosition(x - 1.f, y - 1.5f);
        innerShape.setFillColor(inner);
        window.draw(innerShape);

        sf::CircleShape highlight(STONE_RADIUS * 0.38f);
        highlight.setOrigin(STONE_RADIUS * 0.38f, STONE_RADIUS * 0.38f);
        highlight.setScale(1.6f, 0.75f);
        highlight.setPosition(x - STONE_RADIUS * 0.30f, y - STONE_RADIUS * 0.33f);
        highlight.setFillColor(gloss);
        window.draw(highlight);

        sf::CircleShape lowLight(STONE_RADIUS * 0.22f);
        lowLight.setOrigin(STONE_RADIUS * 0.22f, STONE_RADIUS * 0.22f);
        lowLight.setScale(1.3f, 0.65f);
        lowLight.setPosition(x + STONE_RADIUS * 0.18f, y + STONE_RADIUS * 0.18f);
        lowLight.setFillColor(sf::Color(255, 255, 255, 55));
        window.draw(lowLight);
    }

    void Renderer::drawMoveMarker(
        sf::RenderWindow& window,
        Position pos,
        sf::Color color,
        float thickness,
        float scale) const
    {
        const float size = CELL * scale;
        const float x = PADDING + pos.col * CELL - size / 2.f;
        const float y = PADDING + pos.row * CELL - size / 2.f;

        sf::RectangleShape box(sf::Vector2f(size, size));
        box.setPosition(x, y);
        box.setFillColor(sf::Color::Transparent);
        box.setOutlineThickness(thickness);
        box.setOutlineColor(color);
        window.draw(box);
    }

    void Renderer::drawStones(sf::RenderWindow& window, const GameState& game) const
    {
        for (int row = 0; row < BOARD_SIZE; ++row)
        {
            for (int col = 0; col < BOARD_SIZE; ++col)
            {
                const CellState cell = game.board.at(row, col);
                if (cell == CellState::Empty)
                    continue;

                drawOvalStone(window, PADDING + col * CELL, PADDING + row * CELL, cell);
            }
        }

        if (game.lastMove.has_value())
            drawMoveMarker(window, *game.lastMove, sf::Color(255, 215, 0), 2.f, 0.42f);

        if (game.suggestionMove.has_value())
            drawMoveMarker(window, *game.suggestionMove, sf::Color(30, 180, 30), 2.f, 0.68f);
    }

    void Renderer::drawPanelBackground(sf::RenderWindow& window) const
    {
        sf::RectangleShape panel(sf::Vector2f(PANEL_W, WINDOW_H - PANEL_Y * 2.f));
        panel.setPosition(PANEL_X, PANEL_Y);
        panel.setFillColor(sf::Color(245, 239, 230));
        panel.setOutlineThickness(1.5f);
        panel.setOutlineColor(sf::Color(170, 160, 150));
        window.draw(panel);
    }

    void Renderer::drawButton(sf::RenderWindow& window, const UIButton& button, bool active) const
    {
        sf::RectangleShape rect(sf::Vector2f(button.rect.width, button.rect.height));
        rect.setPosition(button.rect.left, button.rect.top);
        rect.setFillColor(active ? sf::Color(220, 230, 255) : sf::Color(232, 226, 216));
        rect.setOutlineThickness(1.2f);
        rect.setOutlineColor(sf::Color(120, 115, 105));
        window.draw(rect);

        drawText(window, button.label, 16, button.rect.left + 12.f, button.rect.top + 6.f, sf::Color::Black, true);
    }

    void Renderer::drawPanel(sf::RenderWindow& window, const EngineAPI& engine, const UIState& uiState) const
    {
        const GameState& game = engine.state();

        drawPanelBackground(window);

        drawText(window, "UI Panel", 22, PANEL_X + 18.f, PANEL_Y + 14.f, sf::Color::Black, true);
        drawText(window, "Mode: " + modeName(game.mode), 17, PANEL_X + 18.f, PANEL_Y + 48.f);
        drawText(
            window,
            "Turn: " + playerDisplayName(game.currentPlayer),
            17,
            PANEL_X + 18.f,
            PANEL_Y + 74.f,
            playerDisplayColor(game.currentPlayer),
            true);

        if (uiState.aiThinking)
        {
            drawText(window, "AI is thinking...", 16, PANEL_X + 18.f, PANEL_Y + 100.f, sf::Color(120, 70, 20), true);
        }
        else if (game.gameOver && game.winner.has_value())
        {
            drawText(
                window,
                "Winner: " + playerDisplayName(*game.winner),
                16,
                PANEL_X + 18.f,
                PANEL_Y + 100.f,
                playerDisplayColor(*game.winner),
                true);
        }
        else
        {
            drawText(window, "Ready", 16, PANEL_X + 18.f, PANEL_Y + 100.f, sf::Color(40, 110, 40), true);
        }

        Controller helper;
        const std::vector<std::pair<ButtonId, UIButton>> buttons = helper.buildButtons(game);
        for (std::size_t i = 0; i < buttons.size(); ++i)
        {
            const bool active =
                (buttons[i].first == ButtonId::ToggleMode && game.mode == GameMode::VsAI);
            drawButton(window, buttons[i].second, active);
        }

        float y = PANEL_Y + 320.f;

        drawText(window, "Captures", 18, PANEL_X + 18.f, y, sf::Color::Black, true);
        y += 28.f;
        drawText(window, "Blue: " + std::to_string(game.capturesBlack), 16, PANEL_X + 18.f, y, playerDisplayColor(CellState::Black));
        y += 22.f;
        drawText(window, "Red:  " + std::to_string(game.capturesWhite), 16, PANEL_X + 18.f, y, playerDisplayColor(CellState::White));
        y += 34.f;

        drawText(window, "AI / Suggestion", 18, PANEL_X + 18.f, y, sf::Color::Black, true);
        y += 28.f;
        drawText(window, "Thinking time: " + std::to_string(game.debug.thinkingTimeMs) + " ms", 15, PANEL_X + 18.f, y);
        y += 22.f;
        drawText(window, "Suggested move: " + game.debug.evaluatedMove, 15, PANEL_X + 18.f, y);
        y += 34.f;

        drawText(window, "Debug", 18, PANEL_X + 18.f, y, sf::Color::Black, true);
        y += 28.f;
        drawText(window, "Score: " + std::to_string(game.debug.score), 15, PANEL_X + 18.f, y);
        y += 21.f;
        drawText(window, "Depth: " + std::to_string(game.debug.depthReached), 15, PANEL_X + 18.f, y);
        y += 21.f;
        drawText(window, "Nodes: " + std::to_string(game.debug.nodesExplored), 15, PANEL_X + 18.f, y);
        y += 34.f;

        drawText(window, "Status", 18, PANEL_X + 18.f, y, sf::Color::Black, true);
        y += 28.f;
        drawText(window, game.statusMessage, 14, PANEL_X + 18.f, y, sf::Color(30, 30, 30));
        y += 38.f;
        drawText(window, game.endgameMessage, 14, PANEL_X + 18.f, y, sf::Color(120, 20, 20), true);
    }

    void Renderer::draw(sf::RenderWindow& window, const EngineAPI& engine, const UIState& uiState) const
    {
        drawBoardBackground(window);
        drawGrid(window);
        drawStones(window, engine.state());
        drawPanel(window, engine, uiState);
    }
}
