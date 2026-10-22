#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/configdialog/RingComponentConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class GuiServices;

/// The configuration of a centering ring (OpenRocket's CenteringRingConfig, swing
/// gui/configdialog/CenteringRingConfig.java): the tab "General" ("General properties") of
/// RingComponentConfig with "Thickness:" (the length), "Outer diameter:" and "Inner diameter:",
/// each with its "Automatic", then the placement and the material (no wall thickness); then
/// Override and Comment.
class CenteringRingConfig : public RingComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the centering rings of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a ring
    ///         component
    CenteringRingConfig(OpenRocketDocument& document, const GuiServices& services,
                        std::shared_ptr<ComponentSelection> selection, QWidget* parent);
};

}  // namespace QtRocket::Gui
