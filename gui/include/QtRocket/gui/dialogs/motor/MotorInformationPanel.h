#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/util/GuiServices.h"

class QEvent;
class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QTimeZone;

namespace QtRocket
{
class ThrustCurveMotor;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ThrustCurvePreview;

/// The "Show Details" tab of the motor chooser (OpenRocket's MotorInformationPanel, swing
/// gui/dialogs/motor/thrustcurve/MotorInformationPanel.java): the selected thrust curve's data,
/// its description and a preview of the curves the curve list offers.
///
/// The rows, each a label and its value (updateData()), the units the default ones:
/// "Manufacturer:" (the display name), "Designation:", "Common name:", "Total impulse:" (in the
/// impulse unit, then its class in the dim text colour, "(76% C)", MotorClass's getDescription();
/// both with the tooltip of the class, "Class C (5 Ns - 10 Ns)"), "Avg. thrust:", "Max. thrust:"
/// (force), "Burn time:" (short time), "Launch mass:", "Empty mass:" (mass, the burnout mass),
/// "Motor type:" (the type's name, which OpenRocket does not translate), "Case info:",
/// "Propellant:", "Compatible Cases:" (the cases joined by ", "), "Data points:" (the time points
/// less one), a paragraph gap, "Last updated:" (formatUpdatedOn()) and "Source:"
/// (formatSource()). Below them the description (trimmed; "No description available." in italic
/// and the dim colour when there is none), in a read-only text area five rows high, two points
/// smaller, wrapping at words and scrolled to the top, which takes the height the panel gains;
/// then, a paragraph gap below, the ThrustCurvePreview, at least 300 x 180. A click on the
/// preview opens ThrustCurveMotorPlotDialog with the preview's curves and selection, when a curve
/// is selected (Java's listener returns otherwise).
///
/// The texts and the preview follow the palette: the dim colours are ThemeColors' TEXT_DIM and
/// TEXT, read again on every palette change.
///
/// The objects' names are Java's fields' (manufacturerLabel, ..., sourceLabel, comment; the
/// preview "layer", the CustomLayeredPane OpenRocket keeps in a local of that name).
///
/// Deviations from OpenRocket:
/// - No "Digest:" row (OpenRocket shows it only with -Dopenrocket.debug.motordigest).
/// - "Compatible Cases:" is plain text that wraps at the panel's width; OpenRocket's label holds
///   "<html>" + the cases + "<html>" (so that it wraps), which would also read a case's '<' or '&'
///   as markup.
/// - "Last updated:" of an instant whose date in the time zone is beyond the years Java's LocalDate
///   holds (more than 999999999 from year 0: the ends of Java's Instant range) shows that date;
///   OpenRocket's formatUpdatedOn() throws DateTimeException there, out of updateData() (the
///   probe).
/// - The layout is a grid of the labels' and the values' columns (MigLayout "fill"); its paragraph
///   gaps ("wrap para") are rows of 12 pixels.
class MotorInformationPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The description area's rows (new JTextArea(5, 5)).
    static constexpr int kCommentRows = 5;
    /// The points the description is smaller than the panel's font (changeFontSize(comment, -2)).
    static constexpr double kCommentFontChange = -2.0;
    /// MigLayout's paragraph gap ("wrap para") and its unrelated gap ("gapleft unrel") before the
    /// impulse's class.
    static constexpr int kParagraphGap = 12;
    static constexpr int kUnrelatedGap = 12;

    /// The panel without a curve (clearData()), the plot dialog's export through the prompts of
    /// @p services, which must outlive it.
    MotorInformationPanel(const GuiServices& services, QWidget* parent);
    ~MotorInformationPanel() override;
    MotorInformationPanel(const MotorInformationPanel&)            = delete;
    MotorInformationPanel& operator=(const MotorInformationPanel&) = delete;
    MotorInformationPanel(MotorInformationPanel&&)                 = delete;
    MotorInformationPanel& operator=(MotorInformationPanel&&)      = delete;

    /// clearData(): no curve; every value empty, no tooltips, "No description available.", an
    /// empty preview.
    void clearData();

