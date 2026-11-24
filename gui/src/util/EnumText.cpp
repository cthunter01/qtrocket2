#include "QtRocket/gui/util/EnumText.h"

#include <string>
#include <string_view>

#include <QCoreApplication>
#include <QString>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/simulation/FlightDataType.h"

namespace QtRocket::Gui
{

QString translatedEnumText(std::string_view key, std::string_view englishText)
{
    // translate() takes NUL-terminated texts.
    const std::string source(englishText);
    const std::string disambiguation(key);
    return QCoreApplication::translate(kEnumTextContext, source.c_str(), disambiguation.c_str());
}

QString flightDataTypeText(const FlightDataType& type)
{
    if (!type.isBuiltin())
    {
        return QtStrings::fromUtf8(type.getName());
    }
    return translatedEnumText(type.getDisplayKey(), type.getName());
}

}  // namespace QtRocket::Gui
