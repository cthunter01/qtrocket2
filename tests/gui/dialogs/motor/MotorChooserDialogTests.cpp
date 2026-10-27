// The motor chooser's dialog (Java: MotorChooserDialog, with CloseableDialog and MotorSelector).
// OpenRocket has no JUnit test of it; these tests follow MotorChooserDialog.java and its call site
// (MotorConfigurationPanel.selectMotor()) over smallMotorDatabase(): the question as
// UserPrompts::chooseMotor() asks it (MotorChooserDialog::choose(), the dialog answered from its
// own event loop by NextModalDialog), its OK, Cancel, Escape and close button, the chooser a window
// keeps between questions, and QtRocket's fixes (the close button after an earlier OK, bug 7; a
// configured motor the mount's limits hide). The chooser of every motor mount of the examples is
// pinned against OpenRocket in motor_chooser_mounts_golden_tests.cpp.

#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <QAbstractItemView>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QPushButton>
#include <QRect>
#include <QString>
#include <QTableView>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "MotorChooserTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"
#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "TestGuiServices.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using MotorChooserDialogTest = Gui::Test::GuiTest;
using Gui::Test::BodyTubeMount;
using Gui::Test::NextModalDialog;
using MotorChoice = UserPrompts::MotorChoice;

/// The chooser of @p dialog, the modal widget NextModalDialog hands over.
[[nodiscard]] MotorChooserDialog& chooserOf(QWidget& dialog)
{
    auto* chooser = qobject_cast<MotorChooserDialog*>(&dialog);
    if (chooser == nullptr)
    {
        bug("the modal dialog is no motor chooser: " + QtStrings::toUtf8(dialog.objectName()));
    }
    return *chooser;
}

/// @p chooser's child of the type T named @p name.
template <class T>
[[nodiscard]] T& childOf(const MotorChooserDialog& chooser, std::string_view name)
{
    // The name as a std::string_view: GCC's -Wdangling-reference takes a reference returned by a
    // function given a temporary QString for one into that temporary.
    T* found = chooser.findChild<T*>(QtStrings::fromUtf8(name));
    if (found == nullptr)
    {
        bug("the motor chooser has no child " + std::string(name));
    }
    return *found;
}

[[nodiscard]] QPushButton& okButton(const MotorChooserDialog& chooser)
{
    return childOf<QPushButton>(chooser, "okButton");
}

[[nodiscard]] QPushButton& cancelButton(const MotorChooserDialog& chooser)
{
    return childOf<QPushButton>(chooser, "cancelButton");
}

[[nodiscard]] QLineEdit& searchField(const MotorChooserDialog& chooser)
{
    return childOf<QLineEdit>(chooser, "searchField");
}

[[nodiscard]] QComboBox& delayBox(const MotorChooserDialog& chooser)
{
    return childOf<QComboBox>(chooser, "delayBox");
}

[[nodiscard]] QTableView& tableOf(const MotorChooserDialog& chooser)
{
    return childOf<QTableView>(chooser, "table");
}

/// The set of the table's selected row, or null.
[[nodiscard]] const ThrustCurveMotorSet* tableSet(const MotorChooserDialog& chooser)
{
    const QModelIndexList rows = tableOf(chooser).selectionModel()->selectedRows();
    return rows.isEmpty() ? nullptr
                          : &chooser.selectionPanel().tableModel().motorSet(rows.front().row());
}

/// Whether @p motor is a curve of @p set.
[[nodiscard]] bool holds(const ThrustCurveMotorSet&                     set,
                         const std::shared_ptr<const ThrustCurveMotor>& motor)
{
    return motor != nullptr && std::ranges::find(set.getMotors(), motor) != set.getMotors().end();
}

/// The answers of the dialog's actions.
void clickOk(QWidget& dialog)
{
    QTest::mouseClick(&okButton(chooserOf(dialog)), Qt::LeftButton);
}

void clickCancel(QWidget& dialog)
{
    QTest::mouseClick(&cancelButton(chooserOf(dialog)), Qt::LeftButton);
}

void pressEscape(QWidget& dialog)
{
    QTest::keyClick(&searchField(chooserOf(dialog)), Qt::Key_Escape);
}

