// The motor chooser's "Show Details" tab (Java: MotorInformationPanel). OpenRocket has no JUnit
// test of it. The texts it shows for every curve of smallMotorDatabase() are compared with
// OpenRocket's in motor_information_golden_tests.cpp; formatUpdatedOn() and formatSource() here
// with the UPDATED, ZONE and SOURCE lines of the probe
// tools/openrocket-probes/motor-selection-panel (its expected.txt: OpenRocket's private methods
// called for edge texts, and java.time's dates of the instants in fixed-offset zones; the probe
// runs in UTC).

#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QString>
#include <QTest>
#include <QTimeZone>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/dialogs/ThrustCurveMotorPlotDialog.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/plot/ThrustCurvePreview.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "ScopedApplicationPalette.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

using MotorInformationPanelTest = Gui::Test::GuiTest;
using Curves                    = std::vector<std::shared_ptr<const ThrustCurveMotor>>;

/// A motor of three points with the description, the update date, the source and the case given.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> makeMotor(std::string description,
                                                                std::string updatedOn,
                                                                std::string source,
                                                                std::string caseInfo = {})
{
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("AeroTech"))
            .setDesignation("H128W")
            .setCommonName("H128")
            .setDescription(std::move(description))
            .setUpdatedOn(std::move(updatedOn))
            .setDataSource(std::move(source))
            .setCaseInfo(std::move(caseInfo))
            .setMotorType(Motor::Type::RELOAD)
            .setStandardDelays({6})
            .setDiameter(0.029)
            .setLength(0.194)
            .setTimePoints({0.0, 0.5, 1.0})
            .setThrustPoints({0.0, 300.0, 0.0})
            .setCGPoints({Coordinate(0.1, 0, 0, 0.2), Coordinate(0.1, 0, 0, 0.15),
                          Coordinate(0.1, 0, 0, 0.1)})
            .setDigest("test")
            .build();
    if (!motor)
    {
        bug("the test motor does not build: " + motor.error().message);
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

/// The value label of @p panel named @p name.
[[nodiscard]] QLabel& valueLabel(const MotorInformationPanel& panel, std::string_view name)
{
    auto* label = panel.findChild<QLabel*>(QtStrings::fromUtf8(name));
    if (label == nullptr)
    {
        bug("no label " + std::string(name));
    }
    return *label;
}

[[nodiscard]] QPlainTextEdit& comment(const MotorInformationPanel& panel)
{
    return *panel.findChild<QPlainTextEdit*>(QStringLiteral("comment"));
}

/// The names of the value labels, in Java's order.
constexpr std::array<std::string_view, 17> kValueLabels{
    "manufacturerLabel",   "designationLabel",     "commonNameLabel", "totalImpulseLabel",
    "classificationLabel", "avgThrustLabel",       "maxThrustLabel",  "burnTimeLabel",
    "launchMassLabel",     "emptyMassLabel",       "motorTypeLabel",  "caseInfoLabel",
    "propInfoLabel",       "compatibleCasesLabel", "dataPointsLabel", "lastUpdatedOnLabel",
    "sourceLabel"};

/// The value labels that are not empty, "<name>=<text>" each, separated by ' '.
[[nodiscard]] std::string filledLabels(const MotorInformationPanel& panel)
{
    std::string filled;
    for (const std::string_view name : kValueLabels)
    {
        const QLabel& label = valueLabel(panel, name);
        if (!label.text().isEmpty() || !label.toolTip().isEmpty())
        {
            filled += std::format("{}{}={}", filled.empty() ? "" : " ", name,
                                  QtStrings::toUtf8(label.text()));
        }
    }
    return filled;
}

TEST_F(MotorInformationPanelTest, StartsWithoutACurve)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    const MotorInformationPanel      panel(services.services(), &window);
    EXPECT_EQ(panel.objectName(), QStringLiteral("motorInformationPanel"));
    // INFO no motor: every value empty, no tooltip, "No description available." in italic.
    EXPECT_EQ(filledLabels(panel), "");
    EXPECT_EQ(comment(panel).toPlainText(), QStringLiteral("No description available."));
    EXPECT_TRUE(comment(panel).font().italic());
    EXPECT_TRUE(comment(panel).isReadOnly());
    EXPECT_EQ(panel.preview().chart().curveCount(), 0);
    EXPECT_EQ(panel.preview().objectName(), QStringLiteral("layer"));
    EXPECT_EQ(panel.selectedMotor(), nullptr);
    EXPECT_TRUE(panel.curves().empty());
}

