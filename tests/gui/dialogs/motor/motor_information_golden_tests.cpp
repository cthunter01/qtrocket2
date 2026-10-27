// The motor chooser's "Show Details" tab (MotorInformationPanel) for every curve of the eleven sets
// of smallMotorDatabase(), compared with what OpenRocket's own panel showed for them: the INFO
// lines of the probe tools/openrocket-probes/motor-selection-panel (its expected.txt), which drove
// OpenRocket's real ThrustCurveMotorSelectionPanel over the bundled database, chose every curve in
// its curve list with "Hide very similar thrust curves" off, and printed every value label, the
// tooltips of the impulse, the MD5 of the description, whether it was italic and the plot's
// series. The rows below are those lines, pinned by hand. As in OpenRocket, the curves are chosen
// through the selection panel (the details follow its curve list).

#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QString>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "QtRocket/gui/dialogs/motor/MotorInformationPanel.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"
#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/plot/ThrustCurvePreview.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Md5.h"
#include "TestGuiServices.h"
#include "motor/TestMotorDatabase.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using MotorInformationGolden = Gui::Test::GuiTest;

/// An INFO line of the probe: the details of a curve.
struct InformationPin
{
    /// The curve's index in the set.
    std::size_t curve;
    /// The set: its manufacturer's display name and its designation, separated by a space.
    std::string_view set;
    std::string_view manufacturer;
    std::string_view designation;
    std::string_view commonName;
    std::string_view totalImpulse;
    std::string_view classification;
    std::string_view totalImpulseTip;
    std::string_view classificationTip;
    std::string_view avgThrust;
    std::string_view maxThrust;
    std::string_view burnTime;
    std::string_view launchMass;
    std::string_view emptyMass;
    std::string_view motorType;
    std::string_view caseInfo;
    std::string_view propInfo;
    /// OpenRocket's text without its "<html>" before and after.
    std::string_view compatibleCases;
    std::string_view dataPoints;
    std::string_view lastUpdatedOn;
    std::string_view source;
    /// The MD5 of the description's text, in UTF-8.
    std::string_view commentMd5;
    /// The item of the curve list chosen.
    int item;
    /// How many curves the plot shows, and which is the selected one (drawn 3 px wide).
    int series;
    int selectedSeries;
    /// With UnitGroup.setDefaultImperialUnits().
    bool imperial;
    bool commentItalic;
};

