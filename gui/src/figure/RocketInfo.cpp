#include "QtRocket/gui/figure/RocketInfo.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QLatin1Char>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>

#include "QtRocket/gui/figure/Caret.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StabilityUnits.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// Points per pixel of a pixel-sized UI font (Qt's 72 / 96).
constexpr double kPointsPerPixel = 0.75;
/// The highest choice of RocketInfoFontSize (small 0, medium 1, large 2) and the points each
/// step adds.
constexpr int    kLargestFontSizeChoice = 2;
constexpr double kPointsPerFontSizeStep = 3.0;
/// The points the overlay's font is smaller than the UI font, and the small and the bold font
/// smaller and larger than the overlay's.
constexpr double kSmallerThanUiFont    = 2.0;
constexpr double kSmallAndBoldFontStep = 1.5;
/// How far above a row's baseline its caret's centre is, in lines.
constexpr double kCaretRaise = 0.3;

/// Where the overlay's texts go in the visible rectangle (paint(): x1, x2, y1, y2, line) and how
/// they are measured.
struct Frame
{
    double               x1{0.0};
    double               x2{0.0};
    double               y1{0.0};
    double               y2{0.0};
    double               line{0.0};
    const QFontMetricsF* regular{nullptr};
    const QFontMetricsF* small{nullptr};
};

/// The visual width of @p text (GlyphVector.getVisualBounds().getWidth()).
[[nodiscard]] double visualWidth(const QFontMetricsF& metrics, const QString& text)
{
    return metrics.tightBoundingRect(text).width();
}

/// A text item: @p text in @p role (measured with @p metrics) and @p colour at @p position.
[[nodiscard]] RocketInfo::Item textItem(RocketInfo::Item::FontRole role,
                                        const QFontMetricsF& metrics, const QColor& colour,
                                        const QString& text, QPointF position)
{
    RocketInfo::Item item;
    item.kind           = RocketInfo::Item::Kind::TEXT;
    item.font           = role;
    item.colour         = colour;
    item.text           = text;
    item.position       = position;
    const QRectF bounds = metrics.tightBoundingRect(text);
    item.width          = bounds.width();
    item.bounds         = bounds.translated(position);
    return item;
}

/// A caret item of @p kind in @p colour, centred at @p position.
[[nodiscard]] RocketInfo::Item caretItem(RocketInfo::Item::Kind kind, const QColor& colour,
                                         QPointF position)
{
    RocketInfo::Item item;
    item.kind           = kind;
    item.colour         = colour;
    item.position       = position;
    const double radius = Caret::kRadius / RocketInfo::kCaretScale;
    item.bounds = QRectF(position.x() - radius, position.y() - radius, 2 * radius, 2 * radius);
    return item;
}

/// The font of @p role among @p fonts.
[[nodiscard]] const QFont& fontOf(const RocketInfo::Fonts& fonts, RocketInfo::Item::FontRole role)
{
    switch (role)
    {
        case RocketInfo::Item::FontRole::REGULAR:
            return fonts.regular;
        case RocketInfo::Item::FontRole::SMALL:
            return fonts.small;
        case RocketInfo::Item::FontRole::BOLD:
            return fonts.bold;
    }
    QTROCKET_UNREACHABLE();
}

/// The texts at the top right of the overlay, in the order Java draws them (drawStabilityInfo()).
struct StabilityTexts
{
    QString stabilityValue;
    QString cgValue;
    QString cpValue;
    QString stabilityLabel;
    QString cgLabel;
    QString cpLabel;
    QString conditions;
};

