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

/// The configuration of a tube coupler or an engine block (OpenRocket's
/// ThicknessRingComponentConfig, swing gui/configdialog/ThicknessRingComponentConfig.java): the
/// tab "General" ("General properties") of RingComponentConfig with every row: "Length:",
/// "Outer diameter:" with its "Automatic", "Inner diameter:" (no automatic state) and
/// "Wall thickness:", then the placement and the material; then Override and Comment.
class ThicknessRingComponentConfig : public RingComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the tube couplers or engine blocks of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a ring
    ///         component
    ThicknessRingComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                                 std::shared_ptr<ComponentSelection> selection, QWidget* parent);
};

}  // namespace QtRocket::Gui