// NOLINTBEGIN(modernize-use-designated-initializers): the probe's INFO lines, one each
constexpr auto kInformationPins = std::to_array<InformationPin>({
    {0,
     "Klima C6",
     "Klima",
     "C6",
     "C6",
     "9.99 Ns",
     "(100% C)",
     "Class C (5 Ns - 10 Ns)",
     "Class C (5 Ns - 10 Ns)",
     "6.17 N",
     "15 N",
     "1.61 s",
     "20.5 g",
     "10.9 g",
     "Single-use",
     "SU 18.0x70.0",
     "composite",
     "",
     "15",
     "2020-10-17",
     "manual",
     "897581b9c53da4d56601c3f92cd3f5d4",
     0,
     2,
     0,
     false,
     false},
    {1,
     "Klima C6",
     "Klima",
     "C6",
     "C6",
     "7.48 Ns",
     "(50% C)",
     "Class C (5 Ns - 10 Ns)",
     "Class C (5 Ns - 10 Ns)",
     "4 N",
     "11.6 N",
     "1.86 s",
     "20.5 g",
     "10.9 g",
     "Single-use",
     "SU 18.0x70.0",
     "composite",
     "",
     "15",
     "2020-10-17",
     "manual",
     "897581b9c53da4d56601c3f92cd3f5d4",
     1,
     2,
     1,
     false,
     false},
    {0,
     "AeroTech E15W",
     "AeroTech",
     "E15W",
     "E15",
     "39.9 Ns",
     "(99% E)",
     "Class E (20 Ns - 40 Ns)",
     "Class E (20 Ns - 40 Ns)",
     "15.9 N",
     "28.8 N",
     "2.51 s",
     "50.1 g",
     "30 g",
     "Single-use",
     "SU 24.0x70.0",
     "White Lightning",
     "",
     "44",
     "2021-09-08",
     "ThrustCurve.org",
     "1dd301ec9e88e53496f0d99f093446de",
     0,
     3,
     0,
     false,
     false},
    {1,
     "AeroTech E15W",
     "AeroTech",
     "E15W",
     "E15",
     "39.7 Ns",
     "(99% E)",
     "Class E (20 Ns - 40 Ns)",
     "Class E (20 Ns - 40 Ns)",
     "15.7 N",
     "28.8 N",
     "2.53 s",
     "50.1 g",
     "30 g",
     "Single-use",
     "SU 24.0x70.0",
     "White Lightning",
     "",
     "32",
     "2021-09-08",
     "ThrustCurve.org",
     "1dd301ec9e88e53496f0d99f093446de",
     1,
     3,
     1,
     false,
     false},
    {2,
     "AeroTech E15W",
     "AeroTech",
     "E15W",
     "E15",
     "34.7 Ns",
     "(73% E)",
     "Class E (20 Ns - 40 Ns)",
     "Class E (20 Ns - 40 Ns)",
     "22.1 N",
     "34.7 N",
     "1.56 s",
     "50.1 g",
     "30 g",
     "Single-use",
     "SU 24.0x70.0",
     "White Lightning",
     "",
     "31",
     "2021-09-08",
     "ThrustCurve.org",
     "1dd301ec9e88e53496f0d99f093446de",
     2,
     3,
     2,
     false,
     false},
    {0,
     "AeroTech E16W",
     "AeroTech",
     "E16W",
     "E16",
     "37.7 Ns",
     "(88% E)",
     "Class E (20 Ns - 40 Ns)",
     "Class E (20 Ns - 40 Ns)",
     "20.8 N",
     "37.2 N",
     "1.8 s",
     "107 g",
     "88 g",
     "Reloadable",
     "RMS-29/40-120",
     "White Lightning",
     "",
     "23",
     "2019-04-17",
     "ThrustCurve.org",
     "975012ae0154390fa666c2a892d106ab",
     0,
     1,
     0,
     false,
     false},
    {0,
     "AeroTech G12ST",
     "AeroTech",
     "G12ST",
     "G12",
     "146 Ns",
     "(82% G)",
     "Class G (80 Ns - 160 Ns)",
     "Class G (80 Ns - 160 Ns)",
     "11.7 N",
     "31.3 N",
     "12.4 s",
     "155 g",
     "72.7 g",
     "Single-use",
     "SU 29.0x153.0",
     "Super Thunder",
     "",
     "25",
     "2021-01-30",
     "ThrustCurve.org",
     "833cb1544adbf8a7f759a536f28d096a",
     0,
     2,
     0,
     false,
     false},
    {1,
     "AeroTech G12ST",
     "AeroTech",
     "G12ST",
     "G12",
     "142 Ns",
     "(77% G)",
     "Class G (80 Ns - 160 Ns)",
     "Class G (80 Ns - 160 Ns)",
     "10.9 N",
     "33.8 N",
     "13 s",
     "155 g",
     "72.7 g",
     "Single-use",
     "SU 29.0x153.0",
     "Super Thunder",
     "",
     "20",
     "2021-01-30",
     "ThrustCurve.org",
     "833cb1544adbf8a7f759a536f28d096a",
     1,
     2,
     1,
     false,
     false},
    {0,
     "AeroTech J145H_2JET_STD",
     "AeroTech",
     "J145H_2JET_STD",
     "J145",
     "822 Ns",
     "(28% J)",
     "Class J (640 Ns - 1280 Ns)",
     "Class J (640 Ns - 1280 Ns)",
     "166 N",
     "300 N",
     "4.9 s",
     "1797 g",
     "1387 g",
     "Hybrid",
     "RMS-54/1280",
     "",
     "RMS-54/1280, RMS-54/1706, RMS-54/2560",
     "26",
     "2022-06-15",
     "ThrustCurve.org",
     "7b46bcad97c3f884831f095a4733147a",
     0,
     1,
     0,
     false,
     false},
    {0,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7805 Ns",
     "(52% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1897 N",
     "4486 N",
     "4.07 s",
     "6693 g",
     "2742 g",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "73",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     0,
     3,
     0,
     false,
     false},
    {1,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7922 Ns",
     "(55% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1925 N",
     "4486 N",
     "4.07 s",
     "6693 g",
     "2742 g",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "61",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     1,
     3,
     1,
     false,
     false},
    {2,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7366 Ns",
     "(44% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1354 N",
     "2411 N",
     "5.41 s",
     "6693 g",
     "2742 g",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "23",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     2,
     3,
     2,
     false,
     false},
    {0,
     "Cesaroni Technology Inc. 117G69-14A",
     "Cesaroni Technology Inc.",
     "117G69-14A",
     "G69",
     "130 Ns",
     "(63% G)",
     "Class G (80 Ns - 160 Ns)",
     "Class G (80 Ns - 160 Ns)",
     "67 N",
     "85.9 N",
     "1.94 s",
     "204 g",
     "138 g",
     "Reloadable",
     "Pro38-1G",
     "Skidmark",
     "Pro38-1G, Pro38-2G, Pro38-4G",
     "23",
     "2021-06-13",
     "ThrustCurve.org",
     "e5394a587783ab2995da78ce4d57a420",
     0,
     3,
     0,
     false,
     false},
    {1,
     "Cesaroni Technology Inc. 117G69-14A",
     "Cesaroni Technology Inc.",
     "117G69-14A",
     "G69",
     "116 Ns",
     "(45% G)",
     "Class G (80 Ns - 160 Ns)",
     "Class G (80 Ns - 160 Ns)",
     "61.1 N",
     "91.9 N",
     "1.9 s",
     "204 g",
     "138 g",
     "Reloadable",
     "Pro38-1G",
     "Skidmark",
     "Pro38-1G, Pro38-2G, Pro38-4G",
     "19",
     "2021-06-13",
     "ThrustCurve.org",
     "e5394a587783ab2995da78ce4d57a420",
     1,
     3,
     1,
     false,
     false},
    {2,
     "Cesaroni Technology Inc. 117G69-14A",
     "Cesaroni Technology Inc.",
     "117G69-14A",
     "G69",
     "133 Ns",
     "(66% G)",
     "Class G (80 Ns - 160 Ns)",
     "Class G (80 Ns - 160 Ns)",
     "69.2 N",
     "84.5 N",
     "1.92 s",
     "204 g",
     "138 g",
     "Reloadable",
     "Pro38-1G",
     "Skidmark",
     "Pro38-1G, Pro38-2G, Pro38-4G",
     "12",
     "2021-06-13",
     "ThrustCurve.org",
     "e5394a587783ab2995da78ce4d57a420",
     2,
     3,
     2,
     false,
     false},
    {0,
     "Estes 1/2A3",
     "Estes",
     "1/2A3",
     "1/2A3",
     "1.09 Ns",
     "(75% 1/2A)",
     "Class 1/2A (0.625 Ns - 1.25 Ns)",
     "Class 1/2A (0.625 Ns - 1.25 Ns)",
     "3.24 N",
     "7.62 N",
     "0.336 s",
     "6.6 g",
     "4.6 g",
     "Single-use",
     "SU 13.0x45.0",
     "black powder",
     "",
     "27",
     "2019-10-05",
     "ThrustCurve.org",
     "e5fc3e88077421e47855a12f7e28b94b",
     0,
     1,
     0,
     false,
     false},
    {0,
     "Estes A8",
     "Estes",
     "A8",
     "A8",
     "2.15 Ns",
     "(72% A)",
     "Class A (1.25 Ns - 2.5 Ns)",
     "Class A (1.25 Ns - 2.5 Ns)",
     "4.08 N",
     "9.56 N",
     "0.527 s",
     "16.3 g",
     "13.1 g",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "25",
     "2019-10-05",
     "ThrustCurve.org",
     "e7cff670504fadd29375fba5590b0d8c",
     0,
     2,
     0,
     false,
     false},
    {1,
     "Estes A8",
     "Estes",
     "A8",
     "A8",
     "2.32 Ns",
     "(86% A)",
     "Class A (1.25 Ns - 2.5 Ns)",
     "Class A (1.25 Ns - 2.5 Ns)",
     "3.48 N",
     "9.73 N",
     "0.663 s",
     "16.3 g",
     "13 g",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "23",
     "2019-10-05",
     "ThrustCurve.org",
     "e7cff670504fadd29375fba5590b0d8c",
     1,
     2,
     1,
     false,
     false},
    {0,
     "Estes C6",
     "Estes",
     "C6",
     "C6",
     "8.82 Ns",
     "(76% C)",
     "Class C (5 Ns - 10 Ns)",
     "Class C (5 Ns - 10 Ns)",
     "4.81 N",
     "14.1 N",
     "1.83 s",
     "23.1 g",
     "12.3 g",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "24",
     "2019-10-05",
     "ThrustCurve.org",
     "2d9f849b1b78c8adfa4785df1a6c9cc7",
     0,
     2,
     0,
     false,
     false},
    {1,
     "Estes C6",
     "Estes",
     "C6",
     "C6",
     "10 Ns",
     "(1% D)",
     "Class D (10 Ns - 20 Ns)",
     "Class D (10 Ns - 20 Ns)",
     "4.98 N",
     "11.6 N",
     "2.01 s",
     "23.1 g",
     "12.3 g",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "21",
     "2019-10-05",
     "ThrustCurve.org",
     "2d9f849b1b78c8adfa4785df1a6c9cc7",
     1,
     2,
     1,
     false,
     false},
    {0,
     "Quest B6W",
     "Quest",
     "B6W",
     "B6",
     "5.04 Ns",
     "(1% C)",
     "Class C (5 Ns - 10 Ns)",
     "Class C (5 Ns - 10 Ns)",
     "4.43 N",
     "9.3 N",
     "1.14 s",
     "18.3 g",
     "15.5 g",
     "Single-use",
     "SU 18.0x79.0",
     "White Lightning",
     "",
     "16",
     "2021-05-25",
     "ThrustCurve.org",
     "d0eccadd0accf86dd84a3597222b6caa",
     0,
     1,
     0,
     false,
     false},
    {0,
     "Estes C6",
     "Estes",
     "C6",
     "C6",
     "8.82 Ns",
     "(76% C)",
     "Class C (5 Ns - 10 Ns)",
     "Class C (5 Ns - 10 Ns)",
     "4.81 N",
     "14.1 N",
     "1.83 s",
     "0.815 oz",
     "0.434 oz",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "24",
     "2019-10-05",
     "ThrustCurve.org",
     "2d9f849b1b78c8adfa4785df1a6c9cc7",
     0,
     2,
     0,
     true,
     false},
    {1,
     "Estes C6",
     "Estes",
     "C6",
     "C6",
     "10 Ns",
     "(1% D)",
     "Class D (10 Ns - 20 Ns)",
     "Class D (10 Ns - 20 Ns)",
     "4.98 N",
     "11.6 N",
     "2.01 s",
     "0.815 oz",
     "0.434 oz",
     "Single-use",
     "SU 18.0x70.0",
     "black powder",
     "",
     "21",
     "2019-10-05",
     "ThrustCurve.org",
     "2d9f849b1b78c8adfa4785df1a6c9cc7",
     1,
     2,
     1,
     true,
     false},
    {0,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7805 Ns",
     "(52% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1897 N",
     "4486 N",
     "4.07 s",
     "236 oz",
     "96.7 oz",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "73",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     0,
     3,
     0,
     true,
     false},
    {1,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7922 Ns",
     "(55% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1925 N",
     "4486 N",
     "4.07 s",
     "236 oz",
     "96.7 oz",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "61",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     1,
     3,
     1,
     true,
     false},
    {2,
     "AeroTech M1850W",
     "AeroTech",
     "M1850W",
     "M1850",
     "7366 Ns",
     "(44% M)",
     "Class M (5120 Ns - 10240 Ns)",
     "Class M (5120 Ns - 10240 Ns)",
     "1354 N",
     "2411 N",
     "5.41 s",
     "236 oz",
     "96.7 oz",
     "Reloadable",
     "RMS-75/7680",
     "White Lightning",
     "",
     "23",
     "2024-01-23",
     "ThrustCurve.org",
     "18f3a51bacbc39c1aa43c4f1a172f3ca",
     2,
     3,
     2,
     true,
     false},
});
// NOLINTEND(modernize-use-designated-initializers)