/// drawStabilityInfo(): the values right-aligned, each label a space left of the widest value,
/// the carets 10 pixels left of the widest label, the conditions centred under the block (or
/// right-aligned when wider than it).
void addStabilityItems(const Frame& frame, const StabilityTexts& texts, int spaceWidth,
                       const QColor& textColour, const QColor& dimColour, const QColor& cgColour,
                       const QColor& cpColour, std::vector<RocketInfo::Item>& items)
{
    using Item                     = RocketInfo::Item;
    const QFontMetricsF& regular   = *frame.regular;
    const double         cgWidth   = visualWidth(regular, texts.cgValue);
    const double         cpWidth   = visualWidth(regular, texts.cpValue);
    const double         stabWidth = visualWidth(regular, texts.stabilityValue);

    // Add an extra space worth of width so the text doesn't run into the values
    const double unitWidth     = MathUtil::max(cpWidth, cgWidth) + spaceWidth;
    const double stabUnitWidth = stabWidth + spaceWidth;
    const double textWidth =
        std::max(visualWidth(regular, texts.cpLabel), visualWidth(regular, texts.cgLabel));
    const double line = frame.line;

    // Draw the stability, CG & CP values (and units)
    items.push_back(textItem(Item::FontRole::REGULAR, regular, textColour, texts.stabilityValue,
                             {frame.x2 - stabWidth, frame.y1}));
    items.push_back(textItem(Item::FontRole::REGULAR, regular, textColour, texts.cgValue,
                             {frame.x2 - cgWidth, frame.y1 + line}));
    items.push_back(textItem(Item::FontRole::REGULAR, regular, textColour, texts.cpValue,
                             {frame.x2 - cpWidth, frame.y1 + (2 * line)}));

    // Draw the stability, CG & CP labels
    items.push_back(textItem(
        Item::FontRole::REGULAR, regular, textColour, texts.stabilityLabel,
        {frame.x2 - stabUnitWidth - visualWidth(regular, texts.stabilityLabel), frame.y1}));
    items.push_back(
        textItem(Item::FontRole::REGULAR, regular, textColour, texts.cgLabel,
                 {frame.x2 - unitWidth - visualWidth(regular, texts.cgLabel), frame.y1 + line}));
    items.push_back(textItem(
        Item::FontRole::REGULAR, regular, textColour, texts.cpLabel,
        {frame.x2 - unitWidth - visualWidth(regular, texts.cpLabel), frame.y1 + (2 * line)}));

    // Draw the CG caret, then the CP caret
    const double caretX = frame.x2 - unitWidth - textWidth - RocketInfo::kCaretGap;
    items.push_back(caretItem(Item::Kind::CG_CARET, cgColour,
                              {caretX, frame.y1 + line - (kCaretRaise * line)}));
    items.push_back(caretItem(Item::Kind::CP_CARET, cpColour,
                              {caretX, frame.y1 + (2 * line) - (kCaretRaise * line)}));

    const double atWidth = visualWidth(*frame.small, texts.conditions);
    const double block   = unitWidth + textWidth + RocketInfo::kCaretGap;
    const double atPos = block > atWidth ? frame.x2 - ((block + atWidth) / 2) : frame.x2 - atWidth;
    items.push_back(textItem(Item::FontRole::SMALL, *frame.small, dimColour, texts.conditions,
                             {atPos, frame.y1 + (3 * line)}));
}

/// drawWarnings(): @p texts centred on a common axis at the bottom right, the last on the bottom
/// row.
void addWarningItems(const Frame& frame, const std::vector<QString>& texts, const QColor& colour,
                     std::vector<RocketInfo::Item>& items)
{
    double maximum = 0;
    for (const QString& text : texts)
    {
        maximum = std::max(maximum, visualWidth(*frame.regular, text));
    }
    double y = frame.y2 - (frame.line * static_cast<double>(texts.size() - 1));
    for (const QString& text : texts)
    {
        const double width = visualWidth(*frame.regular, text);
        items.push_back(textItem(RocketInfo::Item::FontRole::REGULAR, *frame.regular, colour, text,
                                 {frame.x2 - (maximum / 2) - (width / 2), y}));
        y += frame.line;
    }
}

}  // namespace

RocketInfo::RocketInfo(const FlightConfiguration& configuration, const Preferences& preferences)
  : m_preferences(&preferences), m_mach(preferences.getDefaultMach())
{
    setCurrentConfig(configuration);
}

void RocketInfo::setCurrentConfig(const FlightConfiguration& configuration)
{
    m_rocketName              = QtStrings::fromUtf8(configuration.getRocket().getName());
    m_hasMotors               = configuration.hasMotors();
    m_stabilityUnits          = StabilityUnits::stabilityUnits(configuration);
    m_secondaryStabilityUnits = StabilityUnits::secondaryStabilityUnits(configuration);
}

QString RocketInfo::getMassWithMotors(const Unit& unit) const
{
    return QtStrings::unitTextWithUnit(unit, m_massWithMotors);
}