TEST_F(MotorInformationPanelTest, ShowsTheSelectedCurveAndThePreviewOfTheList)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    MotorInformationPanel            panel(services.services(), &window);
    const Curves                     curves{makeMotor("one", "", ""), makeMotor("two", "", "")};
    panel.updateData(curves, curves[1]);
    EXPECT_EQ(panel.selectedMotor(), curves[1]);
    EXPECT_EQ(panel.curves(), curves);
    EXPECT_EQ(panel.preview().chart().curveCount(), 2);
    EXPECT_EQ(panel.preview().chart().selectedCurve(), 1);
    EXPECT_EQ(valueLabel(panel, "designationLabel").text(), QStringLiteral("H128W"));
    EXPECT_EQ(valueLabel(panel, "dataPointsLabel").text(), QStringLiteral("2"));
    EXPECT_EQ(valueLabel(panel, "motorTypeLabel").text(), QStringLiteral("Reloadable"));
    EXPECT_EQ(comment(panel).toPlainText(), QStringLiteral("two"));
    EXPECT_FALSE(comment(panel).font().italic());
    // A selection that is none of the curves: Java's indexOf() gives -1.
    panel.updateData(curves, makeMotor("three", "", ""));
    EXPECT_EQ(panel.preview().chart().selectedCurve(), -1);
    EXPECT_EQ(comment(panel).toPlainText(), QStringLiteral("three"));
    // No selection: clearData().
    panel.updateData(curves, nullptr);
    EXPECT_EQ(filledLabels(panel), "");
    EXPECT_EQ(panel.preview().chart().curveCount(), 0);
    EXPECT_TRUE(panel.curves().empty());
}

TEST_F(MotorInformationPanelTest, ANullCurveIsABug)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    MotorInformationPanel            panel(services.services(), &window);
    const Curves                     curves{makeMotor("one", "", ""), nullptr};
    EXPECT_THROW(panel.updateData(curves, curves[0]), BugError);
}

TEST_F(MotorInformationPanelTest, TheDescriptionIsTrimmedAndItalicWithoutOne)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    MotorInformationPanel            panel(services.services(), &window);
    const Curves                     curves{makeMotor("\t A description.\n ", "", "")};
    panel.updateData(curves, curves[0]);
    EXPECT_EQ(comment(panel).toPlainText(), QStringLiteral("A description."));
    EXPECT_FALSE(comment(panel).font().italic());
    EXPECT_EQ(comment(panel).palette().color(QPalette::Text),
              ThemeColors::color(ThemeColors::Key::TEXT, panel.palette()));
    const Curves blank{makeMotor(" \t\n", "", "")};
    panel.updateData(blank, blank[0]);
    EXPECT_EQ(comment(panel).toPlainText(), QStringLiteral("No description available."));
    EXPECT_TRUE(comment(panel).font().italic());
    EXPECT_EQ(comment(panel).palette().color(QPalette::Text),
              ThemeColors::color(ThemeColors::Key::TEXT_DIM, panel.palette()));
}

TEST_F(MotorInformationPanelTest, TheDimTextsFollowThePalette)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    const MotorInformationPanel      panel(services.services(), &window);
    const QLabel&                    classification = valueLabel(panel, "classificationLabel");
    EXPECT_EQ(
        classification.palette().color(QPalette::WindowText),
        ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::theme(panel.palette())));
    {
        const Gui::Test::ScopedApplicationPalette dark(Gui::Test::ScopedApplicationPalette::dark());
        QCoreApplication::sendPostedEvents();
        EXPECT_EQ(classification.palette().color(QPalette::WindowText),
                  ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::DARK));
        EXPECT_EQ(comment(panel).palette().color(QPalette::Text),
                  ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::DARK));
    }
    QCoreApplication::sendPostedEvents();
}

