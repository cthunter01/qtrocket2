#pragma once
// Helpers of the tests of the assemblies' and stages' configuration panels
// (ComponentAssemblyConfig, AxialStageConfig).

#include <memory>

#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
// The components' own headers rather than declarations: a test hands them to functions that take
// a RocketComponent&, which needs the complete class (RingPanelTestSupport.h does the same).
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Streamer.h"

class QComboBox;

namespace QtRocket::Gui::Test
{

/// The design of the section "assemblies" of part D2c's Java probe (D2cProbe.java, run on
/// OpenRocket 5f164fd0e: its output is what the tests pin): a new rocket
/// (OpenRocketDocumentFactory::createNewRocket()) whose stage holds a body tube with a pod set and
/// a booster set, each made by its constructor (Java's `new X()`), in a document whose undo
/// history starts with the whole design (clearUndo()). The pointers stay valid until an undo or a
/// redo replaces the components.
struct AssemblyDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    AxialStage*                         stage{nullptr};
    BodyTube*                           body{nullptr};
    PodSet*                             pods{nullptr};
    ParallelStage*                      boosters{nullptr};

    AssemblyDesign();
};

/// The design of D2cProbe's section "recovery tab": a new rocket whose stage holds a body tube
/// with a parachute "Main", a booster set (holding a body tube with a parachute "Booster chute")
/// and a streamer "Streamer", in that order; the undo history starts with the whole design.
struct RecoveryDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    AxialStage*                         stage{nullptr};
    BodyTube*                           body{nullptr};
    Parachute*                          main{nullptr};
    ParallelStage*                      boosters{nullptr};
    Parachute*                          boosterChute{nullptr};
    Streamer*                           streamer{nullptr};

    RecoveryDesign();
};

/// The texts of the items of @p combo, in order.
[[nodiscard]] QStringList comboItemTexts(const QComboBox& combo);

}  // namespace QtRocket::Gui::Test
