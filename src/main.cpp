#include <exception>
#include <iostream>
#include <new>

#include "App.hpp"

int main()
{
    try
    {
        gomoku::App app;
        return app.run();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "Fatal error: out of memory.\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << '\n';
        return 1;
    }
    catch (...)
    {
        std::cerr << "Fatal error: unexpected exception.\n";
        return 1;
    }
}