    /// updateData(motors, selectedMotor): shows @p selectedMotor's data, and the preview the
    /// curves @p motors (the curve list's, in its order) with @p selectedMotor's index among them
    /// (-1 when it is not one of them, as Java's indexOf()). A null @p selectedMotor is
    /// clearData(). The curves are kept (shared) for the plot dialog.
    /// @throws BugError for a null curve in @p motors
    void updateData(std::span<const std::shared_ptr<const ThrustCurveMotor>> motors,
                    const std::shared_ptr<const ThrustCurveMotor>&           selectedMotor);

    /// The curves and the selection updateData() gave (none after clearData()).
    [[nodiscard]] const std::vector<std::shared_ptr<const ThrustCurveMotor>>& curves()
        const noexcept
    {
        return m_curves;
    }
    [[nodiscard]] const std::shared_ptr<const ThrustCurveMotor>& selectedMotor() const noexcept
    {
        return m_selectedMotor;
    }

    /// The preview (objectName "layer"): its chart shows curves() with the selection.
    [[nodiscard]] ThrustCurvePreview& preview() const noexcept { return *m_preview; }

    /// formatUpdatedOn(): "Last updated:" of @p updatedOn, trimmed (Java's String.trim()): when it
    /// is an ISO-8601 instant as Java's Instant.parse() reads it (DateTimeFormatter.ISO_INSTANT:
    /// "2024-01-02T03:04:05Z", a fraction of up to nine digits, an offset "+02:00"), the date it
    /// falls on in @p zone, as Java's LocalDate.toString() writes it ("2024-01-02", "+12024-01-02",
    /// "-0001-12-31"); otherwise the trimmed text (the database's dates, "2019-10-05", are not
    /// instants). The rules Instant.parse() follows are reimplemented from the probe
    /// tools/openrocket-probes/motor-selection-panel (no JDK source). OpenRocket uses the system's
    /// time zone (the panel passes QTimeZone::systemTimeZone()).
    [[nodiscard]] static QString formatUpdatedOn(std::string_view updatedOn, const QTimeZone& zone);

    /// formatSource(): "Source:" of @p source, trimmed; "thrustcurve.org" in any case becomes
    /// "ThrustCurve.org" (a text OpenRocket does not translate).
    [[nodiscard]] static QString formatSource(std::string_view source);

protected:
    void changeEvent(QEvent* event) override;

private:
    /// The row of the label @p text and the value label named @p name, in the grid's next row.
    [[nodiscard]] QLabel& addRow(QGridLayout& grid, const QString& text, const QString& name);
    /// The description and, below it, the preview, in the rows of @p layout after the labels'.
    void makeDescriptionAndPreview(QGridLayout& layout);
    /// setComment(): the trimmed @p text, or "No description available." in italic.
    void setComment(std::string_view text);
    /// The text colours of the class label and the description, from the palette.
    void updateColors();
    /// The preview's click (Java's ChartPanel listener).
    void plotRequested();

    GuiServices m_services;

    QLabel*             m_manufacturerLabel{nullptr};
    QLabel*             m_designationLabel{nullptr};
    QLabel*             m_commonNameLabel{nullptr};
    QLabel*             m_totalImpulseLabel{nullptr};
    QLabel*             m_classificationLabel{nullptr};
    QLabel*             m_avgThrustLabel{nullptr};
    QLabel*             m_maxThrustLabel{nullptr};
    QLabel*             m_burnTimeLabel{nullptr};
    QLabel*             m_launchMassLabel{nullptr};
    QLabel*             m_emptyMassLabel{nullptr};
    QLabel*             m_motorTypeLabel{nullptr};
    QLabel*             m_caseInfoLabel{nullptr};
    QLabel*             m_propInfoLabel{nullptr};
    QLabel*             m_compatibleCasesLabel{nullptr};
    QLabel*             m_dataPointsLabel{nullptr};
    QLabel*             m_lastUpdatedOnLabel{nullptr};
    QLabel*             m_sourceLabel{nullptr};
    QPlainTextEdit*     m_comment{nullptr};
    ThrustCurvePreview* m_preview{nullptr};
    int                 m_nextRow{0};
    /// Whether the description shows "No description available." (Java's noCommentFont).
    bool m_noComment{true};

    std::vector<std::shared_ptr<const ThrustCurveMotor>> m_curves;
    std::shared_ptr<const ThrustCurveMotor>              m_selectedMotor;
};

}  // namespace QtRocket::Gui
