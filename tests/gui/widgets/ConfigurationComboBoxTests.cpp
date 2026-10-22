#include "QtRocket/gui/widgets/ConfigurationComboBox.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QPoint>
#include <QPointF>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QVariant>
#include <QWheelEvent>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "RibbonTestSupport.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using ConfigurationComboBoxTest = Test::GuiTest;
using Test::ChangeEventTypes;
using Test::pickComboItem;

/// The names of @p rocket's configurations as the combo box lists them: the default first.
[[nodiscard]] QStringList configurationNames(Rocket& rocket, const Preferences& preferences)
{
    QStringList names;
    for (int i = 0; i <= rocket.getConfigurationCount(); ++i)
    {
        names.append(QtStrings::fromUtf8(
            rocket.getFlightConfigurationByIndex(i, true).getName(preferences)));
    }
    return names;
}

/// The texts of @p combo's items.
[[nodiscard]] QStringList itemTexts(const QComboBox& combo)
{
    QStringList texts;
    for (int i = 0; i < combo.count(); ++i)
    {
        texts.append(combo.itemText(i));
    }
    return texts;
}

/// The id of the configuration at @p index of @p rocket, the default first.
[[nodiscard]] FlightConfigurationId idAt(Rocket& rocket, int index)
{
    return rocket.getFlightConfigurationByIndex(index, true).getId();
}

/// The id a configurationActivated() signal recorded by @p spy carried, at @p index.
[[nodiscard]] FlightConfigurationId activatedId(const QSignalSpy& spy, qsizetype index)
{
    return spy.at(index).at(0).value<FlightConfigurationId>();
}

TEST_F(ConfigurationComboBoxTest, ListsTheDefaultConfigurationFirstThenTheOthers)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    const InMemoryPreferences               preferences;
    const ConfigurationComboBox             combo(rocket, preferences, nullptr);
    EXPECT_TRUE(combo.updatesRocketConfig());
    EXPECT_EQ(combo.sizeAdjustPolicy(), QComboBox::AdjustToContents);
    ASSERT_EQ(combo.count(), 6);
    EXPECT_EQ(combo.itemText(0), QStringLiteral("[No motors]"));
    EXPECT_EQ(itemTexts(combo), configurationNames(rocket, preferences));
    EXPECT_EQ(combo.configurationIdAt(0), FlightConfigurationId::defaultValueId());
    EXPECT_EQ(combo.configurationIdAt(3), idAt(rocket, 3));
    // The default configuration is the selected one.
    EXPECT_EQ(combo.currentIndex(), 0);
    EXPECT_EQ(combo.selectedConfigurationId(), rocket.getSelectedConfiguration().getId());
}

TEST_F(ConfigurationComboBoxTest, APickSelectsTheConfigurationInTheRocket)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    const InMemoryPreferences               preferences;
    ConfigurationComboBox                   combo(rocket, preferences, nullptr);
    const ChangeEventTypes                  events(rocket);
    const QSignalSpy activated(&combo, &ConfigurationComboBox::configurationActivated);

    pickComboItem(combo, 2);
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), idAt(rocket, 2));
    EXPECT_EQ(combo.currentIndex(), 2);
    EXPECT_EQ(events.types(), std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
    EXPECT_TRUE(events.allFromRocket());
    ASSERT_EQ(activated.count(), 1);
    EXPECT_EQ(activatedId(activated, 0), idAt(rocket, 2));

    // The same item again: the rocket fires nothing; the pick is reported all the same (Java's
    // action listeners hear every setSelectedItem()).
    pickComboItem(combo, 2);
    EXPECT_EQ(events.types().size(), 1U);
    EXPECT_EQ(activated.count(), 2);

    // A selection made by the program is shown, but is no pick.
    combo.setSelectedConfiguration(idAt(rocket, 4));
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), idAt(rocket, 4));
    EXPECT_EQ(combo.currentIndex(), 4);
    EXPECT_EQ(activated.count(), 2);
}

TEST_F(ConfigurationComboBoxTest, FollowsTheRocketsConfigurations)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    const InMemoryPreferences               preferences;
    const ConfigurationComboBox             combo(rocket, preferences, nullptr);

    // Selected elsewhere.
    rocket.setSelectedConfiguration(idAt(rocket, 5));
    EXPECT_EQ(combo.currentIndex(), 5);

    // A new configuration, named.
    FlightConfiguration& added = rocket.createFlightConfiguration();
    added.setName("Extra");
    EXPECT_EQ(combo.count(), 7);
    // The name came without an event: shown at the next one.
    EXPECT_NE(combo.itemText(6), QStringLiteral("Extra"));
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
    EXPECT_EQ(combo.itemText(6), QStringLiteral("Extra"));
    EXPECT_EQ(combo.configurationIdAt(6), added.getId());

    // The selected configuration removed: the default is selected.
    rocket.removeFlightConfiguration(idAt(rocket, 5));
    EXPECT_EQ(combo.count(), 6);
    EXPECT_EQ(combo.currentIndex(), 0);
    EXPECT_EQ(itemTexts(combo), configurationNames(rocket, preferences));
}

