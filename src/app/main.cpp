#include "echo.hpp"
#include "echo_offline.hpp"

int main()
{
    ECHO app;
    // ECHOOFFLINE app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}