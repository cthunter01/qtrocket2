#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace QtRocket
{

class WarningSet;

/// Where the command-line tool writes: its standard output (the results) and its standard error
/// (the warnings of a load, the errors). The executable hands every text on at once to two sinks
/// that print it (std::print to stdout and stderr, which writes Unicode to a Windows console);
/// the tests use the output without sinks, which keeps what was written (out(), err()).
///
/// Every line ends with '\n', on every platform.
class CliOutput
{
public:
    /// Takes a piece of text, which holds whole lines.
    using Sink = std::function<void(std::string_view)>;

    /// An output that keeps what is written, for out() and err().
    CliOutput() = default;

    /// An output that hands what is written to @p out and @p err at once and keeps nothing.
    CliOutput(Sink out, Sink err) : m_outSink(std::move(out)), m_errSink(std::move(err)) { }

    /// Writes @p text to the standard output as it is.
    void writeOut(std::string_view text);
    /// Writes @p text to the standard error as it is.
    void writeErr(std::string_view text);

    /// Writes a line to the standard output: @p format with @p args, and '\n'.
    template <class... Args>
    void outLine(std::format_string<Args...> format, Args&&... args)
    {
        writeOut(std::format(format, std::forward<Args>(args)...) + '\n');
    }

    /// Writes a line to the standard error: @p format with @p args, and '\n'.
    template <class... Args>
    void errLine(std::format_string<Args...> format, Args&&... args)
    {
        writeErr(std::format(format, std::forward<Args>(args)...) + '\n');
    }

    /// Writes each of @p warnings (Message::toString()) to the standard error as a line
    /// "warning: <text>", in the set's order.
    void warningLines(const WarningSet& warnings);

    /// What was written to the standard output; empty for an output with sinks.
    [[nodiscard]] const std::string& out() const noexcept { return m_out; }
    /// What was written to the standard error; empty for an output with sinks.
    [[nodiscard]] const std::string& err() const noexcept { return m_err; }

private:
    Sink        m_outSink;
    Sink        m_errSink;
    std::string m_out;
    std::string m_err;
};

}  // namespace QtRocket