TEST_F(ConfigurationComboBoxTest, KeepsItsOwnSelectionWhenAskedTo)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    const InMemoryPreferences               preferences;
    ConfigurationComboBox                   combo(rocket, preferences, false, nullptr);
    const ChangeEventTypes                  events(rocket);
    const QSignalSpy activated(&combo, &ConfigurationComboBox::configurationActivated);
    EXPECT_FALSE(combo.updatesRocketConfig());
    EXPECT_EQ(combo.currentIndex(), 0);

    pickComboItem(combo, 1);
    EXPECT_EQ(combo.selectedConfigurationId(), idAt(rocket, 1));
    EXPECT_EQ(combo.currentIndex(), 1);
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), FlightConfigurationId::defaultValueId());
    EXPECT_TRUE(events.types().empty());
    ASSERT_EQ(activated.count(), 1);
    EXPECT_EQ(activatedId(activated, 0), idAt(rocket, 1));

    // The rocket's selection does not move it.
    rocket.setSelectedConfiguration(idAt(rocket, 3));
    EXPECT_EQ(combo.currentIndex(), 1);

    // An id the rocket does not have selects the default configuration (Java's
    // rkt.getFlightConfiguration()), and so does the removal of the one selected.
    combo.setSelectedConfiguration(FlightConfigurationId::fromString("no-such-configuration"));
    EXPECT_EQ(combo.selectedConfigurationId(), FlightConfigurationId::defaultValueId());
    EXPECT_EQ(combo.currentIndex(), 0);
    combo.setSelectedConfiguration(idAt(rocket, 2));
    rocket.removeFlightConfiguration(idAt(rocket, 2));
    EXPECT_EQ(combo.selectedConfigurationId(), FlightConfigurationId::defaultValueId());
    EXPECT_EQ(combo.currentIndex(), 0);
}

TEST_F(ConfigurationComboBoxTest, KeepsWorkingWhenAnUndoReplacesTheConfigurations)
{
    QtRocket::Test::TestEstesAlphaIII         alpha;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    Rocket&                     rocket = document->getRocket();
    const InMemoryPreferences   preferences;
    ConfigurationComboBox const combo(rocket, preferences, nullptr);

    document->addUndoPosition("Add configuration");
    rocket.createFlightConfiguration();
    ASSERT_EQ(combo.count(), 7);
    document->undo();
    EXPECT_EQ(combo.count(), 6);
    EXPECT_EQ(itemTexts(combo), configurationNames(rocket, preferences));

    pickComboItem(combo, 3);
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), idAt(rocket, 3));
}

TEST_F(ConfigurationComboBoxTest, TheWheelGoesOnToTheParent)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    const InMemoryPreferences               preferences;
    ConfigurationComboBox                   combo(rocket, preferences, nullptr);
    const ChangeEventTypes                  events(rocket);

    QWheelEvent wheel(QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, -120), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&combo, &wheel);
    EXPECT_FALSE(wheel.isAccepted());
    EXPECT_EQ(combo.currentIndex(), 0);
    EXPECT_TRUE(events.types().empty());
}

TEST_F(ConfigurationComboBoxTest, OpeningThePopupRefreshesTheNames)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    Rocket&                                 rocket = *alpha.rocket;
    InMemoryPreferences                     preferences;
    ConfigurationComboBox                   combo(rocket, preferences, nullptr);
    combo.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&combo));

    // The default name is a preference, whose change fires no event of the rocket.
    preferences.setDefaultFlightConfigName("Flight {motors}");
    EXPECT_EQ(combo.itemText(0), QStringLiteral("[No motors]"));
    {
        // The offscreen platform cannot raise the popup or grab the keyboard for it.
        const Gui::Test::ExpectedQtMessage offscreen(
            QStringLiteral("^This plugin does not support (raise\\(\\)|grabbing the keyboard)$"));
        combo.showPopup();
        combo.hidePopup();
    }
    EXPECT_EQ(combo.itemText(0), QStringLiteral("Flight No motors"));
    EXPECT_EQ(itemTexts(combo), configurationNames(rocket, preferences));
}

}  // namespace

}  // namespace QtRocket::Gui
