// QtRocket_cli: the command-line tool. Everything it does is in QtRocket_cli_lib (cli/Cli.h);
// this reads the arguments and the data directory and runs it on the standard streams.

#include <cstddef>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/cli/Cli.h"
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
        return QtRocket::exitStatus(
            QtRocket::Cli::runWithStreams(arguments, settings, stdout, stderr));
    }
    catch (...)
    {
        // Not std::print: it can throw, and nothing may escape main.
        static_cast<void>(std::fputs("QtRocket_cli: internal error\n", stderr));
        return QtRocket::exitStatus(QtRocket::ExitCode::INTERNAL);
    }
}
