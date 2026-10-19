#pragma once

#include <QGroupBox>
#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"

class QGridLayout;
class QWidget;

namespace QtRocket::Gui
{

class BindingScope;
class IntSpinBox;

/// The instances of a component repeated along the axis (OpenRocket's InstancesPanel, swing
/// gui/configdialog/InstancesPanel.java; launch lugs and rail buttons): the border "Instances",
/// "Instance Count:" (at least 1) and "Instance Separation:" with its unit and a slider from minus
/// to plus the parent's length, or 0.1 m without a parent or with a parent of no length, fixed
/// when the panel is made (as Java).
///
/// The main component of the scope's selection must be a LineInstanceable; the edits reach every
/// selected component that is one.
class InstancesPanel : public QGroupBox
{
    Q_OBJECT

public:
    /// The instances of the components of @p scope's selection.
    /// @throws BugError when the scope has no valid selection or its main component is not a
    ///         LineInstanceable
    InstancesPanel(BindingScope& scope, QWidget* parent);

    [[nodiscard]] QGridLayout& grid() const noexcept { return *m_grid; }
    [[nodiscard]] IntSpinBox&  countSpinBox() const noexcept { return *m_count; }
    [[nodiscard]] const ConfigFields::NumberWidgets& separationWidgets() const noexcept
    {
        return m_separation;
    }

    /// The instance count of a component (Java: IntegerModel(component, "InstanceCount", 1)).
    [[nodiscard]] static IntSource instanceCountSource();
    /// The instance separation of a LineInstanceable (Java: DoubleModel(component,
    /// "InstanceSeparation")).
    [[nodiscard]] static DoubleSource instanceSeparationSource();

private:
    QGridLayout*                m_grid;
    IntSpinBox*                 m_count{nullptr};
    ConfigFields::NumberWidgets m_separation;
};

}  // namespace QtRocket::Gui
