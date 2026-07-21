#pragma once

#include <SFML/Graphics.hpp>

#include "Controller.hpp"
#include "Renderer.hpp"
#include "engine_api.hpp"

namespace gomoku
{
    class App
    {
    public:
        App();
        int run();

    private:
        sf::RenderWindow window_;
        EngineAPI engine_;
        Controller controller_;
        Renderer renderer_;
        UIState uiState_;
    };
}