#pragma once

// A translator for the tests of the GUI's enum texts (EnumText's translatedEnumText()): it
// translates every text of the context kEnumTextContext to "<key>|<English text>" (the key is
// the disambiguation), so that a test sees which key a text was asked with. InstalledKeyTranslator
// installs one for as long as it lives.

#include <string>

#include <QCoreApplication>
#include <QString>
#include <QTranslator>

#include "QtRocket/gui/util/EnumText.h"

namespace QtRocket::Gui::Test
{

/// A translator that knows every enum text: "<key>|<English text>".
class KeyTranslator : public QTranslator
{
public:
    [[nodiscard]] QString translate(const char* context, const char* sourceText,
                                    const char* disambiguation, int /*n*/) const override
    {
        if (context == nullptr || std::string(context) != kEnumTextContext ||
            disambiguation == nullptr)
        {
            return {};
        }
        return QString::fromUtf8(disambiguation) + QStringLiteral("|") +
               QString::fromUtf8(sourceText);
    }

    [[nodiscard]] bool isEmpty() const override { return false; }
};

/// Installs a KeyTranslator for as long as it lives.
class InstalledKeyTranslator
{
public:
    InstalledKeyTranslator() { QCoreApplication::installTranslator(&m_translator); }
    ~InstalledKeyTranslator() { QCoreApplication::removeTranslator(&m_translator); }
    InstalledKeyTranslator(const InstalledKeyTranslator&)            = delete;
    InstalledKeyTranslator& operator=(const InstalledKeyTranslator&) = delete;
    InstalledKeyTranslator(InstalledKeyTranslator&&)                 = delete;
    InstalledKeyTranslator& operator=(InstalledKeyTranslator&&)      = delete;

private:
    KeyTranslator m_translator;
};

}  // namespace QtRocket::Gui::Test
