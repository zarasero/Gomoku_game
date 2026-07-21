#include "App.hpp"

namespace gomoku
{
    App::App()
        : window_(sf::VideoMode(WINDOW_W, WINDOW_H), "Gomoku")
    {
        window_.setFramerateLimit(60);

        const bool fontLoaded = renderer_.loadFont();
        engine_.state().statusMessage = fontLoaded
            ? "Welcome to Gomoku."
            : "Font not found. UI text may be missing.";
    }

    int App::run()
    {
        while (window_.isOpen())
        {
            sf::Event event;
            while (window_.pollEvent(event))
            {
                if (event.type == sf::Event::Closed)
                {
                    window_.close();
                }
                else if (event.type == sf::Event::MouseButtonPressed
                      && event.mouseButton.button == sf::Mouse::Left)
                {
                    controller_.handleMouseClick(
                        engine_,
                        uiState_,
                        sf::Vector2f(
                            static_cast<float>(event.mouseButton.x),
                            static_cast<float>(event.mouseButton.y)));
                }
            }

            controller_.update(engine_, uiState_);

            window_.clear(sf::Color(235, 205, 150));
            renderer_.draw(window_, engine_, uiState_);
            window_.display();
        }

        return 0;
    }
}