void closeWindow(QWidget& dialog)
{
    dialog.close();
}

/// A design window with the chooser's services: smallMotorDatabase() and fresh preferences.
class ChooserWindow
{
public:
    ChooserWindow() : m_child(makeChild<QWidget>(&m_window))
    {
        m_services.motors() = QtRocket::Test::smallMotorDatabase();
    }

    [[nodiscard]] Gui::Test::TestGuiServices& services() noexcept { return m_services; }
    [[nodiscard]] QWidget&                    window() noexcept { return m_window; }
    /// A widget of the window.
    [[nodiscard]] QWidget& child() const noexcept { return *m_child; }

    /// The set of the database named so.
    [[nodiscard]] const ThrustCurveMotorSet& set(std::string_view manufacturer,
                                                 std::string_view designation)
    {
        return Gui::Test::findMotorSet(m_services.motors(), manufacturer, designation);
    }

    /// The curve @p curve of the set named so.
    [[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motor(std::string_view manufacturer,
                                                                std::string_view designation,
                                                                std::size_t      curve = 0)
    {
        return set(manufacturer, designation).getMotors().at(curve);
    }

    /// chooseMotor() of the window for @p mount in @p fcid, the dialog answered by @p action.
    [[nodiscard]] std::optional<MotorChoice> choose(const MotorMount&            mount,
                                                    const FlightConfigurationId& fcid,
                                                    NextModalDialog::Action      action)
    {
        const NextModalDialog answer(std::move(action));
        return MotorChooserDialog::choose(&m_window, m_services.services(), mount, fcid);
    }

    /// The window's choosers.
    [[nodiscard]] QList<MotorChooserDialog*> choosers() const
    {
        return m_window.findChildren<MotorChooserDialog*>(Qt::FindDirectChildrenOnly);
    }

private:
    Gui::Test::TestGuiServices m_services;
    QWidget                    m_window;
    QWidget*                   m_child;
};

/// What the action saw of the chooser before it answered.
struct Seen
{
    std::shared_ptr<const ThrustCurveMotor> motor;
    double                                  delay  = 0.0;
    double                                  nozzle = 0.0;
    const ThrustCurveMotorSet*              row    = nullptr;
    QString                                 search;
    bool                                    visibleAfterReturn = false;
};

/// Records what the chooser shows, then does @p answer.
struct LookThenAnswer
{
    Seen* seen;
    void (*answer)(QWidget&);

    void operator()(QWidget& dialog) const
    {
        const MotorChooserDialog& chooser = chooserOf(dialog);
        seen->motor                       = chooser.selectionPanel().getSelectedMotor();
        seen->delay                       = chooser.selectionPanel().getSelectedDelay();
        seen->nozzle = chooser.selectionPanel().getSelectedNozzleExitDiameter();
        seen->row    = tableSet(chooser);
        seen->search = searchField(chooser).text();
        answer(dialog);
    }
};

/// Types @p text into the search field, then cancels.
struct SearchThenCancel
{
    QString text;

    void operator()(QWidget& dialog) const
    {
        QLineEdit& search = searchField(chooserOf(dialog));
        search.setFocus();
        QTest::keyClicks(&search, text);
        clickCancel(dialog);
    }
};

/// Presses Return in the search field and, with a delay typed, in the delay editor, then OK.
struct ReturnThenOk
{
    Seen* seen;

    void operator()(QWidget& dialog) const
    {
        const MotorChooserDialog& chooser = chooserOf(dialog);
        QTest::keyClick(&searchField(chooser), Qt::Key_Return);
        Gui::Test::typeIntoComboBox(delayBox(chooser), QStringLiteral("7"));
        QTest::keyClick(delayBox(chooser).lineEdit(), Qt::Key_Return);
        seen->visibleAfterReturn = chooser.isVisible();
        seen->delay              = chooser.selectionPanel().getSelectedDelay();
        clickOk(dialog);
    }
};

/// Types a delay without Enter and clicks OK without the focus moving (a macOS button takes none).
struct TypeDelayThenOk
{
    void operator()(QWidget& dialog) const
    {
        const MotorChooserDialog& chooser = chooserOf(dialog);
        Gui::Test::typeIntoComboBox(delayBox(chooser), QStringLiteral("3"));
        okButton(chooser).click();
    }
};

/// Double-clicks the table's row of @p set.
struct DoubleClickRow
{
    const ThrustCurveMotorSet* set;

    void operator()(QWidget& dialog) const
    {
        const MotorChooserDialog& chooser = chooserOf(dialog);
        QTableView&               table   = tableOf(chooser);
        const QModelIndex         index   = chooser.selectionPanel().tableModel().index(
            chooser.selectionPanel().tableModel().rowOf(*set), 0);
        table.scrollTo(index);
        Gui::Test::doubleClickAt(*table.viewport(), table.visualRect(index).center());
    }
};

/// Asks the window's chooser again while it is open, then cancels.
struct AskAgainThenCancel
{
    ChooserWindow*              window;
    const MotorMount*           mount;
    FlightConfigurationId       fcid;
    std::optional<MotorChoice>* nested;

    void operator()(QWidget& dialog) const
    {
        *nested = MotorChooserDialog::choose(&window->window(), window->services().services(),
                                             *mount, fcid);
        clickCancel(dialog);
    }
};

/// Destroys the window (and with it its chooser) while the chooser is open.
struct DestroyWindow
{
    std::unique_ptr<QWidget>* window;

    void operator()(QWidget& /*dialog*/) const { window->reset(); }
};

/// Two inner tubes of one rocket's body tube, each a motor mount, and a flight configuration in
/// which the first has @p first and the second @p second: a 29 mm mount 0.15 m long and an 18 mm
/// mount 0.075 m long.
class TwoMounts
{
public:
    TwoMounts(const std::shared_ptr<const ThrustCurveMotor>& first,
              const std::shared_ptr<const ThrustCurveMotor>& second)
    {
        AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>());
        m_large           = &addMount(body, 0.0145, 0.15);
        m_small           = &addMount(body, 0.009, 0.075);
        m_rocket.createFlightConfiguration(m_fcid);
        configure(*m_large, first);
        configure(*m_small, second);
    }

    [[nodiscard]] InnerTube&                   large() const noexcept { return *m_large; }
    [[nodiscard]] InnerTube&                   small() const noexcept { return *m_small; }
    [[nodiscard]] const FlightConfigurationId& fcid() const noexcept { return m_fcid; }

private:
    [[nodiscard]] static InnerTube& addMount(BodyTube& body, double innerRadius, double length)
    {
        InnerTube& tube = body.addChild(std::make_unique<InnerTube>());
        tube.setOuterRadius(innerRadius + 0.0005);
        tube.setInnerRadius(innerRadius);
        tube.setLength(length);
        tube.setMotorMount(true);
        return tube;
    }

    void configure(InnerTube& mount, const std::shared_ptr<const ThrustCurveMotor>& motor) const
    {
        MotorConfiguration configuration(mount, m_fcid);
        configuration.setMotor(motor);
        configuration.setEjectionDelay(0.0);
        mount.setMotorConfig(configuration, m_fcid);
    }

    Rocket                m_rocket;
    InnerTube*            m_large{nullptr};
    InnerTube*            m_small{nullptr};
    FlightConfigurationId m_fcid;
};

// ------------------------------------------------------------------------------ the dialog

TEST_F(MotorChooserDialogTest, IsTitledHoldsTheSelectionPanelAndHasNoDefaultButton)
{
    ChooserWindow             window;
    const MotorChooserDialog& chooser =
        MotorChooserDialog::forWindow(window.window(), window.services().services());
    EXPECT_EQ(chooser.objectName(), QStringLiteral("motorChooserDialog"));
    EXPECT_EQ(chooser.windowTitle(), QStringLiteral("Select a rocket motor"));
    EXPECT_EQ(chooser.windowModality(), Qt::ApplicationModal);
    EXPECT_EQ(chooser.parentWidget(), &window.window());
    EXPECT_EQ(chooser.selectionPanel().objectName(), QStringLiteral("selectionPanel"));
    EXPECT_EQ(chooser.selectionPanel().parentWidget(), &chooser);
    EXPECT_EQ(okButton(chooser).text(), QStringLiteral("OK"));
    EXPECT_EQ(cancelButton(chooser).text(), QStringLiteral("Cancel"));
    // Return must not close it (OpenRocket's dialog has no default button).
    EXPECT_FALSE(okButton(chooser).autoDefault());
    EXPECT_FALSE(okButton(chooser).isDefault());
    EXPECT_FALSE(cancelButton(chooser).autoDefault());
    EXPECT_FALSE(cancelButton(chooser).isDefault());
    // The search field has the focus first.
    EXPECT_EQ(chooser.focusWidget(), &chooser.selectionPanel().getDefaultFocus());
    EXPECT_EQ(chooser.getSelectedMotor(), nullptr);
    EXPECT_TRUE(chooser.isHidden());
}

TEST_F(MotorChooserDialogTest, OkAnswersTheConfiguredMotorWithItsDelayAndNozzle)
{
    ChooserWindow                                 window;
    BodyTubeMount                                 mount;
    const std::shared_ptr<const ThrustCurveMotor> c6   = window.motor("Estes", "C6", 1);
    const FlightConfigurationId                   fcid = mount.configure(c6, 5.0, 0.004);
    Seen                                          seen;
    const std::optional<MotorChoice>              choice =
        window.choose(mount.mount(), fcid, LookThenAnswer{.seen = &seen, .answer = clickOk});
    // The chooser opened on the configuration's motor, delay and nozzle exit diameter.
    EXPECT_EQ(seen.motor, c6);
    EXPECT_EQ(seen.delay, 5.0);
    EXPECT_EQ(seen.nozzle, 0.004);
    EXPECT_EQ(seen.row, &window.set("Estes", "C6"));
    const MotorChoice chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, c6);
    EXPECT_EQ(chosen.delay, 5.0);
    EXPECT_EQ(chosen.nozzleExitDiameter, 0.004);
    // OK stores the curve as its set's preferred one (selectedMotor()).
    EXPECT_TRUE(
        MotorPreferences::isPreferredThrustCurve(window.services().preferences(), c6->getDigest()));
    EXPECT_FALSE(MotorPreferences::isPreferredThrustCurve(
        window.services().preferences(), window.motor("Estes", "C6", 0)->getDigest()));
}

