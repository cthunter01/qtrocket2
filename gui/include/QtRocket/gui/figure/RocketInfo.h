#pragma once

#include <limits>
#include <memory>
#include <vector>

#include <QColor>
#include <QCoreApplication>
#include <QFont>
#include <QPointF>
#include <QRectF>
#include <QString>

#include "QtRocket/gui/figure/CGCaret.h"
#include "QtRocket/gui/figure/CPCaret.h"
#include "QtRocket/gui/figure/FigureElement.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/unit/UnitGroup.h"

class QPaintDevice;
class QPainter;
class QRect;

namespace QtRocket
{
class FlightConfiguration;
class Preferences;
class Unit;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The text over the design view's figure (OpenRocket's RocketInfo): a FigureElement painted in
/// the figure's visible rectangle (an absolute extra of RocketFigure, so it stays put while the
/// figure is scrolled or zoomed), with the numbers DesignPhysics computes:
///
/// - top left, in the text colour: the rocket's name; "Length L, max. diameter D"; "Mass with no
///   motors M"; "Mass with motors M" when the configuration has motors;
/// - top right, right-aligned: "Stability:" and the margin in the configuration's stability unit,
///   with " / " and the secondary unit unless the margin is NaN, the two units are the same or
///   the preference DisplaySecondaryStability is off (getStabilityCombined()); "CG:" and "CP:"
///   with their values, each label after a caret (CGCaret, CPCaret at the scale 1.7, upright);
///   under them, small and dim, "at M=0.300" with " \u03B1=" (alpha) and " \u0398=" (Theta) when
///   the angle of attack or the wind direction is set;
/// - bottom right, in the error colour, when there are warnings: "Warning:" and a line per
///   warning, or "%1 warning(s)" when they are not shown (setShowWarnings()), centred on a
///   common axis.
///
/// The layout is Java's, in the widget's pixels: a margin of kMargin pixels, rows one line apart
/// (the regular font's line spacing plus its descent), the first row's baseline one line below
/// the top; a text's width is its visual (ink) width (Java: GlyphVector.getVisualBounds()), and a
/// label stands a space (the font's integer advance of " ") left of the widest value. layout()
/// gives every text and caret with its place, which paint() draws. The values are written with
/// the default units at paint time; the stability units are the configuration's
/// (StabilityUnits::stabilityUnits()), made anew by setCurrentConfig().
///
/// Fonts (fonts()): the UI font (the painter's, which the figure gives its own font) at the size
/// S = max(8, floor(UI size) - 2 + 3 * RocketInfoFontSize), the small one at S - 1.5 and the bold
/// one at S + 1.5 (the flight block's, Milestone 4), as SwingPreferences
/// .getRocketInfoFontSize() computes it with the UI scale 1 (UIScale is the UI page's,
/// Milestone 5). Colours: ThemeColors' TEXT, TEXT_DIM and ERROR, read at paint time; a custom
/// text colour (setCustomTextColor(), the view settings of Milestone 5) replaces the first two.
///
/// Deviations from OpenRocket:
/// - The sizes are in the unit of the UI font: points when it has a point size (a pixel-sized
///   font counts 0.75 points per pixel), where Java's are Java2D units (pixels) of its bundled
///   Inter font. QtRocket draws in the platform's font, whose sizes are points.
/// - setCurrentConfig() keeps the rocket's name and whether the configuration has motors, read
///   then (Java reads them from the configuration at each paint), so that no configuration
///   object is held: an undo replaces them all. The design view sets the configuration anew on
///   every change of the rocket, as Java's does, so the same is shown.
/// - No flight block ("Flight configuration:", "Apogee:", ...; "Calculating..."): it shows the
///   background simulation of Milestone 4. What is shown is what OpenRocket shows with the
///   preference "backgroundFlight" off.
/// - Only paint(painter, scale, visible): Java's paint(g2, scale) throws.
class RocketInfo final : public FigureElement
{
    Q_DECLARE_TR_FUNCTIONS(RocketInfo)

public:
    /// MARGIN: the pixels between the texts and the edges of the visible rectangle.
    static constexpr int kMargin = 8;
    /// The scale of the carets beside "CG:" and "CP:" (radius 7 / 1.7 pixels).
    static constexpr double kCaretScale = 1.7;
    /// The pixels between a caret and the widest label (Java's literal 10).
    static constexpr double kCaretGap = 10.0;
    /// The smallest font size (SwingPreferences: Math.max(8, ...)).
    static constexpr double kMinimumFontSize = 8.0;

