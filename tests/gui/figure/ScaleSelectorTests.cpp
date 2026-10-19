#include "QtRocket/gui/figure/ScaleSelector.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLayout>
#include <QLineEdit>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/figure/ZoomModel.h"
#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

namespace
{

using ScaleSelectorTest = Test::GuiTest;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

/// Port of ScaleSelectorTest.DeferredZoomModel: records the requests without changing the
/// scale it publishes (1, fitted).
class DeferredZoomModel final : public ZoomModel
{
public:
    DeferredZoomModel() : ZoomModel(nullptr) { }

    [[nodiscard]] double scale() override { return 1.0; }
    [[nodiscard]] bool   isFit() const override { return true; }
    void                 setScale(double scale) override { requestedScale = scale; }
    void                 setFit() override { requestedScale = 1.0; }

    double requestedScale = kNaN;
};

/// A model of a scale and a fit that writes down every request (as ScaleProbe's Recording):
/// " setScale(<Java's text of the scale>)" or " setFit()". publish() changes what it shows.
class RecordingZoomModel final : public ZoomModel
{
public:
    RecordingZoomModel(double scale, bool fit) : ZoomModel(nullptr), m_scale(scale), m_fit(fit) { }

    [[nodiscard]] double scale() override { return m_scale; }
    [[nodiscard]] bool   isFit() const override { return m_fit; }
    void                 setScale(double scale) override
    {
        log += " setScale(" + QtStrings::toUtf8(QtStrings::javaDoubleText(scale)) + ")";
    }
    void setFit() override { log += " setFit()"; }

    void publish(double scale, bool fit)
    {
        m_scale = scale;
        m_fit   = fit;
        Q_EMIT changed();
    }

    std::string log;

private:
    double m_scale;
    bool   m_fit;
};

/// Types @p text into the selector's combo box over what it shows, and Enter.
void enterText(const ScaleSelector& selector, const QString& text)
{
    QLineEdit* edit = selector.scaleCombo()->lineEdit();
    ASSERT_NE(edit, nullptr);
    edit->selectAll();
    QTest::keyClick(edit, Qt::Key_Backspace);
    QTest::keyClicks(edit, text);
    QTest::keyClick(edit, Qt::Key_Return);
}

[[nodiscard]] std::string shownText(const ScaleSelector& selector)
{
    return QtStrings::toUtf8(selector.scaleCombo()->currentText());
}

// Port of ScaleSelectorTest.manualEntryRemainsVisibleWhileZoomModelAppliesRequestAsynchronously
// (Java sets the combo box's selected item, which is what an entry does; here the text is typed).
TEST_F(ScaleSelectorTest, ManualEntryRemainsVisibleWhileZoomModelAppliesRequestAsynchronously)
{
    DeferredZoomModel   zoomModel;
    const ScaleSelector selector(&zoomModel, nullptr);
    const QComboBox*    combo = selector.scaleCombo();

    enterText(selector, QStringLiteral("250%"));

    EXPECT_EQ(zoomModel.requestedScale, 2.5);
    EXPECT_EQ(combo->currentText(), QStringLiteral("250%"));
}

TEST_F(ScaleSelectorTest, OffersOpenRocketsLevels)
{
    // ScaleProbe: the combo box's items.
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    const QComboBox*    combo = selector.scaleCombo();
    ASSERT_EQ(combo->count(), 7);
    const std::array<QString, 7> expected{QStringLiteral("15%"),       QStringLiteral("25%"),
                                          QStringLiteral("50%"),       QStringLiteral("75%"),
                                          QStringLiteral("100%(fit)"), QStringLiteral("150%"),
                                          QStringLiteral("200%")};
    for (int index = 0; index < combo->count(); ++index)
    {
        EXPECT_EQ(combo->itemText(index), expected.at(static_cast<std::size_t>(index)));
    }
    EXPECT_TRUE(combo->isEditable());
    EXPECT_EQ(combo->insertPolicy(), QComboBox::NoInsert);
    EXPECT_EQ(ScaleSelector::fitSuffix(), QStringLiteral("(fit)"));
}

/// ScaleProbe: what a new selector shows for a model's scale and fit.
struct InitialText
{
    double      scale;
    bool        fit;
    const char* shown;
};

// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array kInitialTexts{
    InitialText{1.0, true, "100%(fit)"},   InitialText{0.5, false, "50%"},
    InitialText{1.0, false, "100%"},       InitialText{2.5, false, "250%"},
    InitialText{0.123456, false, "12.3%"}, InitialText{0.15, true, "15%(fit)"},
    InitialText{kNaN, false, "NaN"},
};
// NOLINTEND(modernize-use-designated-initializers)