QString RocketInfo::getStabilityCombined() const
{
    const Unit&  stabilityUnit          = m_stabilityUnits->getDefaultUnit();
    const Unit&  secondaryStabilityUnit = m_secondaryStabilityUnits->getDefaultUnit();
    const double stability              = m_cp - m_cg;
    QString      stabilityText          = QtStrings::unitTextWithUnit(stabilityUnit, stability);

    // Don't display secondary units if the stability is NaN, or if the secondary unit is the same
    // as the primary unit, or if it is disabled in the preferences
    if (std::isnan(stability) || secondaryStabilityUnit.equals(stabilityUnit) ||
        !m_preferences->isDisplaySecondaryStability())
    {
        return stabilityText;
    }
    return stabilityText + QStringLiteral(" / ") +
           QtStrings::unitTextWithUnit(secondaryStabilityUnit, stability);
}

QString RocketInfo::getCp(const Unit& unit) const
{
    return QtStrings::unitTextWithUnit(unit, m_cp);
}

QString RocketInfo::getCp() const
{
    return QtStrings::defaultUnitText(UnitGroupId::LENGTH, m_cp);
}

QString RocketInfo::getCg(const Unit& unit) const
{
    return QtStrings::unitTextWithUnit(unit, m_cg);
}

QString RocketInfo::getCg() const
{
    return QtStrings::defaultUnitText(UnitGroupId::LENGTH, m_cg);
}

RocketInfo::Fonts RocketInfo::fonts(const QFont& uiFont, const Preferences& preferences)
{
    // SwingPreferences.getUIFontSize() (an int of at least 8) and getRocketInfoFontSize(), with
    // the UI scale 1.
    const double uiSize =
        uiFont.pointSizeF() > 0 ? uiFont.pointSizeF() : uiFont.pixelSize() * kPointsPerPixel;
    const double uiFontSize = std::max(kMinimumFontSize, std::floor(uiSize));
    const int    choice =
        preferences.getChoice(PreferenceKeys::kRocketInfoFontSize, kLargestFontSizeChoice, 0);
    const double size = std::max(
        kMinimumFontSize, (uiFontSize - kSmallerThanUiFont) + (kPointsPerFontSizeStep * choice));

    Fonts fonts{.regular = uiFont, .small = uiFont, .bold = uiFont};
    fonts.regular.setPointSizeF(size);
    fonts.regular.setBold(false);
    fonts.small.setPointSizeF(size - kSmallAndBoldFontStep);
    fonts.small.setBold(false);
    fonts.bold.setPointSizeF(size + kSmallAndBoldFontStep);
    fonts.bold.setBold(true);
    return fonts;
}