    /// The three fonts of the overlay (updateFontSizes()).
    struct Fonts
    {
        QFont regular;
        QFont small;
        QFont bold;
    };

    /// One thing paint() draws, where it draws it.
    struct Item
    {
        enum class Kind
        {
            TEXT,
            CG_CARET,
            CP_CARET,
        };
        enum class FontRole
        {
            REGULAR,
            SMALL,
            BOLD,
        };

        Kind     kind{Kind::TEXT};
        FontRole font{FontRole::REGULAR};
        /// The colour of a text (a caret has its own: Caret::color()).
        QColor  colour;
        QString text;
        /// A text's baseline origin (drawGlyphVector()'s x, y), a caret's centre.
        QPointF position;
        /// The width the layout gives a text: its visual width (getVisualBounds().getWidth()).
        double width{0.0};
        /// Where it lands: a text's visual bounds at its position, a caret's square.
        QRectF bounds;
    };

    /// The overlay of @p configuration (RocketInfo(configuration): setCurrentConfig()), with the
    /// Mach number of @p preferences (getDefaultMach()), no angle of attack or wind direction,
    /// everything else 0, no warnings, warnings shown. @p preferences must outlive it.
    RocketInfo(const FlightConfiguration& configuration, const Preferences& preferences);

    /// Takes the name, the motors and the stability units of @p configuration
    /// (setCurrentConfig()). The configuration must be the rocket's, or outlive the overlay
    /// (StabilityUnits).
    void setCurrentConfig(const FlightConfiguration& configuration);

    void setCG(double cg) noexcept { m_cg = cg; }
    void setCP(double cp) noexcept { m_cp = cp; }
    void setLength(double length) noexcept { m_length = length; }
    void setDiameter(double diameter) noexcept { m_diameter = diameter; }
    void setMassWithMotors(double mass) noexcept { m_massWithMotors = mass; }
    void setMassWithoutMotors(double mass) noexcept { m_massWithoutMotors = mass; }
    /// A copy of @p warnings (setWarnings(): warnings.clone()).
    void setWarnings(const WarningSet& warnings) { m_warnings = warnings; }
    /// Whether the warnings are listed (true) or only counted (setShowWarnings()).
    void setShowWarnings(bool showWarnings) noexcept { m_showWarnings = showWarnings; }
    /// The angle of attack in radians; NaN (the default) for none (setAOA()).
    void setAOA(double aoa) noexcept { m_aoa = aoa; }
    /// The wind direction in radians; NaN (the default) for none (setTheta()).
    void setTheta(double theta) noexcept { m_theta = theta; }
    void setMach(double mach) noexcept { m_mach = mach; }

    [[nodiscard]] double cg() const noexcept { return m_cg; }
    [[nodiscard]] double cp() const noexcept { return m_cp; }
    [[nodiscard]] double length() const noexcept { return m_length; }
    [[nodiscard]] double diameter() const noexcept { return m_diameter; }
    [[nodiscard]] double massWithMotors() const noexcept { return m_massWithMotors; }
    [[nodiscard]] double massWithoutMotors() const noexcept { return m_massWithoutMotors; }
    [[nodiscard]] const WarningSet& warnings() const noexcept { return m_warnings; }
    [[nodiscard]] bool              isShowWarnings() const noexcept { return m_showWarnings; }
    [[nodiscard]] double            aoa() const noexcept { return m_aoa; }
    [[nodiscard]] double            theta() const noexcept { return m_theta; }
    [[nodiscard]] double            mach() const noexcept { return m_mach; }
    /// The rocket's name and whether the configuration has motors, as setCurrentConfig() read
    /// them.
    [[nodiscard]] const QString& rocketName() const noexcept { return m_rocketName; }
    [[nodiscard]] bool           hasMotors() const noexcept { return m_hasMotors; }