TEST_F(MotorChooserDialogTest, OkAnswersAConfiguredNonStandardDelay)
{
    // Bug 8 (plan 10.3): OpenRocket opens with the closest standard delay (7) and OK writes it.
    ChooserWindow                                 window;
    BodyTubeMount                                 mount;
    const std::shared_ptr<const ThrustCurveMotor> c6   = window.motor("Estes", "C6");
    const FlightConfigurationId                   fcid = mount.configure(c6, 6.0);
    const std::optional<MotorChoice> choice = window.choose(mount.mount(), fcid, clickOk);
    const MotorChoice                chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.delay, 6.0);
}

TEST_F(MotorChooserDialogTest, CancelEscapeAndTheCloseButtonAnswerNothing)
{
    ChooserWindow               window;
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(window.motor("Estes", "A8"), 3.0);
    EXPECT_EQ(window.choose(mount.mount(), fcid, clickCancel), std::nullopt);
    EXPECT_EQ(window.choose(mount.mount(), fcid, pressEscape), std::nullopt);
    EXPECT_EQ(window.choose(mount.mount(), fcid, closeWindow), std::nullopt);
    // Nothing chosen, nothing stored.
    EXPECT_EQ(window.services().preferences().findNode(
                  QtRocket::PreferenceKeys::kPreferredThrustCurveMotorNode),
              nullptr);
}

