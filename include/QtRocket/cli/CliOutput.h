#pragma once

#include <cstdio>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace QtRocket
{

class WarningSet;

/// Where the command-line tool writes: its standard output (the results) and its standard error
/// (the warnings of a load, the errors). The executable prints every text at once to its
/// standard streams (the constructor with two C streams: std::print, which writes Unicode to a
/// Windows console); the tests use the output without streams, which keeps what was written
/// (out(), err()), or with sinks of their own.
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

    /// An output that prints what is written to the C streams @p out and @p err (the
    /// executable's stdout and stderr) at once, each text with std::print and then flushed, and
    /// keeps nothing. A stream that cannot be written makes nothing throw: when @p out fails
    /// (std::print throws a std::system_error, which it does when the text does not fit into
    /// the stream's buffer, or std::fflush() or std::ferror() report an error: a full disk, a
    /// closed descriptor), that is remembered (outFailed()) and nothing more is written to it; a
    /// failure of @p err is handled the same way and not reported, as there is nowhere to report
    /// it. The streams must outlive the output.
    CliOutput(std::FILE* out, std::FILE* err) : m_outStream(out), m_errStream(err) { }

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

    /// What was written to the standard output; empty for an output with sinks or streams.
    [[nodiscard]] const std::string& out() const noexcept { return m_out; }
    /// What was written to the standard error; empty for an output with sinks or streams.
    [[nodiscard]] const std::string& err() const noexcept { return m_err; }

    /// Whether the standard output of an output with C streams could not be written in full
    /// (see the constructor); false for the other outputs.
    [[nodiscard]] bool outFailed() const noexcept { return m_outFailed; }

private:
    Sink        m_outSink;
    Sink        m_errSink;
    std::FILE*  m_outStream{nullptr};
    std::FILE*  m_errStream{nullptr};
    bool        m_outFailed{false};
    bool        m_errFailed{false};
    std::string m_out;
    std::string m_err;
};

}  // namespace QtRocket