    /// The mass with motors in @p unit, with the unit (getMassWithMotors(Unit)).
    [[nodiscard]] QString getMassWithMotors(const Unit& unit) const;
    /// The stability margin (CP - CG) in the stability unit, and " / " with the secondary unit
    /// unless the margin is NaN, the units are the same or DisplaySecondaryStability is off
    /// (getStabilityCombined()): "2.83 cal / 16.6 %".
    [[nodiscard]] QString getStabilityCombined() const;
    /// The CP in @p unit, with the unit (getCp(Unit)); getCp() in the default length unit.
    [[nodiscard]] QString getCp(const Unit& unit) const;
    [[nodiscard]] QString getCp() const;
    /// The CG in @p unit, with the unit (getCg(Unit)); getCg() in the default length unit.
    [[nodiscard]] QString getCg(const Unit& unit) const;
    [[nodiscard]] QString getCg() const;

    /// The colour of the texts but the warnings (RocketPanel.updateTextColors(): the document's
    /// or the preferences' 2D text colour); an invalid colour (the default) for the theme's.
    void setCustomTextColor(const QColor& colour) { m_customTextColor = colour; }
    [[nodiscard]] const QColor& customTextColor() const noexcept { return m_customTextColor; }

    /// The overlay's fonts for the UI font @p uiFont and the preference RocketInfoFontSize of
    /// @p preferences (see the class comment).
    [[nodiscard]] static Fonts fonts(const QFont& uiFont, const Preferences& preferences);

    /// What paint() draws in @p visible (the figure's visible rectangle, in pixels), with
    /// fonts(@p uiFont) measured on @p device (nullptr: the screen), in the order Java draws it.
    [[nodiscard]] std::vector<Item> layout(const QRect& visible, const QFont& uiFont,
                                           const QPaintDevice* device) const;

    /// Draws layout() of @p visible with the painter's font as the UI font; @p scale is not used
    /// (the overlay is in pixels).
    void paint(QPainter& painter, double scale, const QRect& visible) override;

private:
    /// The colour of the texts and of the dim texts (getTextColor(), getDimTextColor()).
    [[nodiscard]] QColor textColor() const;
    [[nodiscard]] QColor dimTextColor() const;

    const Preferences*                             m_preferences;
    std::unique_ptr<UnitGroup::StabilityUnitGroup> m_stabilityUnits;
    std::unique_ptr<UnitGroup::StabilityUnitGroup> m_secondaryStabilityUnits;
    QString                                        m_rocketName;
    bool                                           m_hasMotors{false};
    double                                         m_cg{0.0};
    double                                         m_cp{0.0};
    double                                         m_length{0.0};
    double                                         m_diameter{0.0};
    double                                         m_massWithMotors{0.0};
    double                                         m_aoa{std::numeric_limits<double>::quiet_NaN()};
    double     m_theta{std::numeric_limits<double>::quiet_NaN()};
    double     m_mach;
    double     m_massWithoutMotors{0.0};
    WarningSet m_warnings;
    bool       m_showWarnings{true};
    QColor     m_customTextColor;
    /// The carets drawn beside "CG:" and "CP:".
    CGCaret m_cgCaret{0.0, 0.0};
    CPCaret m_cpCaret{0.0, 0.0};
};

}  // namespace QtRocket::Gui