TEST_F(MotorChooserDialogTest, TheCloseButtonCancelsAlsoAfterAnEarlierOk)
{
    // Bug 7 (plan 10.3): OpenRocket's dialog is only hidden on closing and keeps the OK of the
    // question before, so its caller takes the motor as chosen.
    ChooserWindow               window;
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(window.motor("Estes", "A8"), 3.0);
    EXPECT_TRUE(window.choose(mount.mount(), fcid, clickOk).has_value());
    EXPECT_EQ(window.choose(mount.mount(), fcid, closeWindow), std::nullopt);
    EXPECT_EQ(window.choose(mount.mount(), fcid, pressEscape), std::nullopt);
    EXPECT_TRUE(window.choose(mount.mount(), fcid, clickOk).has_value());
}

TEST_F(MotorChooserDialogTest, OkWithoutAMotorAnswersNothing)
{
    ChooserWindow               window;
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.addEmptyConfiguration();
    EXPECT_EQ(window.choose(mount.mount(), fcid, clickOk), std::nullopt);
}

TEST_F(MotorChooserDialogTest, ReturnInTheSearchFieldOrTheDelayEditorKeepsItOpen)
{
    ChooserWindow                    window;
    BodyTubeMount                    mount;
    const FlightConfigurationId      fcid = mount.configure(window.motor("Estes", "C6"), 5.0);
    Seen                             seen;
    const std::optional<MotorChoice> choice =
        window.choose(mount.mount(), fcid, ReturnThenOk{.seen = &seen});
    EXPECT_TRUE(seen.visibleAfterReturn);
    EXPECT_EQ(seen.delay, 7.0);
    const MotorChoice chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.delay, 7.0);
}

