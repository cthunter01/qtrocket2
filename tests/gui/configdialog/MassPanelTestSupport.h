#pragma once

// Helpers of the tests of the mass objects' configuration panels (MassComponentConfig,
// ShockCordConfig, ParachuteConfig, StreamerConfig and their shared parts: RadialPositionPanel,
// MassObjectFields, RecoveryDeviceConfig).

#include <memory>

#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
// The components' own headers rather than declarations: a test hands them to functions that take
// a RocketComponent&, which needs the complete class (RingPanelTestSupport.h does the same).
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/Streamer.h"

class QWidget;

namespace QtRocket::Gui::Test
{

/// The design of part D2b's Java probe (D2bProbe.java, run on OpenRocket 5f164fd0e: its output is
/// what the tests pin): a new rocket (OpenRocketDocumentFactory::createNewRocket()) whose stage
/// holds a body tube with, in this order, a mass component, a shock cord, a parachute and a
/// streamer, each made by its constructor (Java's `new X()`), in a document whose undo history
/// starts with the whole design (clearUndo()). The pointers stay valid until an undo or a redo
/// replaces the components.
struct MassDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    AxialStage*                         stage{nullptr};
    BodyTube*                           body{nullptr};
    MassComponent*                      mass{nullptr};
    ShockCord*                          cord{nullptr};
    Parachute*                          parachute{nullptr};
    Streamer*                           streamer{nullptr};

    MassDesign();
};

/// Adds a second stage below the first of @p document's rocket (D2bProbe's "booster") with a body
/// tube, and returns that body tube; the undo history starts again there.
BodyTube& addBoosterStage(OpenRocketDocument& document);

/// The texts of the labels under @p widget, in the order they were made.
[[nodiscard]] QStringList labelTextsUnder(const QWidget& widget);

}  // namespace QtRocket::Gui::Test
