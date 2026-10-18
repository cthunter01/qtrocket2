#include "QtRocket/gui/util/EnumText.h"

#include <string>
#include <string_view>

#include <QCoreApplication>
#include <QString>

namespace QtRocket::Gui
{

QString translatedEnumText(std::string_view key, std::string_view englishText)
{
    // translate() takes NUL-terminated texts.
    const std::string source(englishText);
    const std::string disambiguation(key);
    return QCoreApplication::translate(kEnumTextContext, source.c_str(), disambiguation.c_str());
}

}  // namespace QtRocket::Gui