TEST_F(MotorChooserDialogTest, OkCommitsADelayTypedWithoutEnter)
{
    ChooserWindow                    window;
    BodyTubeMount                    mount;
    const FlightConfigurationId      fcid   = mount.configure(window.motor("Estes", "C6"), 5.0);
    const std::optional<MotorChoice> choice = window.choose(mount.mount(), fcid, TypeDelayThenOk{});
    const MotorChoice                chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.delay, 3.0);
}

TEST_F(MotorChooserDialogTest, ADoubleClickOnTheTableIsOk)
{
    ChooserWindow                    window;
    BodyTubeMount                    mount;
    const FlightConfigurationId      fcid = mount.addEmptyConfiguration();
    const ThrustCurveMotorSet&       e16w = window.set("AeroTech", "E16W");
    const std::optional<MotorChoice> choice =
        window.choose(mount.mount(), fcid, DoubleClickRow{.set = &e16w});
    const MotorChoice chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, e16w.getMotors().front());
}

// ------------------------------------------------------------------------------ the window's
// chooser

TEST_F(MotorChooserDialogTest, TheSearchAndTheTableRowSurviveBetweenQuestions)
{
    ChooserWindow                                 window;
    BodyTubeMount                                 mount;
    const std::shared_ptr<const ThrustCurveMotor> c6     = window.motor("Estes", "C6");
    const FlightConfigurationId                   withC6 = mount.configure(c6, 5.0);
    const FlightConfigurationId                   empty  = mount.addEmptyConfiguration();
    EXPECT_EQ(
        window.choose(mount.mount(), withC6, SearchThenCancel{.text = QStringLiteral("estes")}),
        std::nullopt);
    // Opened for a configuration without a motor, the chooser selects the row the question before
    // left selected (MotorChooserDialog.open()), with the search as it was.
    Seen                             seen;
    const std::optional<MotorChoice> choice =
        window.choose(mount.mount(), empty, LookThenAnswer{.seen = &seen, .answer = clickOk});
    EXPECT_EQ(seen.search, QStringLiteral("estes"));
    EXPECT_EQ(seen.row, &window.set("Estes", "C6"));
    EXPECT_TRUE(holds(window.set("Estes", "C6"), seen.motor));
    const MotorChoice chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, seen.motor);
    EXPECT_EQ(window.choosers().size(), 1);
}