/// The value label of @p information named @p name.
[[nodiscard]] std::string labelText(const MotorInformationPanel& information, const char* name)
{
    const auto* label = information.findChild<QLabel*>(QString::fromLatin1(name));
    if (label == nullptr)
    {
        bug(std::string("no label ") + name);
    }
    return QtStrings::toUtf8(label->text());
}

/// The tooltip of the value label of @p information named @p name.
[[nodiscard]] std::string labelTip(const MotorInformationPanel& information, const char* name)
{
    return QtStrings::toUtf8(information.findChild<QLabel*>(QString::fromLatin1(name))->toolTip());
}

/// The details @p information shows, as the pins hold them, one field per line.
[[nodiscard]] std::string shownDetails(const MotorInformationPanel& information)
{
    const auto*       comment = information.findChild<QPlainTextEdit*>(QStringLiteral("comment"));
    const std::string text    = QtStrings::toUtf8(comment->toPlainText());
    const ThrustCurvePlot& chart = information.preview().chart();
    return std::format(
        "manufacturer={}\ndesignation={}\ncommon={}\nimpulse={}\nclass={}\nimpulseTip={}\n"
        "classTip={}\navg={}\nmax={}\nburn={}\nlaunch={}\nempty={}\ntype={}\ncase={}\nprop={}\n"
        "compatible={}\npoints={}\nupdated={}\nsource={}\ncommentMd5={}\nitalic={}\nseries={}\n"
        "selected={}\n",
        labelText(information, "manufacturerLabel"), labelText(information, "designationLabel"),
        labelText(information, "commonNameLabel"), labelText(information, "totalImpulseLabel"),
        labelText(information, "classificationLabel"), labelTip(information, "totalImpulseLabel"),
        labelTip(information, "classificationLabel"), labelText(information, "avgThrustLabel"),
        labelText(information, "maxThrustLabel"), labelText(information, "burnTimeLabel"),
        labelText(information, "launchMassLabel"), labelText(information, "emptyMassLabel"),
        labelText(information, "motorTypeLabel"), labelText(information, "caseInfoLabel"),
        labelText(information, "propInfoLabel"), labelText(information, "compatibleCasesLabel"),
        labelText(information, "dataPointsLabel"), labelText(information, "lastUpdatedOnLabel"),
        labelText(information, "sourceLabel"), toHex(md5(std::as_bytes(std::span(text)))),
        comment->font().italic(), chart.curveCount(), chart.selectedCurve());
}