TEST_F(MotorInformationPanelTest, ShowsTheCompatibleCasesJoined)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    MotorInformationPanel            panel(services.services(), &window);
    const Curves curves{makeMotor("", "2019-04-17", "thrustcurve.org", "RMS-29/180")};
    panel.updateData(curves, curves[0]);
    // OpenRocket: "<html>RMS-29/180, RMS-29/240, RMS-29/360<html>".
    EXPECT_EQ(valueLabel(panel, "compatibleCasesLabel").text(),
              QStringLiteral("RMS-29/180, RMS-29/240, RMS-29/360"));
    EXPECT_EQ(valueLabel(panel, "compatibleCasesLabel").textFormat(), Qt::PlainText);
    EXPECT_EQ(valueLabel(panel, "lastUpdatedOnLabel").text(), QStringLiteral("2019-04-17"));
    EXPECT_EQ(valueLabel(panel, "sourceLabel").text(), QStringLiteral("ThrustCurve.org"));
}

TEST_F(MotorInformationPanelTest, ClickingThePreviewOpensThePlotOfItsCurves)
{
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    MotorInformationPanel            panel(services.services(), &window);
    const Curves                     curves{makeMotor("one", "", ""), makeMotor("two", "", "")};
    panel.updateData(curves, curves[1]);
    QTest::mouseClick(&panel.preview(), Qt::LeftButton, {}, QPoint(10, 10));
    auto* dialog = panel.findChild<ThrustCurveMotorPlotDialog*>();
    ASSERT_NE(dialog, nullptr);
    const QPointer<ThrustCurveMotorPlotDialog> shown(dialog);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(dialog->windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(dialog->chart().curveCount(), 2);
    EXPECT_EQ(dialog->chart().selectedCurve(), 1);
    dialog->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(shown.isNull());
}

TEST_F(MotorInformationPanelTest, ClickingThePreviewWithoutACurveOpensNothing)
{
    // Java's listener returns when no motor is selected.
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    const MotorInformationPanel      panel(services.services(), &window);
    QTest::mouseClick(&panel.preview(), Qt::LeftButton, {}, QPoint(10, 10));
    EXPECT_EQ(panel.findChild<QDialog*>(), nullptr);
}

TEST_F(MotorInformationPanelTest, ThePreviewIsAtLeastJavasSizeAndTheDescriptionTakesTheHeight)
{
    // "width 300:300:, height 180:180:" for the preview; the description "grow, pushy".
    const Gui::Test::TestGuiServices services;
    QWidget                          window;
    auto* panel  = makeChild<MotorInformationPanel>(&window, services.services());
    auto* layout = makeChild<QVBoxLayout>(&window);
    layout->addWidget(panel);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    EXPECT_GE(panel->preview().width(), ThrustCurvePreview::kWidth);
    EXPECT_GE(panel->preview().height(), ThrustCurvePreview::kHeight);
    EXPECT_GT(panel->preview().y(), comment(*panel).y() + comment(*panel).height());
    const int     commentHeight = comment(*panel).height();
    const int     previewHeight = panel->preview().height();
    constexpr int kGrowth       = 100;
    window.resize(window.width(), window.height() + kGrowth);
    QCoreApplication::processEvents();
    EXPECT_EQ(comment(*panel).height(), commentHeight + kGrowth);
    EXPECT_EQ(panel->preview().height(), previewHeight);
}

// ------------------------------------------------------------------------------ the formats

/// An UPDATED line of the probe: formatUpdatedOn() of a text, in UTC.
struct UpdatedPin
{
    std::string_view text;
    std::string_view shown;
};

// NOLINTBEGIN(modernize-use-designated-initializers): the probe's lines
constexpr auto kUpdatedPins = std::to_array<UpdatedPin>({
    {"", ""},
    {"   ", ""},
    {"2024-01-02", "2024-01-02"},
    {" 2024-01-02 ", "2024-01-02"},
    {"2024-01-02T03:04:05Z", "2024-01-02"},
    {" 2024-01-02T03:04:05Z\t", "2024-01-02"},
    {"2024-01-02T23:30:00Z", "2024-01-02"},
    {"2024-01-02T03:04Z", "2024-01-02T03:04Z"},
    {"2024-01-02T03:04:05.123456789Z", "2024-01-02"},
    {"2024-01-02T03:04:05.1234567891Z", "2024-01-02T03:04:05.1234567891Z"},
    {"2024-01-02T03:04:05.Z", "2024-01-02"},
    {"2024-01-02T03:04:05,5Z", "2024-01-02T03:04:05,5Z"},
    {"2024-01-02T03:04:05+02:00", "2024-01-02"},
    {"2024-01-02T01:04:05+02:00", "2024-01-01"},
    {"2024-01-02T22:04:05-02:00", "2024-01-03"},
    {"2024-01-02T03:04:05+0200", "2024-01-02T03:04:05+0200"},
    {"2024-01-02T03:04:05+02", "2024-01-02T03:04:05+02"},
    {"2024-01-02T03:04:05+02:00:30", "2024-01-02"},
    {"2024-01-02T03:04:05-00:00", "2024-01-02"},
    {"2024-01-02T03:04:05+18:00", "2024-01-01"},
    {"2024-01-02T03:04:05+19:00", "2024-01-02T03:04:05+19:00"},
    {"2024-01-02t03:04:05z", "2024-01-02"},
    {"2024-01-02T03:04:05z", "2024-01-02"},
    {"2024-01-02T24:00:00Z", "2024-01-03"},
    {"2024-01-02T23:59:60Z", "2024-01-02"},
    {"2016-12-31T23:59:60Z", "2016-12-31"},
    {"2024-01-02T23:60:00Z", "2024-01-02T23:60:00Z"},
    {"2024-02-29T00:00:00Z", "2024-02-29"},
    {"2023-02-29T00:00:00Z", "2023-02-29T00:00:00Z"},
    {"2024-02-30T00:00:00Z", "2024-02-30T00:00:00Z"},
    {"2024-04-31T00:00:00Z", "2024-04-31T00:00:00Z"},
    {"2024-01-02 03:04:05Z", "2024-01-02 03:04:05Z"},
    {"2024-1-2T03:04:05Z", "2024-1-2T03:04:05Z"},
    {"20240102T030405Z", "20240102T030405Z"},
    {"2024-01-02T3:04:05Z", "2024-01-02T3:04:05Z"},
    {"2024-01-02T03:04:05", "2024-01-02T03:04:05"},
    {"+2024-01-02T03:04:05Z", "+2024-01-02T03:04:05Z"},
    {"+12024-01-02T03:04:05Z", "+12024-01-02"},
    {"12024-01-02T03:04:05Z", "12024-01-02T03:04:05Z"},
    {"0000-01-01T00:00:00Z", "0000-01-01"},
    {"-0001-12-31T00:00:00Z", "-0001-12-31"},
    {"0999-01-01T00:00:00Z", "0999-01-01"},
    {"999-01-01T00:00:00Z", "999-01-01T00:00:00Z"},
    {"2024-01-02T03:04:05Z[UTC]", "2024-01-02T03:04:05Z[UTC]"},
    {"2024-01-02T03:04:05.5Z", "2024-01-02"},
    {"2024-01-02T12:00:60Z", "2024-01-02T12:00:60Z"},
    {"2024-01-02T23:58:60Z", "2024-01-02T23:58:60Z"},
    {"2024-01-02T23:59:60.5Z", "2024-01-02"},
    {"2024-01-02T24:00:01Z", "2024-01-02T24:00:01Z"},
    {"2024-01-02T24:01:00Z", "2024-01-02T24:01:00Z"},
    {"2024-01-02T24:00:00.5Z", "2024-01-02T24:00:00.5Z"},
    {"2024-12-31T24:00:00Z", "2025-01-01"},
    {"2024-01-02T25:00:00Z", "2024-01-02T25:00:00Z"},
    {"2024-13-02T03:04:05Z", "2024-13-02T03:04:05Z"},
    {"2024-00-02T03:04:05Z", "2024-00-02T03:04:05Z"},
    {"2024-01-00T03:04:05Z", "2024-01-00T03:04:05Z"},
    {"2024-01-32T03:04:05Z", "2024-01-32T03:04:05Z"},
    {"2024-01-02T03:04:05.000000000Z", "2024-01-02"},
    {"2024-01-02T03:04:05-18:00", "2024-01-02"},
    {"2024-01-02T03:04:05-18:01", "2024-01-02T03:04:05-18:01"},
    {"2024-01-02T03:04:05+2:00", "2024-01-02T03:04:05+2:00"},
    {"2024-01-02T03:04:05+02:0", "2024-01-02T03:04:05+02:0"},
    {"2024-01-02T03:04:05+02:00:3", "2024-01-02T03:04:05+02:00:3"},
    {"2024-01-02T03:04:05+02:00:60", "2024-01-02T03:04:05+02:00:60"},
    {"2024-01-02T03:04:05+02:60", "2024-01-02T03:04:05+02:60"},
    {"2024-01-02T03:04:05+24:00", "2024-01-02T03:04:05+24:00"},
    {"\xEF\xBC\x92"
     "\xEF\xBC\x90"
     "\xEF\xBC\x92"
     "\xEF\xBC\x94"
     "-01-02T03:04:05Z",
     "\xEF\xBC\x92"
     "\xEF\xBC\x90"
     "\xEF\xBC\x92"
     "\xEF\xBC\x94"
     "-01-02T03:04:05Z"},
    {"2024-01-02T03:04:05\xD9\xA0"
     "Z",
     "2024-01-02T03:04:05\xD9\xA0"
     "Z"},
    {"+0999-01-01T00:00:00Z", "+0999-01-01T00:00:00Z"},
    {"+10000-01-01T00:00:00Z", "+10000-01-01"},
    {"-10000-01-01T00:00:00Z", "-10000-01-01"},
    {"-2024-01-02T03:04:05Z", "-2024-01-02"},
    {"+999999999-12-31T23:59:59Z", "+999999999-12-31"},
    // OpenRocket throws DateTimeException (the year is beyond LocalDate's).
    {"+1000000000-12-31T23:59:59Z", "+1000000000-12-31"},
    {"+1000000001-01-01T00:00:00Z", "+1000000001-01-01T00:00:00Z"},
    // OpenRocket throws DateTimeException (the year is beyond LocalDate's).
    {"-1000000000-01-01T00:00:00Z", "-1000000000-01-01"},
    {"-1000000001-01-01T00:00:00Z", "-1000000001-01-01T00:00:00Z"},
    {"2024-01-02T03:04:05Z ", "2024-01-02"},
    {"2024-01-02T03:04:05+00:00", "2024-01-02"},
    {"2024-01-02T03:04:05.123+05:30", "2024-01-01"},
    {"1970-01-01T00:00:00Z", "1970-01-01"},
    {"1969-12-31T23:59:59.999999999Z", "1969-12-31"},
    {"2024-01-02T03:04:05ZZ", "2024-01-02T03:04:05ZZ"},
    {"2024-01-02T03:04:05..5Z", "2024-01-02T03:04:05..5Z"},
    {"2024-01-02T03:04:5Z", "2024-01-02T03:04:5Z"},
    {"-0000-01-01T00:00:00Z", "-0000-01-01T00:00:00Z"},
    {"+00000-01-01T00:00:00Z", "0000-01-01"},
    {"+01000-01-01T00:00:00Z", "1000-01-01"},
    {"2024-01-02T23:59:60+02:00", "2024-01-02"},
    {"2024-01-02T21:59:60Z", "2024-01-02T21:59:60Z"},
    {"2024-01-02T24:00:00.0Z", "2024-01-03"},
    {"2024-01-02T24:00:00.Z", "2024-01-03"},
    {"2024-01-02T03:04:05+18:00:01", "2024-01-02T03:04:05+18:00:01"},
    {"2024-01-02T03:04:05+00:00:00", "2024-01-02"},
    {"2024-01-02T03:04:05-00:00:01", "2024-01-02"},
    {"2024-01-02T03:04:05.Z", "2024-01-02"},
});

/// A ZONE line of the probe: the date of an instant in UTC, -06:00, +05:30, +14:00 and -12:00.
struct ZonePin
{
    std::string_view                text;
    std::array<std::string_view, 5> dates;
};

constexpr auto kZonePins = std::to_array<ZonePin>({
    {"2024-01-02T03:04:05Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {" 2024-01-02T03:04:05Z\t",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T23:30:00Z",
     {"2024-01-02", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T03:04:05.123456789Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05.Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05+02:00",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T01:04:05+02:00",
     {"2024-01-01", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T22:04:05-02:00",
     {"2024-01-03", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T03:04:05+02:00:30",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05-00:00",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05+18:00",
     {"2024-01-01", "2024-01-01", "2024-01-01", "2024-01-01", "2023-12-31"}},
    {"2024-01-02t03:04:05z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T24:00:00Z",
     {"2024-01-03", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T23:59:60Z",
     {"2024-01-02", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2016-12-31T23:59:60Z",
     {"2016-12-31", "2016-12-31", "2017-01-01", "2017-01-01", "2016-12-31"}},
    {"2024-02-29T00:00:00Z",
     {"2024-02-29", "2024-02-28", "2024-02-29", "2024-02-29", "2024-02-28"}},
    {"+12024-01-02T03:04:05Z",
     {"+12024-01-02", "+12024-01-01", "+12024-01-02", "+12024-01-02", "+12024-01-01"}},
    {"0000-01-01T00:00:00Z",
     {"0000-01-01", "-0001-12-31", "0000-01-01", "0000-01-01", "-0001-12-31"}},
    {"-0001-12-31T00:00:00Z",
     {"-0001-12-31", "-0001-12-30", "-0001-12-31", "-0001-12-31", "-0001-12-30"}},
    {"0999-01-01T00:00:00Z",
     {"0999-01-01", "0998-12-31", "0999-01-01", "0999-01-01", "0998-12-31"}},
    {"2024-01-02T03:04:05.5Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T23:59:60.5Z",
     {"2024-01-02", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-12-31T24:00:00Z",
     {"2025-01-01", "2024-12-31", "2025-01-01", "2025-01-01", "2024-12-31"}},
    {"2024-01-02T03:04:05.000000000Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05-18:00",
     {"2024-01-02", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"+10000-01-01T00:00:00Z",
     {"+10000-01-01", "9999-12-31", "+10000-01-01", "+10000-01-01", "9999-12-31"}},
    {"-10000-01-01T00:00:00Z",
     {"-10000-01-01", "-10001-12-31", "-10000-01-01", "-10000-01-01", "-10001-12-31"}},
    {"-2024-01-02T03:04:05Z",
     {"-2024-01-02", "-2024-01-01", "-2024-01-02", "-2024-01-02", "-2024-01-01"}},
    // OpenRocket throws DateTimeException at +05:30 and +14:00 (the year is beyond LocalDate's).
    {"+999999999-12-31T23:59:59Z",
     {"+999999999-12-31", "+999999999-12-31", "+1000000000-01-01", "+1000000000-01-01",
      "+999999999-12-31"}},
    // OpenRocket throws DateTimeException in every zone.
    {"+1000000000-12-31T23:59:59Z",
     {"+1000000000-12-31", "+1000000000-12-31", "+1000000001-01-01", "+1000000001-01-01",
      "+1000000000-12-31"}},
    // OpenRocket throws DateTimeException in every zone.
    {"-1000000000-01-01T00:00:00Z",
     {"-1000000000-01-01", "-1000000001-12-31", "-1000000000-01-01", "-1000000000-01-01",
      "-1000000001-12-31"}},
    {"2024-01-02T03:04:05Z ",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05+00:00",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05.123+05:30",
     {"2024-01-01", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"1970-01-01T00:00:00Z",
     {"1970-01-01", "1969-12-31", "1970-01-01", "1970-01-01", "1969-12-31"}},
    {"1969-12-31T23:59:59.999999999Z",
     {"1969-12-31", "1969-12-31", "1970-01-01", "1970-01-01", "1969-12-31"}},
    {"+00000-01-01T00:00:00Z",
     {"0000-01-01", "-0001-12-31", "0000-01-01", "0000-01-01", "-0001-12-31"}},
    {"+01000-01-01T00:00:00Z",
     {"1000-01-01", "0999-12-31", "1000-01-01", "1000-01-01", "0999-12-31"}},
    {"2024-01-02T23:59:60+02:00",
     {"2024-01-02", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T24:00:00.0Z",
     {"2024-01-03", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T24:00:00.Z",
     {"2024-01-03", "2024-01-02", "2024-01-03", "2024-01-03", "2024-01-02"}},
    {"2024-01-02T03:04:05+00:00:00",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05-00:00:01",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
    {"2024-01-02T03:04:05.Z",
     {"2024-01-02", "2024-01-01", "2024-01-02", "2024-01-02", "2024-01-01"}},
});

/// A SOURCE line of the probe: formatSource() of a text.
struct SourcePin
{
    std::string_view text;
    std::string_view shown;
};

constexpr auto kSourcePins = std::to_array<SourcePin>({
    {"", ""},
    {"  ", ""},
    {"thrustcurve.org", "ThrustCurve.org"},
    {" thrustcurve.org ", "ThrustCurve.org"},
    {"THRUSTCURVE.ORG", "ThrustCurve.org"},
    {"ThrustCurve.org", "ThrustCurve.org"},
    {"thrustcurve.orgx", "thrustcurve.orgx"},
    {"www.thrustcurve.org", "www.thrustcurve.org"},
    {"Manufacturer", "Manufacturer"},
    {" cert ", "cert"},
    {"thrustcurve\xC2\xA0"
     "org",
     "thrustcurve\xC2\xA0"
     "org"},
});
// NOLINTEND(modernize-use-designated-initializers)

/// The pins of kUpdatedPins that formatUpdatedOn() gives otherwise in UTC.
[[nodiscard]] std::string updatedDifferences()
{
    std::string differences;
    for (const UpdatedPin& pin : kUpdatedPins)
    {
        const std::string shown =
            QtStrings::toUtf8(MotorInformationPanel::formatUpdatedOn(pin.text, QTimeZone::utc()));
        if (shown != pin.shown)
        {
            differences += std::format("[{}]: [{}], expected [{}]\n", pin.text, shown, pin.shown);
        }
    }
    return differences;
}

/// The pins of kZonePins that formatUpdatedOn() gives otherwise in their zones.
[[nodiscard]] std::string zoneDifferences()
{
    const std::array<QTimeZone, 5> zones{QTimeZone::utc(), QTimeZone(-6 * 3600),
                                         QTimeZone((5 * 3600) + (30 * 60)), QTimeZone(14 * 3600),
                                         QTimeZone(-12 * 3600)};
    std::string                    differences;
    for (const ZonePin& pin : kZonePins)
    {
        for (std::size_t zone = 0; zone < zones.size(); ++zone)
        {
            const std::string shown =
                QtStrings::toUtf8(MotorInformationPanel::formatUpdatedOn(pin.text, zones.at(zone)));
            if (shown != pin.dates.at(zone))
            {
                differences += std::format("[{}] in zone {}: [{}], expected [{}]\n", pin.text, zone,
                                           shown, pin.dates.at(zone));
            }
        }
    }
    return differences;
}

/// The pins of kSourcePins that formatSource() gives otherwise.
[[nodiscard]] std::string sourceDifferences()
{
    std::string differences;
    for (const SourcePin& pin : kSourcePins)
    {
        const std::string shown = QtStrings::toUtf8(MotorInformationPanel::formatSource(pin.text));
        if (shown != pin.shown)
        {
            differences += std::format("[{}]: [{}], expected [{}]\n", pin.text, shown, pin.shown);
        }
    }
    return differences;
}

TEST_F(MotorInformationPanelTest, FormatsTheUpdateDateAsOpenRocket)
{
    EXPECT_EQ(updatedDifferences(), "");
}

TEST_F(MotorInformationPanelTest, ShowsAnInstantsDateInTheTimeZone)
{
    EXPECT_EQ(zoneDifferences(), "");
}

TEST_F(MotorInformationPanelTest, FormatsTheSourceAsOpenRocket)
{
    EXPECT_EQ(sourceDifferences(), "");
}

}  // namespace

}  // namespace QtRocket::Gui
