#pragma once

#include <memory>

#include <QObject>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class BoolCheckBox;
class ComponentSelection;
class GuiServices;
class HtmlLabel;
class MaterialComboBox;
class PlacementPanel;
class StyledLabel;

/// The configuration of a streamer (OpenRocket's StreamerConfig, swing
/// gui/configdialog/StreamerConfig.java), with the tabs General and Radial position before
/// Override and Comment:
///
/// - General ("General properties"), on the left: "Strip length:" (at least 0, slider 0, 60 cm,
///   1.5 m), "Strip width:" (at least 0, slider 0 to 20 cm), "Strip area:" (at least 0, slider 0,
///   0.04, 0.25 m2), "Aspect ratio:" (the length over the width, at least 0, no unit selector:
///   Java comments it out; slider 2 to 15), "Material:" and the combo box of the surface materials
///   ("The component material affects the weight of the component."), "Drag coefficient CD:" (at
///   least 0, no unit selector, no slider) with "Automatic" ("Calculate the streamer drag
///   coefficient (CD) using its dimensions.") at the end of its row, and the italic note "The
///   drag coefficient is relative to the area of the streamer."; on the right the
///   PlacementPanel, whose box also holds "Packed length:", "Packed diameter:" and its
///   "Automatic" (MassObjectFields), and the group "Deployment" (RecoveryDeviceConfig).
/// - Radial position ("Radial position configuration"): RadialPositionPanel.
///
/// Every field reaches every selected streamer (the user's decision U3, ComponentSelection):
/// Java's Streamer setters forward to the others first, and the two that derive the strip from
/// each other (setArea() keeps the aspect ratio, setAspectRatio() keeps the area) derive it, in
/// each forwarded call, from that streamer's own values, so the results are Java's.
class StreamerConfig : public RecoveryDeviceConfig
{
    Q_OBJECT

public:
    /// The configuration of the streamers of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is not a
    ///         streamer
    StreamerConfig(OpenRocketDocument& document, const GuiServices& services,
                   std::shared_ptr<ComponentSelection> selection, QWidget* parent);

    [[nodiscard]] const ConfigFields::NumberWidgets& stripLengthWidgets() const noexcept
    {
        return m_stripLength;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& stripWidthWidgets() const noexcept
    {
        return m_stripWidth;
    }
    [[nodiscard]] const ConfigFields::NumberWidgets& areaWidgets() const noexcept { return m_area; }
    [[nodiscard]] const ConfigFields::NumberWidgets& aspectRatioWidgets() const noexcept
    {
        return m_aspectRatio;
    }
    [[nodiscard]] MaterialComboBox& materialComboBox() const noexcept { return *m_material; }
    [[nodiscard]] HtmlLabel&        cdLabel() const noexcept { return *m_cdLabel; }
    [[nodiscard]] const ConfigFields::NumberWidgets& cdWidgets() const noexcept { return m_cd; }
    [[nodiscard]] BoolCheckBox& cdAutomatic() const noexcept { return *m_cdAutomatic; }
    [[nodiscard]] StyledLabel&  cdNote() const noexcept { return *m_cdNote; }

    [[nodiscard]] PlacementPanel& placement() const noexcept { return *m_placement; }
    [[nodiscard]] const MassObjectFields::SizeWidgets& sizeWidgets() const noexcept
    {
        return m_size;
    }
    [[nodiscard]] RadialPositionPanel& radialPosition() const
    {
        return RadialPositionPanel::of(*this);
    }

    /// The strip length of a streamer (Java: DoubleModel(component, "StripLength")).
    [[nodiscard]] static DoubleSource stripLengthSource();
    /// The strip width of a streamer (Java: DoubleModel(component, "StripWidth")).
    [[nodiscard]] static DoubleSource stripWidthSource();
    /// The strip area of a streamer (Java: DoubleModel(component, "Area")).
    [[nodiscard]] static DoubleSource areaSource();
    /// The aspect ratio of a streamer (Java: DoubleModel(component, "AspectRatio")).
    [[nodiscard]] static DoubleSource aspectRatioSource();

private:
    /// Makes the left side's fields in @p left.
    void addStripFields(QWidget& left);

    ConfigFields::NumberWidgets m_stripLength;
    ConfigFields::NumberWidgets m_stripWidth;
    ConfigFields::NumberWidgets m_area;
    ConfigFields::NumberWidgets m_aspectRatio;
    MaterialComboBox*           m_material{nullptr};
    HtmlLabel*                  m_cdLabel{nullptr};
    ConfigFields::NumberWidgets m_cd;
    BoolCheckBox*               m_cdAutomatic{nullptr};
    StyledLabel*                m_cdNote{nullptr};

    PlacementPanel*               m_placement{nullptr};
    MassObjectFields::SizeWidgets m_size;
};

}  // namespace QtRocket::Gui
