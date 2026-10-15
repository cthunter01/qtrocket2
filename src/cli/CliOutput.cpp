#include "QtRocket/cli/CliOutput.h"

#include <string_view>

#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"

namespace QtRocket
{

void CliOutput::writeOut(std::string_view text)
{
    if (m_outSink)
    {
        m_outSink(text);
        return;
    }
    m_out += text;
}

void CliOutput::writeErr(std::string_view text)
{
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