/// The details @p pin pins, as shownDetails() writes them.
[[nodiscard]] std::string pinnedDetails(const InformationPin& pin)
{
    return std::format(
        "manufacturer={}\ndesignation={}\ncommon={}\nimpulse={}\nclass={}\nimpulseTip={}\n"
        "classTip={}\navg={}\nmax={}\nburn={}\nlaunch={}\nempty={}\ntype={}\ncase={}\nprop={}\n"
        "compatible={}\npoints={}\nupdated={}\nsource={}\ncommentMd5={}\nitalic={}\nseries={}\n"
        "selected={}\n",
        pin.manufacturer, pin.designation, pin.commonName, pin.totalImpulse, pin.classification,
        pin.totalImpulseTip, pin.classificationTip, pin.avgThrust, pin.maxThrust, pin.burnTime,
        pin.launchMass, pin.emptyMass, pin.motorType, pin.caseInfo, pin.propInfo,
        pin.compatibleCases, pin.dataPoints, pin.lastUpdatedOn, pin.source, pin.commentMd5,
        pin.commentItalic, pin.series, pin.selectedSeries);
}

/// The panel over smallMotorDatabase(), which it fills into @p services' motors first, in
/// @p window.
[[nodiscard]] ThrustCurveMotorSelectionPanel* makePanel(Gui::Test::TestGuiServices& services,
                                                        QWidget&                    window)
{
    services.motors() = QtRocket::Test::smallMotorDatabase();
    return makeChild<ThrustCurveMotorSelectionPanel>(&window, services.services());
}

