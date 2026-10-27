#pragma once

// The design, presets and favourites of the tests of the preset combo box and its binding
// (PresetBindingTests.cpp, widgets/PresetComboBoxTests.cpp), as the Java probe PresetModelProbe
// (tools/openrocket-gui-pins/preset-chooser) makes them.

#include <cstddef>
#include <memory>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFavorites.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui::Test
{

/// A new design (OpenRocketDocumentFactory::createNewRocket()) whose stage holds the default body
/// tubes "A" and "B", with an empty undo history, as the probe's Design.
struct PresetDesign
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    BodyTube*                           a{nullptr};
    BodyTube*                           b{nullptr};

    PresetDesign()
    {
        RocketComponent& stage = document->getRocket().getChild(0);
        a                      = &stage.addChild(std::make_unique<BodyTube>());
        b                      = &stage.addChild(std::make_unique<BodyTube>());
        a->setName("A");
        b->setName("B");
        document->clearUndo();
    }

    /// The tube @p index of the stage now (an undo replaces every component object).
    [[nodiscard]] const BodyTube& tube(std::size_t index) const
    {
        return dynamic_cast<const BodyTube&>(document->getRocket().getChild(0).getChild(index));
    }
};

/// The presets and favourites of the tests: smallPresetDatabase() and an in-memory store.
struct PresetServices
{
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    ComponentPresetDatabase presets = QtRocket::Test::smallPresetDatabase();
    InMemoryPreferences     preferences;
    std::vector<PresetPtr>  tubes = presets.listForType(ComponentPresetType::BODY_TUBE);

    /// The probe's favourites: the eighth and the third tube.
    void tickFavorites()
    {
        ComponentPresetFavorites::setFavorite(preferences, *tubes.at(7),
                                              ComponentPresetType::BODY_TUBE, true);
        ComponentPresetFavorites::setFavorite(preferences, *tubes.at(2),
                                              ComponentPresetType::BODY_TUBE, true);
    }
};

}  // namespace QtRocket::Gui::Test
