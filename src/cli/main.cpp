#include <cstdlib>
#include <exception>
#include <iostream>
#include <print>
#include <span>
#include <string_view>

#include "QtRocket/util/Version.h"

namespace
{

constexpr int kExitOk    = 0;
constexpr int kExitUsage = 1;

void printUsage()
{
    std::println("Usage: QtRocket_cli <command> [options]");
    std::println("");
    std::println("Commands:");
    std::println("  --version   Print the version and exit");
    std::println("  --help      Print this help and exit");
}

}  // namespace

int main(int argc, char* argv[])
{
    try
    {
        const std::span args(argv, static_cast<std::size_t>(argc));
        if (args.size() < 2)
        {
            printUsage();
            return kExitUsage;
        }
        const std::string_view command = args[1];
        if (command == "--version")
        {
            std::println("{}", QtRocket::creatorString());
            return kExitOk;
        }
        if (command == "--help" || command == "-h")
        {
            printUsage();
            return kExitOk;
        }
        std::println(stderr, "QtRocket_cli: unknown command '{}'", command);
        printUsage();
        return kExitUsage;
    }
    catch (const std::exception& e)
    {
        // Not std::println: it can throw, and nothing may escape main.
        std::cerr << "error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
