#include "QtRocket/cli/CliOutput.h"

#include <cstdio>
#include <print>
#include <string_view>
#include <system_error>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"

namespace QtRocket
{

namespace
{

/// Prints @p text to @p stream and flushes it, unless @p failed says that the stream failed
/// before; sets @p failed when this fails (see CliOutput's constructor with C streams).
void printTo(std::FILE* stream, std::string_view text, bool& failed)
{
    if (failed)
    {
        return;
    }
    try
    {
        std::print(stream, "{}", text);
    }
    catch (const std::system_error&)
    {
        // A text larger than the stream's buffer is written at once and fails here; a smaller
        // one fails when it is flushed.
        failed = true;
        return;
    }
    if (std::fflush(stream) != 0 || std::ferror(stream) != 0)
    {
        failed = true;
    }
}

}  // namespace

void CliOutput::writeOut(std::string_view text)
{
    if (m_outStream != nullptr)
    {
        printTo(m_outStream, text, m_outFailed);
        return;
    }
    if (m_outSink)
    {
        m_outSink(text);
        return;
    }
    m_out += text;
}

void CliOutput::writeErr(std::string_view text)
{
    if (m_errStream != nullptr)
    {
        printTo(m_errStream, text, m_errFailed);
        return;
    }
    if (m_errSink)
    {
        m_errSink(text);
        return;
    }
    m_err += text;
}

void CliOutput::warningLines(const WarningSet& warnings)
{
    for (const Warning& warning : warnings)
    {
        errLine("warning: {}", warning.toString());
    }
}

}  // namespace QtRocket
