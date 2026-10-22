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

/// The configuration of a bulkhead (OpenRocket's BulkheadConfig, swing
/// gui/configdialog/BulkheadConfig.java): the tab "General" ("General properties") of
/// RingComponentConfig with "Thickness:" (the length) and "Diameter:" (the outer diameter, with its
/// "Automatic"), then the placement and the material; then Override and Comment. A bulkhead is
/// solid: it has no inner diameter and no wall thickness.
class BulkheadConfig : public RingComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the bulkheads of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a ring
    ///         component
    BulkheadConfig(OpenRocketDocument& document, const GuiServices& services,
                   std::shared_ptr<ComponentSelection> selection, QWidget* parent);
};

}  // namespace QtRocket::Gui
