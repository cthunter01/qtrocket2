#pragma once

#include <array>
#include <optional>

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

class QComboBox;
class QToolButton;

namespace QtRocket::Gui
{

class ZoomModel;

/// The zoom controls of a figure (OpenRocket's ScaleSelector, as its getAsPanel()): a zoom out
/// button, an editable combo box of the zoom levels, a zoom in button and a zoom to fit button,
/// in a row, driving a ZoomModel.
///
/// - The levels are 15 %, 25 %, 50 %, 75 %, 100 %, 150 % and 200 %, written by DecimalFormat
///   "0.#%" (formatScaleText()); the fitted scale shows the suffix of
///   ScaleSelector.lbl.ZoomFitSuffix, "(fit)": "100%(fit)". (Java's properties parser drops the
///   spaces before a value, so the file's " (fit)" is "(fit)" in OpenRocket too.)
/// - A text entered in the combo box (Enter, or leaving it) or a level picked from it asks the
///   model for that scale (parseScaleText(): "250", "250%" are 2.5; at least 0.5 %), or for the
///   fit when it says "(fit)" at 100 %; the box then shows the scale asked for, until the model
///   reports another one (ScaleSelectorTest), and an unreadable text shows the model's scale
///   again.
/// - Zoom out asks for nextLargerScale() (sic: Java's name), zoom in for nextSmallerScale(),
///   zoom to fit for the fit.
/// - The box follows the model's changed().
///
/// Deviation from OpenRocket: the combo box completes no text (Qt's editable combo boxes complete
/// inline unless told not to; Swing's do not).
class ScaleSelector : public QWidget
{
    Q_OBJECT

public:
    /// ScaleSelector.SCALE_LEVELS.
    static constexpr std::array kScaleLevels{0.15, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0};
    /// The smallest scale a text can ask for (0.5 %).
    static constexpr double kMinimumScale = 0.005;

    /// What a text asks for: a scale, and whether it is the fit.
    struct ScaleRequest
    {
        double scale{};
        bool   fit{};

        [[nodiscard]] bool operator==(const ScaleRequest&) const = default;
    };

    /// Zoom controls for @p model, which outlives them or is destroyed with the same parent
    /// (they do nothing without it).
    ScaleSelector(ZoomModel* model, QWidget* parent);

    [[nodiscard]] QToolButton* zoomOutButton() const noexcept { return m_zoomOut; }
    [[nodiscard]] QComboBox*   scaleCombo() const noexcept { return m_combo; }
    [[nodiscard]] QToolButton* zoomInButton() const noexcept { return m_zoomIn; }
    [[nodiscard]] QToolButton* zoomFitButton() const noexcept { return m_zoomFit; }

    /// Shows the model's scale (update()).
    void updateZoomText();

    /// The text of @p scale: DecimalFormat "0.#%" ("15%", "12.3%", U+221E "%" for an infinity,
    /// "NaN"), with the fit suffix when @p fit ("100%(fit)").
    [[nodiscard]] static QString formatScaleText(double scale, bool fit);
    /// The suffix of a fitted scale, ScaleSelector.lbl.ZoomFitSuffix: "(fit)".
    [[nodiscard]] static QString fitSuffix();
    /// What @p text asks for (the combo box's action): the number in it once the fit suffix and
    /// every '%' are removed, read by Double.parseDouble and divided by 100, at least
    /// kMinimumScale (NaN stays NaN); the fit when the text ends with the suffix and the scale is
    /// within 0.0001 of 1. nullopt for a text that is not a number.
    [[nodiscard]] static std::optional<ScaleRequest> parseScaleText(const QString& text);
    /// The scale zoom out goes to (getNextLargerScale()): the level below @p currentScale where
    /// it lies within the levels' bands (a level plus 0.05 to the next one plus 0.05); above
    /// 0.75, the next lower whole 100 % (ceil(scale - 1.05)), at least 200 %; else scale / 1.5.
    [[nodiscard]] static double nextLargerScale(double currentScale) noexcept;
    /// The scale zoom in goes to (getNextSmallerScale()): the level above @p currentScale within
    /// the bands (a level less 0.05 to the next one less 0.05); above 0.75, the next whole
    /// 100 % (floor(scale + 1.05)); else scale * 1.5.
    [[nodiscard]] static double nextSmallerScale(double currentScale) noexcept;

private:
    /// The combo box's action: asks the model for what @p text says.
    void applyText(const QString& text);
    /// setZoomText() of the model.
    void setZoomText();
    /// Shows formatScaleText() of @p scale and @p fit unless the box shows it already.
    void setZoomText(double scale, bool fit);
    void zoomOut();
    void zoomIn();
    void zoomFit();

    QPointer<ZoomModel> m_model;
    QToolButton*        m_zoomOut;
    QComboBox*          m_combo;
    QToolButton*        m_zoomIn;
    QToolButton*        m_zoomFit;
    bool                m_updatingText      = false;
    bool                m_inEditingFinished = false;
};

}  // namespace QtRocket::Gui