std::vector<RocketInfo::Item> RocketInfo::layout(const QRect& visible, const QFont& uiFont,
                                                 const QPaintDevice* device) const
{
    const Fonts         fonts = RocketInfo::fonts(uiFont, *m_preferences);
    const QFontMetricsF regular(fonts.regular, device);
    const QFontMetricsF small(fonts.small, device);

    // The line metrics' height (ascent, descent and leading) plus the descent.
    Frame frame;
    frame.line    = regular.lineSpacing() + regular.descent();
    frame.x1      = visible.x() + kMargin;
    frame.x2      = visible.x() + visible.width() - kMargin;
    frame.y1      = visible.y() + frame.line;
    frame.y2      = visible.y() + visible.height() - kMargin;
    frame.regular = &regular;
    frame.small   = &small;

    std::vector<Item> items;
    const QColor      text = textColor();

    // drawMainInfo()
    const double line = frame.line;
    items.push_back(
        textItem(Item::FontRole::REGULAR, regular, text, m_rocketName, {frame.x1, frame.y1}));
    //: RocketInfo.lengthLine.Length
    const QString length = tr("Length") + QLatin1Char(' ') +
                           QtStrings::defaultUnitText(UnitGroupId::LENGTH, m_length) +
                           //: RocketInfo.lengthLine.maxdiameter
                           tr(", max. diameter") + QLatin1Char(' ') +
                           QtStrings::defaultUnitText(UnitGroupId::LENGTH, m_diameter);
    items.push_back(
        textItem(Item::FontRole::REGULAR, regular, text, length, {frame.x1, frame.y1 + line}));
    //: RocketInfo.massWithoutMotors
    const QString massWithoutMotors =
        tr("Mass with no motors") + QLatin1Char(' ') +
        QtStrings::defaultUnitText(UnitGroupId::MASS, m_massWithoutMotors);
    items.push_back(textItem(Item::FontRole::REGULAR, regular, text, massWithoutMotors,
                             {frame.x1, frame.y1 + (2 * line)}));
    if (m_hasMotors)
    {
        //: RocketInfo.massWithMotors
        const QString massWithMotors =
            tr("Mass with motors") + QLatin1Char(' ') +
            QtStrings::defaultUnitText(UnitGroupId::MASS, m_massWithMotors);
        items.push_back(textItem(Item::FontRole::REGULAR, regular, text, massWithMotors,
                                 {frame.x1, frame.y1 + (3 * line)}));
    }

    // drawStabilityInfo()
    StabilityTexts stability;
    stability.stabilityValue = getStabilityCombined();
    stability.cgValue        = getCg();
    stability.cpValue        = getCp();
    //: RocketInfo.cgText
    stability.cgLabel = tr("CG:");
    //: RocketInfo.cpText
    stability.cpLabel = tr("CP:");
    //: RocketInfo.stabText
    stability.stabilityLabel = tr("Stability:");
    //: RocketInfo.at
    stability.conditions =
        tr("at M=") + QtStrings::defaultUnitText(UnitGroupId::COEFFICIENT, m_mach);
    if (!std::isnan(m_aoa))
    {
        stability.conditions +=
            QStringLiteral(" \u03B1=") + QtStrings::defaultUnitText(UnitGroupId::ANGLE, m_aoa);
    }
    if (!std::isnan(m_theta))
    {
        stability.conditions +=
            QStringLiteral(" \u0398=") + QtStrings::defaultUnitText(UnitGroupId::ANGLE, m_theta);
    }
    // GlyphVector visual bounds drops the spaces, so we'll add them
    const int spaceWidth = QFontMetrics(fonts.regular, device).horizontalAdvance(QLatin1Char(' '));
    addStabilityItems(frame, stability, spaceWidth, text, dimTextColor(), m_cgCaret.color(),
                      m_cpCaret.color(), items);

    // drawWarnings()
    if (!m_warnings.empty())
    {
        std::vector<QString> warnings;
        if (m_showWarnings)
        {
            //: RocketInfo.Warning
            warnings.push_back(tr("Warning:"));
            for (const Warning& warning : m_warnings)
            {
                warnings.push_back(QtStrings::fromUtf8(warning.toString()));
            }
        }
        else
        {
            //: RocketInfo.lbl.warnings (Java: "%d warning(s)")
            warnings.push_back(tr("%1 warning(s)").arg(QString::number(m_warnings.size())));
        }
        // Keep error color from theme for warnings
        addWarningItems(frame, warnings, ThemeColors::color(ThemeColors::Key::ERROR), items);
    }
    return items;
}

void RocketInfo::paint(QPainter& painter, double /*scale*/, const QRect& visible)
{
    const QFont             uiFont = painter.font();
    const Fonts             fonts  = RocketInfo::fonts(uiFont, *m_preferences);
    const std::vector<Item> items  = layout(visible, uiFont, painter.device());

    painter.save();
    for (const Item& item : items)
    {
        switch (item.kind)
        {
            case Item::Kind::TEXT:
                painter.setFont(fontOf(fonts, item.font));
                painter.setPen(item.colour);
                painter.drawText(item.position, item.text);
                break;
            case Item::Kind::CG_CARET:
                m_cgCaret.setPosition(item.position.x(), item.position.y());
                m_cgCaret.paint(painter, kCaretScale);
                break;
            case Item::Kind::CP_CARET:
                m_cpCaret.setPosition(item.position.x(), item.position.y());
                m_cpCaret.paint(painter, kCaretScale);
                break;
        }
    }
    painter.restore();
}

QColor RocketInfo::textColor() const
{
    return m_customTextColor.isValid() ? m_customTextColor
                                       : ThemeColors::color(ThemeColors::Key::TEXT);
}

QColor RocketInfo::dimTextColor() const
{
    return m_customTextColor.isValid() ? m_customTextColor
                                       : ThemeColors::color(ThemeColors::Key::TEXT_DIM);
}

}  // namespace QtRocket::Gui
