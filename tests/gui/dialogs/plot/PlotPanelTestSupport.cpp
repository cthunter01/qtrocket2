#include "PlotPanelTestSupport.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QColor>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>
#include <QString>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/dialogs/plot/FlightEventTableModel.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotTypeSelector.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/ColorChooserButton.h"
#include "QtRocket/gui/widgets/DataTypeComboBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// PlotTabProbe's rgb(): "#rrggbb", "/<alpha>" after it unless opaque, "none" for no colour.
[[nodiscard]] std::string colorText(const QColor& color)
{
    if (!color.isValid())
    {
        return "none";
    }
    std::string text = QtStrings::toUtf8(color.name(QColor::HexRgb));
    if (color.alpha() != Color::kOpaque)
    {
        text += "/" + std::to_string(color.alpha());
    }
    return text;
}

[[nodiscard]] std::string colorText(const std::optional<Color>& color)
{
    return color.has_value()
               ? colorText(QColor(color->red(), color->green(), color->blue(), color->alpha()))
               : std::string("none");
}

}  // namespace

TestFlight::TestFlight(std::shared_ptr<FlightData> data, const std::string& name)
  : m_document(OpenRocketDocumentFactory::createNewRocket()),
    m_simulation(std::make_shared<Simulation>(
        m_document.get(), m_document->getRocket(), Simulation::Status::UPTODATE, name,
        SimulationOptions(), std::vector<std::shared_ptr<SimulationExtension>>{}, std::move(data)))
{
    m_document->addSimulation(m_simulation);
}

TestFlight::~TestFlight() = default;

std::shared_ptr<FlightData> positionFlight()
{
    using Id          = FlightDataTypeId;
    const auto type   = [](Id id) -> const FlightDataType& { return FlightDataType::builtin(id); };
    const auto branch = std::make_shared<FlightDataBranch>(
        "Sustainer", std::vector{&type(Id::TYPE_TIME), &type(Id::TYPE_ALTITUDE),
                                 &type(Id::TYPE_VELOCITY_Z), &type(Id::TYPE_ACCELERATION_Z),
                                 &type(Id::TYPE_POSITION_X), &type(Id::TYPE_POSITION_Y)});
    for (int i = 0; i < 3; ++i)
    {
        branch->addPoint();
        branch->setValue(type(Id::TYPE_TIME), i);
        branch->setValue(type(Id::TYPE_ALTITUDE), i * 100.0);
        branch->setValue(type(Id::TYPE_VELOCITY_Z), i * 20.0);
        branch->setValue(type(Id::TYPE_ACCELERATION_Z), i * 1000.0);
        branch->setValue(type(Id::TYPE_POSITION_X), i * 10.0);
        branch->setValue(type(Id::TYPE_POSITION_Y), i * 10.0);
    }
    return std::make_shared<FlightData>(std::initializer_list{branch});
}

std::string configurationText(const SimulationPlotConfiguration& configuration)
{
    std::string text = "name='" + configuration.getName() +
                       "' X=" + configuration.getDomainAxisType().getName() + " [" +
                       configuration.getDomainAxisUnit().getUnit() +
                       "] rows=" + std::to_string(configuration.getDataCount()) + ":";
    for (std::size_t i = 0; i < configuration.getDataCount(); ++i)
    {
        text += " " + configuration.getType(i).getName() + " [" +
                configuration.getUnit(i).getUnit() + "] @" +
                std::to_string(configuration.getAxis(i)) +
                " color=" + colorText(configuration.getPlotDataColor(i)) + " " +
                std::string(displayName(configuration.getPlotDataLineStyle(i)));
        if (i + 1 < configuration.getDataCount())
        {
            text += ';';
        }
    }
    text += " events=[";
    const std::vector<FlightEvent::Type> events = configuration.getActiveEvents();
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        text += (i == 0 ? "" : ", ") + QtStrings::toUtf8(enumText(events[i]));
    }
    return text + "]";
}

std::string rowText(const SimulationPlotTypeSelector& row)
{
    return std::to_string(row.index()) + " " + row.selectedType()->getName() + " [" +
           row.selectedUnit().getUnit() + "] " +
           QtStrings::toUtf8(row.axisSelector().currentText()) + " " +
           colorText(row.selectedColor()) + " " +
           std::string(displayName(row.selectedLineStyle())) +
           " setDefault=" + (row.setDefaultButton().isEnabled() ? "on" : "off") +
           " reset=" + (row.resetToFactoryButton().isEnabled() ? "on" : "off");
}

std::string rowsText(const SimulationPlotPanel& panel)
{
    std::string text;
    for (std::size_t i = 0; i < panel.selectorCount(); ++i)
    {
        text += (i == 0 ? "" : " | ") + rowText(panel.plotTypeSelector(i));
    }
    return text;
}

std::string eventsText(const FlightEventTableModel& model)
{
    std::string text = std::to_string(model.rowCount()) + ":";
    for (int row = 0; row < model.rowCount(); ++row)
    {
        text += " " +
                QtStrings::toUtf8(
                    model.index(row, FlightEventTableModel::kNameColumn).data().toString()) +
                "=" + (model.isChecked(row) ? "true" : "false") +
                (row + 1 < model.rowCount() ? ";" : "");
    }
    return text;
}

std::string stateText(const SimulationPlotPanel& panel)
{
    const FlightDataType* domain = panel.domainTypeSelector().selectedType();
    return "preset box=" + std::to_string(panel.configurationSelector().currentIndex()) + " '" +
           QtStrings::toUtf8(panel.configurationSelector().currentText()) + "' X box='" +
           (domain == nullptr ? std::string("none") : domain->getName()) +
           "' X unit=" + panel.domainUnitSelector().getSelectedUnit().getUnit();
}

std::string extrasText(const SimulationPlotPanel& panel)
{
    const auto toolTip = [](const QWidget& widget) {
        return widget.toolTip().isEmpty() ? std::string("none")
                                          : "'" + QtStrings::toUtf8(widget.toolTip()) + "'";
    };
    const auto flag  = [](bool value) { return std::string(value ? "true" : "false"); };
    const auto radio = [&](const QRadioButton& button) {
        return " " + QtStrings::toUtf8(button.text()) + " enabled=" + flag(button.isEnabled()) +
               " selected=" + flag(button.isChecked()) + " tooltip=" + toolTip(button);
    };
    return "description visible=" + flag(!panel.descriptionArea().isHidden()) +
           " marker style enabled=" + flag(panel.markerStyleLabel().isEnabled()) +
           " tooltip=" + toolTip(panel.markerStyleLabel()) + radio(panel.verticalMarkerButton()) +
           radio(panel.iconMarkerButton());
}

}  // namespace QtRocket::Gui::Test
