#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <QString>

#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{
class Unit;
}

/// The GUI's side of the core's text: the core holds every text as UTF-8 std::string (names,
/// unit symbols such as "°", messages), the GUI as QString. Every conversion between them goes
/// through here, so that no text takes the Windows ANSI code page or Latin-1 on the way, and
/// every path through the core's pathFromUtf8()/pathToUtf8() (util/FileIo.h): never
/// QString::toStdString() or std::filesystem::path(std::string) for a path.
namespace QtRocket::Gui::QtStrings
{

/// The UTF-8 text @p text as a QString; a sequence that is not UTF-8 becomes U+FFFD.
[[nodiscard]] QString fromUtf8(std::string_view text);

/// @p text as UTF-8; a lone surrogate becomes U+FFFD (QString::toUtf8()).
[[nodiscard]] std::string toUtf8(const QString& text);

/// The path @p path as text for the GUI and for Qt's file APIs: pathToUtf8(), in the native
/// format (backslashes on Windows).
[[nodiscard]] QString fromPath(const std::filesystem::path& path);

/// The path the text @p text names (from a file dialog, QFileInfo, the command line, ...):
/// pathFromUtf8(), which takes '/' as a separator on every platform.
[[nodiscard]] std::filesystem::path toPath(const QString& text);

/// @p value in @p unit without the unit, as the unit writes it (Unit::toString(), e.g. "12.5").
/// @p value is in SI units.
[[nodiscard]] QString unitText(const Unit& unit, double value);

/// @p value in @p unit with the unit (Unit::toStringUnit(), e.g. "12.5 cm", "30°").
[[nodiscard]] QString unitTextWithUnit(const Unit& unit, double value);

/// @p value in the default unit of the group @p group, with the unit
/// (unitGroup(group).toStringUnit(), the texts of the tree's tooltips and the figure's info).
[[nodiscard]] QString defaultUnitText(UnitGroupId group, double value);

/// The symbol of @p unit ("cm", "°", "in"), as the unit selectors show it.
[[nodiscard]] QString unitSymbol(const Unit& unit);

/// Java's Double.toString(@p value) (Strings::javaDoubleToString()): "0.5", "1.0E-4", "NaN",
/// for the texts OpenRocket writes with string concatenation.
[[nodiscard]] QString javaDoubleText(double value);

}  // namespace QtRocket::Gui::QtStrings
