// QtRocket_cli: the command-line tool. Everything it does is in QtRocket_cli_lib (cli/Cli.h);
// this reads the arguments and the data directory and prints.

#include <cstddef>
#include <cstdio>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/Cli.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/DataDirectory.h"
#include "QtRocket/cli/ExitCode.h"

int main(int argc, char* argv[])
{
    try
    {
        QtRocket::DataDirectory::useBinaryStandardStreams();
        const std::vector<std::string> arguments =
            QtRocket::DataDirectory::commandLineArguments(argc, argv);
        const std::span<char*>        all(argv, static_cast<std::size_t>(argc > 0 ? argc : 0));
        const std::string_view        argv0 = !all.empty() && all[0] != nullptr ? all[0] : "";
        const QtRocket::Cli::Settings settings{
            .dataDirectory = QtRocket::DataDirectory::dataDirectory(argv0),
            .motorDatabase = nullptr};
        QtRocket::CliOutput output(
            [](std::string_view text) {
                std::print(stdout, "{}", text);
                std::fflush(stdout);
            },
            [](std::string_view text) {
                std::print(stderr, "{}", text);
                std::fflush(stderr);
            });
        return QtRocket::exitStatus(QtRocket::Cli::run(arguments, settings, output));
    }
    catch (...)
    {
        // Not std::print: it can throw, and nothing may escape main.
        static_cast<void>(std::fputs("QtRocket_cli: internal error\n", stderr));
        return QtRocket::exitStatus(QtRocket::ExitCode::INTERNAL);
    }
}