TEST_F(MotorChooserDialogTest, TheConfiguredMotorStaysWhenTheMountsLimitsHideIt)
{
    // QtRocket's fix (ThrustCurveMotorSelectionPanel): with the mount's limits on, the 18 mm
    // mount's AeroTech E15W (24 mm) is not in the table, and the Estes C6 the 29 mm mount's
    // question selected is. OpenRocket's chooser keeps that row selected and selects its C6, which
    // its OK then puts into the 18 mm mount; QtRocket's keeps the E15W.
    ChooserWindow window;
    MotorPreferences::setLimitDiameter(window.services().preferences(), true);
    MotorPreferences::setLimitLength(window.services().preferences(), true);
    const std::shared_ptr<const ThrustCurveMotor> c6   = window.motor("Estes", "C6");
    const std::shared_ptr<const ThrustCurveMotor> e15w = window.motor("AeroTech", "E15W");
    const TwoMounts                               mounts(c6, e15w);
    const std::optional<MotorChoice> first = window.choose(mounts.large(), mounts.fcid(), clickOk);
    const MotorChoice                firstChosen = Gui::Test::chosenMotor(first);
    EXPECT_EQ(firstChosen.motor, c6);
    Seen                             seen;
    const std::optional<MotorChoice> second = window.choose(
        mounts.small(), mounts.fcid(), LookThenAnswer{.seen = &seen, .answer = clickOk});
    EXPECT_EQ(seen.motor, e15w);
    EXPECT_EQ(seen.row, nullptr);
    const MotorChoice secondChosen = Gui::Test::chosenMotor(second);
    EXPECT_EQ(secondChosen.motor, e15w);
}

TEST_F(MotorChooserDialogTest, AWindowKeepsOneChooserForItsServices)
{
    ChooserWindow               window;
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(window.motor("Estes", "A8"), 3.0);
    EXPECT_EQ(window.choose(mount.mount(), fcid, clickCancel), std::nullopt);
    ASSERT_EQ(window.choosers().size(), 1);
    const MotorChooserDialog* const chooser = window.choosers().front();
    // A question from a widget of the window goes to the window's chooser.
    const NextModalDialog            answer(clickOk);
    const std::optional<MotorChoice> choice = MotorChooserDialog::choose(
        &window.child(), window.services().services(), mount.mount(), fcid);
    EXPECT_TRUE(choice.has_value());
    EXPECT_EQ(window.choosers().size(), 1);
    EXPECT_EQ(window.choosers().front(), chooser);
    // Other services get a chooser of their own; another window too.
    const Gui::Test::TestGuiServices other;
    EXPECT_NE(&MotorChooserDialog::forWindow(window.window(), other.services()), chooser);
    EXPECT_EQ(window.choosers().size(), 2);
    ChooserWindow second;
    EXPECT_NE(&MotorChooserDialog::forWindow(second.window(), window.services().services()),
              chooser);
}

TEST_F(MotorChooserDialogTest, AQuestionWithoutAParentGetsAChooserOfItsOwn)
{
    ChooserWindow                    window;
    BodyTubeMount                    mount;
    const FlightConfigurationId      fcid = mount.configure(window.motor("Estes", "A8"), 3.0);
    const NextModalDialog            answer(clickOk);
    const std::optional<MotorChoice> choice =
        MotorChooserDialog::choose(nullptr, window.services().services(), mount.mount(), fcid);
    const MotorChoice chosen = Gui::Test::chosenMotor(choice);
    EXPECT_EQ(chosen.motor, window.motor("Estes", "A8"));
    EXPECT_TRUE(window.choosers().isEmpty());
}

TEST_F(MotorChooserDialogTest, AQuestionWhileTheChooserIsOpenAnswersNothing)
{
    ChooserWindow                    window;
    BodyTubeMount                    mount;
    const FlightConfigurationId      fcid = mount.configure(window.motor("Estes", "A8"), 3.0);
    std::optional<MotorChoice>       nested{MotorChoice{}};
    const std::optional<MotorChoice> choice = window.choose(
        mount.mount(), fcid,
        AskAgainThenCancel{
            .window = &window, .mount = &mount.mount(), .fcid = fcid, .nested = &nested});
    EXPECT_EQ(nested, std::nullopt);
    EXPECT_EQ(choice, std::nullopt);
}

TEST_F(MotorChooserDialogTest, TheQuestionEndsWithItsWindow)
{
    Gui::Test::TestGuiServices services;
    services.motors() = QtRocket::Test::smallMotorDatabase();
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(
        Gui::Test::findMotorSet(services.motors(), "Estes", "A8").getMotors().front(), 3.0);
    auto                  window = std::make_unique<QWidget>();
    const NextModalDialog destroy(DestroyWindow{.window = &window});
    EXPECT_EQ(MotorChooserDialog::choose(window.get(), services.services(), mount.mount(), fcid),
              std::nullopt);
    EXPECT_EQ(window, nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
