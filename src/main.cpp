#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <print>
#include <span>
#include <string_view>

#include "QtRocket/greet.h"

int main(int argc, char* argv[])
{
    try
    {
        const std::span        args(argv, static_cast<std::size_t>(argc));
        const std::string_view name = args.size() > 1 ? args[1] : "";
        std::println("{}", QtRocket::greet(name));
    }
    catch (const std::exception& e)
    {
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
