#include "QtRocket/gui/util/QtStrings.h"

#include <filesystem>
#include <string>
#include <string_view>

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui::QtStrings
{

QString fromUtf8(std::string_view text)
{
    return QString::fromUtf8(QByteArrayView(text));
}

std::string toUtf8(const QString& text)
{
    return text.toUtf8().toStdString();
}

QString fromPath(const std::filesystem::path& path)
{
    return fromUtf8(pathToUtf8(path));
}

std::filesystem::path toPath(const QString& text)
{
    return pathFromUtf8(toUtf8(text));
}

QString unitText(const Unit& unit, double value)
{
    return fromUtf8(unit.toString(value));
}

QString unitTextWithUnit(const Unit& unit, double value)
{
    return fromUtf8(unit.toStringUnit(value));
}

QString defaultUnitText(UnitGroupId group, double value)
{
    return fromUtf8(unitGroup(group).toStringUnit(value));
}

QString unitSymbol(const Unit& unit)
{
    return fromUtf8(unit.getUnit());
}

QString javaDoubleText(double value)
{
    return fromUtf8(Strings::javaDoubleToString(value));
}

}  // namespace QtRocket::Gui::QtStrings