TEST_F(ScaleSelectorTest, ShowsTheModelsScaleAsOpenRocket)
{
    for (const InitialText& initial : kInitialTexts)
    {
        RecordingZoomModel  model(initial.scale, initial.fit);
        const ScaleSelector selector(&model, nullptr);
        EXPECT_EQ(shownText(selector), initial.shown);
    }
}

/// ScaleProbe: formatScaleText(value, false) and (value, true).
struct FormattedScale
{
    double      scale;
    const char* text;
    const char* fitText;
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array kFormattedScales{
    FormattedScale{0.15, "15%", "15%(fit)"},
    FormattedScale{0.25, "25%", "25%(fit)"},
    FormattedScale{0.5, "50%", "50%(fit)"},
    FormattedScale{0.75, "75%", "75%(fit)"},
    FormattedScale{1.0, "100%", "100%(fit)"},
    FormattedScale{1.5, "150%", "150%(fit)"},
    FormattedScale{2.0, "200%", "200%(fit)"},
    FormattedScale{2.5, "250%", "250%(fit)"},
    FormattedScale{0.123456, "12.3%", "12.3%(fit)"},
    FormattedScale{1 / 1.5, "66.7%", "66.7%(fit)"},
    FormattedScale{0.001, "0.1%", "0.1%(fit)"},
    FormattedScale{5.0E-4, "0.1%", "0.1%(fit)"},
    FormattedScale{4.9E-4, "0%", "0%(fit)"},
    FormattedScale{0.0015, "0.1%", "0.1%(fit)"},
    FormattedScale{0.0025, "0.2%", "0.2%(fit)"},
    FormattedScale{1.0005, "100%", "100%(fit)"},
    FormattedScale{1.0015, "100.2%", "100.2%(fit)"},
    FormattedScale{0.995, "99.5%", "99.5%(fit)"},
    FormattedScale{0.9995, "100%", "100%(fit)"},
    FormattedScale{12.3456, "1234.6%", "1234.6%(fit)"},
    FormattedScale{1000000.0, "100000000%", "100000000%(fit)"},
    FormattedScale{1.0E12, "100000000000000%", "100000000000000%(fit)"},
    FormattedScale{-0.5, "-50%", "-50%(fit)"},
    FormattedScale{-0.0, "-0%", "-0%(fit)"},
    FormattedScale{0.0, "0%", "0%(fit)"},
    FormattedScale{kNaN, "NaN", "NaN(fit)"},
    FormattedScale{kInfinity, "∞%", "∞%(fit)"},
    FormattedScale{-kInfinity, "-∞%", "-∞%(fit)"},
    FormattedScale{1.0E307, "∞%", "∞%(fit)"},
    FormattedScale{0.005, "0.5%", "0.5%(fit)"},
    FormattedScale{1.0 / 3, "33.3%", "33.3%(fit)"},
    FormattedScale{2.0 / 3, "66.7%", "66.7%(fit)"},
    FormattedScale{0.0125, "1.2%", "1.2%(fit)"},
    FormattedScale{0.0135, "1.4%", "1.4%(fit)"},
};
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

TEST_F(ScaleSelectorTest, FormatsScalesAsOpenRocket)
{
    for (const FormattedScale& formatted : kFormattedScales)
    {
        EXPECT_EQ(QtStrings::toUtf8(ScaleSelector::formatScaleText(formatted.scale, false)),
                  formatted.text)
            << formatted.scale;
        EXPECT_EQ(QtStrings::toUtf8(ScaleSelector::formatScaleText(formatted.scale, true)),
                  formatted.fitText)
            << formatted.scale;
    }
}

/// ScaleProbe: a text set in the combo box of a selector of a model at 100 % fitted, the
/// requests it made, and what it shows then.
struct EnteredText
{
    const char* typed;
    const char* requests;
    const char* shown;
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array kEnteredTexts{
    EnteredText{"250%", " setScale(2.5)", "250%"},
    EnteredText{"250", " setScale(2.5)", "250%"},
    EnteredText{" 75 % ", " setScale(0.75)", "75%"},
    EnteredText{"100% (fit)", " setFit()", "100%(fit)"},
    EnteredText{"100%", " setScale(1.0)", "100%"},
    EnteredText{"100", " setScale(1.0)", "100%"},
    EnteredText{"100 (fit)", " setFit()", "100%(fit)"},
    EnteredText{"50% (fit)", " setScale(0.5)", "50%"},
    EnteredText{"0.1", " setScale(0.005)", "0.5%"},
    EnteredText{"0.4%", " setScale(0.005)", "0.5%"},
    EnteredText{"0.5%", " setScale(0.005)", "0.5%"},
    EnteredText{"0.6%", " setScale(0.006)", "0.6%"},
    EnteredText{"abc", "", "100%(fit)"},
    EnteredText{"", "", "100%(fit)"},
    EnteredText{"NaN", " setScale(NaN)", "NaN"},
    EnteredText{"Infinity", " setScale(Infinity)", "∞%"},
    EnteredText{"-Infinity", " setScale(0.005)", "0.5%"},
    EnteredText{"-5", " setScale(0.005)", "0.5%"},
    EnteredText{"1e3", " setScale(10.0)", "1000%"},
    EnteredText{"12.345", " setScale(0.12345)", "12.3%"},
    EnteredText{"1,5", "", "100%(fit)"},
    EnteredText{"2.5d", " setScale(0.025)", "2.5%"},
    EnteredText{"0x10p0", " setScale(0.16)", "16%"},
    EnteredText{"100% (fit) ", " setScale(1.0)", "100%"},
    EnteredText{" (fit)", "", "100%(fit)"},
    EnteredText{"100.00001% (fit)", " setFit()", "100%(fit)"},
    EnteredText{"100.01% (fit)", " setFit()", "100%(fit)"},
    EnteredText{"99.995% (fit)", " setFit()", "100%(fit)"},
    EnteredText{"%", "", "100%(fit)"},
    EnteredText{"25%%", " setScale(0.25)", "25%"},
    EnteredText{"1e-400", " setScale(0.005)", "0.5%"},
    EnteredText{"1e400", " setScale(Infinity)", "∞%"},
    EnteredText{"200 %", " setScale(2.0)", "200%"},
    EnteredText{"+50", " setScale(0.5)", "50%"},
    EnteredText{"100.0% (fit)", " setFit()", "100%(fit)"},
};
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

void expectEnteredText(const EnteredText& entered)
{
    SCOPED_TRACE(std::string("typed [") + entered.typed + "]");
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    enterText(selector, QtStrings::fromUtf8(entered.typed));
    EXPECT_EQ(model.log, entered.requests);
    EXPECT_EQ(shownText(selector), entered.shown);
}

TEST_F(ScaleSelectorTest, EnteredTextsAsOpenRocket)
{
    for (const EnteredText& entered : kEnteredTexts)
    {
        expectEnteredText(entered);
    }
}

TEST_F(ScaleSelectorTest, ParsesTheTextsOfAScale)
{
    using Request = ScaleSelector::ScaleRequest;
    EXPECT_EQ(ScaleSelector::parseScaleText(QStringLiteral("250%")), (Request{2.5, false}));
    EXPECT_EQ(ScaleSelector::parseScaleText(QStringLiteral("100%(fit)")), (Request{1.0, true}));
    EXPECT_EQ(ScaleSelector::parseScaleText(QStringLiteral("150%(fit)")), (Request{1.5, false}));
    EXPECT_EQ(ScaleSelector::parseScaleText(QStringLiteral("0")),
              (Request{ScaleSelector::kMinimumScale, false}));
    EXPECT_EQ(ScaleSelector::parseScaleText(QStringLiteral("x")), std::nullopt);
    const std::optional<Request> notANumber = ScaleSelector::parseScaleText(QStringLiteral("NaN"));
    ASSERT_TRUE(notANumber.has_value());
    EXPECT_TRUE(std::isnan(notANumber.value_or(Request{.scale = 0.0, .fit = false}).scale));
}

/// ScaleProbe: getNextLargerScale() (zoom out) and getNextSmallerScale() (zoom in) of a scale.
struct ZoomStep
{
    double scale;
    double zoomOut;
    double zoomIn;
};

// clang-format off
// NOLINTBEGIN(modernize-use-designated-initializers): a table printed by the Java probe
constexpr std::array kZoomSteps{
    ZoomStep{0.001, 6.666666666666666E-4, 0.0015},
    ZoomStep{0.005, 0.0033333333333333335, 0.0075},
    ZoomStep{0.1, 0.06666666666666667, 0.25},
    ZoomStep{0.15, 0.09999999999999999, 0.25},
    ZoomStep{0.2, 0.13333333333333333, 0.30000000000000004},
    ZoomStep{0.2001, 0.15, 0.5},
    ZoomStep{0.25, 0.15, 0.5},
    ZoomStep{0.3, 0.19999999999999998, 0.5},
    ZoomStep{0.5, 0.25, 0.75},
    ZoomStep{0.55, 0.3666666666666667, 0.75},
    ZoomStep{0.7, 0.5, 1.0499999999999998},
    ZoomStep{0.75, 0.5, 1.0},
    // Java's quirk: above 0.75 and outside the bands, zoom out goes to 200 %.
    ZoomStep{0.8, 2.0, 1.0},
    ZoomStep{0.9, 0.75, 1.0},
    ZoomStep{0.95, 0.75, 2.0},
    ZoomStep{0.951, 0.75, 1.5},
    ZoomStep{1.0, 0.75, 1.5},
    ZoomStep{1.04, 0.75, 1.5},
    ZoomStep{1.06, 1.0, 1.5},
    ZoomStep{1.2, 1.0, 1.5},
    ZoomStep{1.5, 1.0, 2.0},
    ZoomStep{1.549, 1.0, 2.0},
    ZoomStep{1.55, 2.0, 2.0},
    ZoomStep{1.6, 1.5, 2.0},
    ZoomStep{1.95, 1.5, 3.0},
    ZoomStep{2.0, 1.5, 3.0},
    ZoomStep{2.05, 2.0, 3.0},
    ZoomStep{2.1, 2.0, 3.0},
    ZoomStep{2.5, 2.0, 3.0},
    ZoomStep{3.0, 2.0, 4.0},
    ZoomStep{3.04, 2.0, 4.0},
    ZoomStep{3.06, 3.0, 4.0},
    ZoomStep{10.0, 9.0, 11.0},
    ZoomStep{0.1000001, 0.06666673333333333, 0.25},
    ZoomStep{0.0999999, 0.0666666, 0.14999985},
    ZoomStep{0.2000001, 0.15, 0.5},
    ZoomStep{0.1999999, 0.13333326666666667, 0.25},
    ZoomStep{1.0499999, 0.75, 1.5},
    ZoomStep{1.0500001, 1.0, 1.5},
    ZoomStep{0.7999999, 0.5, 1.0},
    ZoomStep{0.8000001, 0.75, 1.0},
};
// NOLINTEND(modernize-use-designated-initializers)
// clang-format on

TEST_F(ScaleSelectorTest, ZoomsInAndOutAsOpenRocket)
{
    for (const ZoomStep& step : kZoomSteps)
    {
        EXPECT_EQ(ScaleSelector::nextLargerScale(step.scale), step.zoomOut) << step.scale;
        EXPECT_EQ(ScaleSelector::nextSmallerScale(step.scale), step.zoomIn) << step.scale;
    }
}

TEST_F(ScaleSelectorTest, TheButtonsZoomAndFit)
{
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    QTest::mouseClick(selector.zoomOutButton(), Qt::LeftButton);
    EXPECT_EQ(model.log, " setScale(0.75)");
    EXPECT_EQ(shownText(selector), "75%");
    QTest::mouseClick(selector.zoomInButton(), Qt::LeftButton);
    EXPECT_EQ(model.log, " setScale(0.75) setScale(1.5)");
    EXPECT_EQ(shownText(selector), "150%");
    QTest::mouseClick(selector.zoomFitButton(), Qt::LeftButton);
    EXPECT_EQ(model.log, " setScale(0.75) setScale(1.5) setFit()");
    EXPECT_EQ(shownText(selector), "100%(fit)");
}

TEST_F(ScaleSelectorTest, PickingALevelAsksForIt)
{
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    // The keyboard picks the next level, as the popup list does.
    QTest::keyClick(selector.scaleCombo()->lineEdit(), Qt::Key_Down);
    EXPECT_EQ(model.log, " setScale(1.5)");
    EXPECT_EQ(shownText(selector), "150%");
    QTest::keyClick(selector.scaleCombo()->lineEdit(), Qt::Key_Up);
    EXPECT_EQ(model.log, " setScale(1.5) setFit()");
    EXPECT_EQ(shownText(selector), "100%(fit)");
}

TEST_F(ScaleSelectorTest, FollowsTheModel)
{
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    model.publish(0.42, false);
    EXPECT_EQ(shownText(selector), "42%");
    model.publish(0.25, false);
    EXPECT_EQ(shownText(selector), "25%");
    EXPECT_EQ(selector.scaleCombo()->currentIndex(), 1);
    model.publish(1.0, true);
    EXPECT_EQ(shownText(selector), "100%(fit)");
    EXPECT_TRUE(model.log.empty());
}

TEST_F(ScaleSelectorTest, CompletesNoText)
{
    // Qt would complete "2" to "25%" inline; Swing's combo box takes "2" as typed.
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    enterText(selector, QStringLiteral("2"));
    EXPECT_EQ(model.log, " setScale(0.02)");
    EXPECT_EQ(shownText(selector), "2%");
}

TEST_F(ScaleSelectorTest, DoesNothingOnceTheModelIsGone)
{
    auto                model = std::make_unique<RecordingZoomModel>(1.0, true);
    const ScaleSelector selector(model.get(), nullptr);
    model.reset();
    QTest::mouseClick(selector.zoomOutButton(), Qt::LeftButton);
    QTest::mouseClick(selector.zoomInButton(), Qt::LeftButton);
    QTest::mouseClick(selector.zoomFitButton(), Qt::LeftButton);
    enterText(selector, QStringLiteral("50"));
    EXPECT_EQ(shownText(selector), "50");
}

TEST_F(ScaleSelectorTest, HasTheZoomControlsInARow)
{
    RecordingZoomModel  model(1.0, true);
    const ScaleSelector selector(&model, nullptr);
    EXPECT_EQ(selector.zoomOutButton()->objectName(), QStringLiteral("zoomOutButton"));
    EXPECT_EQ(selector.scaleCombo()->objectName(), QStringLiteral("scaleSelectorCombo"));
    EXPECT_EQ(selector.zoomInButton()->objectName(), QStringLiteral("zoomInButton"));
    EXPECT_EQ(selector.zoomFitButton()->objectName(), QStringLiteral("zoomFitButton"));
    EXPECT_EQ(selector.zoomOutButton()->toolTip(), QStringLiteral("Zoom out"));
    EXPECT_EQ(selector.zoomInButton()->toolTip(), QStringLiteral("Zoom in"));
    EXPECT_EQ(selector.zoomFitButton()->toolTip(), QStringLiteral("Zoom to fit"));
    EXPECT_FALSE(selector.zoomOutButton()->icon().isNull());
    EXPECT_FALSE(selector.zoomInButton()->icon().isNull());
    EXPECT_FALSE(selector.zoomFitButton()->icon().isNull());

    const QLayout* layout = selector.layout();
    ASSERT_NE(layout, nullptr);
    EXPECT_EQ(layout->indexOf(selector.zoomOutButton()), 0);
    EXPECT_LT(layout->indexOf(selector.zoomOutButton()), layout->indexOf(selector.scaleCombo()));
    EXPECT_LT(layout->indexOf(selector.scaleCombo()), layout->indexOf(selector.zoomInButton()));
    EXPECT_LT(layout->indexOf(selector.zoomInButton()), layout->indexOf(selector.zoomFitButton()));
    EXPECT_GE(selector.scaleCombo()->minimumWidth(), 120);
}

}  // namespace

}  // namespace QtRocket::Gui