/// The chooser of the probe: smallMotorDatabase(), "Hide very similar thrust curves" off.
class Chooser
{
public:
    Chooser() : m_panel(makePanel(m_services, m_window))
    {
        auto* layout = makeChild<QVBoxLayout>(&m_window);
        layout->addWidget(m_panel);
        m_panel->findChild<QCheckBox*>(QStringLiteral("hideSimilarBox"))->click();
    }

    /// Selects @p pin's set through the table and its item in the curve list: the differences of
    /// the details and the curve selected, empty when there are none.
    [[nodiscard]] std::string differences(const InformationPin& pin)
    {
        const std::size_t          space = pin.set.rfind(' ');
        const ThrustCurveMotorSet& set   = Gui::Test::findMotorSet(
            m_services.motors(), pin.set.substr(0, space), pin.set.substr(space + 1));
        m_panel->findChild<QTableView*>(QStringLiteral("table"))
            ->selectRow(m_panel->tableModel().rowOf(set));
        auto* curves = m_panel->findChild<QComboBox*>(QStringLiteral("curveSelectionBox"));
        // A click on the item in the popup (QComboBox's itemSelected()).
        curves->setCurrentIndex(pin.item);
        Q_EMIT curves->activated(pin.item);
        std::string differences;
        if (m_panel->getSelectedMotor() != set.getMotors().at(pin.curve))
        {
            differences += std::format("{} item {}: not curve {}\n", pin.set, pin.item, pin.curve);
        }
        const std::string shown  = shownDetails(*m_panel->findChild<MotorInformationPanel*>());
        const std::string pinned = pinnedDetails(pin);
        if (shown != pinned)
        {
            differences +=
                std::format("{}{} item {}:\n{}expected\n{}", pin.imperial ? "imperial " : "",
                            pin.set, pin.item, shown, pinned);
        }
        return differences;
    }

private:
    Gui::Test::TestGuiServices      m_services;
    QWidget                         m_window;
    ThrustCurveMotorSelectionPanel* m_panel{nullptr};
};

/// The differences of the pins of the units @p imperial.
[[nodiscard]] std::string informationDifferences(bool imperial)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    if (imperial)
    {
        UnitGroup::setDefaultImperialUnits();
    }
    Chooser     chooser;
    std::string differences;
    for (const InformationPin& pin : kInformationPins)
    {
        if (pin.imperial == imperial)
        {
            differences += chooser.differences(pin);
        }
    }
    return differences;
}

TEST_F(MotorInformationGolden, ShowsEveryCurveOfTheSmallDatabaseAsOpenRocket)
{
    EXPECT_EQ(informationDifferences(false), "");
}

TEST_F(MotorInformationGolden, ShowsTheValuesInTheDefaultImperialUnitsAsOpenRocket)
{
    EXPECT_EQ(informationDifferences(true), "");
}

}  // namespace

}  // namespace QtRocket::Gui
