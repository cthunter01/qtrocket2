#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

#include <QString>

/// The text a core enum shows in the GUI. Each enum of the core that OpenRocket shows by a
/// translated name has two functions found by argument-dependent lookup: displayKey(value), the
/// key of OpenRocket's messages.properties ("MassComponent.Altimeter"), and displayName(value),
/// its English text ("Altimeter"). Java shows trans.get(key); the GUI shows
/// QCoreApplication::translate(kEnumTextContext, <English text>, <key>): the English text when no
/// translator knows it, as tr() of the English text does for the GUI's own strings, with the key
/// as the disambiguation (two enums may share an English text, "None" or "Fixed", that another
/// language translates apart).
namespace QtRocket::Gui
{

/// The translation context of every enum text.
inline constexpr const char* kEnumTextContext = "EnumText";

/// An enum the GUI can show: displayKey() and displayName() exist for it.
template <class Enum>
concept DisplayedEnum = std::is_enum_v<Enum> && requires(Enum value) {
    { displayKey(value) } -> std::convertible_to<std::string_view>;
    { displayName(value) } -> std::convertible_to<std::string_view>;
};

/// QCoreApplication::translate(kEnumTextContext, @p englishText, @p key), both texts UTF-8.
[[nodiscard]] QString translatedEnumText(std::string_view key, std::string_view englishText);

/// The text of @p value: its displayName() through the application's translators, with its
/// displayKey().
template <DisplayedEnum Enum>
[[nodiscard]] QString enumText(Enum value)
{
    return translatedEnumText(displayKey(value), displayName(value));
}

}  // namespace QtRocket::Gui
