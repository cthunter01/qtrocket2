#pragma once

#include <QPalette>

namespace QtRocket::Gui::Test
{

/// Sets the application's palette for as long as it lives, and puts the previous one back (the
/// fixture GuiTest fails a test that leaves a palette changed). light() and dark() are the
/// palettes Qt generates from a light and from a dark window colour, for the code that chooses
/// OpenRocket's light or dark colours by the palette (ThemeColors).
class ScopedApplicationPalette
{
public:
    explicit ScopedApplicationPalette(const QPalette& palette);
    ~ScopedApplicationPalette();
    ScopedApplicationPalette(const ScopedApplicationPalette&)            = delete;
    ScopedApplicationPalette& operator=(const ScopedApplicationPalette&) = delete;
    ScopedApplicationPalette(ScopedApplicationPalette&&)                 = delete;
    ScopedApplicationPalette& operator=(ScopedApplicationPalette&&)      = delete;

    /// The palette of a light window (#f0f0f0): dark text on light colours.
    [[nodiscard]] static QPalette light();
    /// The palette of a dark window (#2b2b2b): light text on dark colours.
    [[nodiscard]] static QPalette dark();

private:
    QPalette m_previous;
};

}  // namespace QtRocket::Gui::Test
