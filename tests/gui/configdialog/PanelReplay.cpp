#include "PanelReplay.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QObject>
#include <QPushButton>
#include <QString>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The count of an edit "int:<n>".
[[nodiscard]] int editCount(std::string_view value)
{
    value.remove_prefix(std::string_view("int:").size());
    bool      parsed = false;
    const int count  = QtStrings::fromUtf8(value).toInt(&parsed);
    QTROCKET_ASSERT(parsed);
    return count;
}

/// Selects the value named @p name (its Java name) in the binding of @p combo when its values are
/// of the enum @p E with the names @p nameOf: nullopt when they are not, else whether the value is
/// one of them (selected, or selected already).
template <class E, class NameOf>
[[nodiscard]] std::optional<bool> selectNamed(const EnumComboBox& combo, std::string_view name,
                                              const NameOf& nameOf)
{
    const std::shared_ptr<EnumBinding<E>> binding = combo.enumBinding<E>();
    if (binding == nullptr)
    {
        return std::nullopt;
    }
    for (const E value : binding->displayedValues())
    {
        if (nameOf(value) == name)
        {
            static_cast<void>(binding->select(value));
            return true;
        }
    }
    return false;
}

/// The edit "select:<NAME>" of @p combo: the enums the probes' panels select from.
[[nodiscard]] bool selectItem(const EnumComboBox& combo, std::string_view value)
{
    value.remove_prefix(std::string_view("select:").size());
    if (const std::optional<bool> selected = selectNamed<FinCrossSection>(
            combo, value, [](FinCrossSection section) { return finCrossSectionName(section); }))
    {
        return *selected;
    }
    if (const std::optional<bool> selected = selectNamed<AxialMethod>(
            combo, value, [](AxialMethod method) { return axialMethodName(method); }))
    {
        return *selected;
    }
    return false;
}

}  // namespace

std::string javaDouble(double value)
{
    return FloatingDecimal::toJavaFormatString(value);
}

bool applyPanelEdit(const RocketComponentConfig& panel, const PanelEdit& edit)
{
    const QString name = QtStrings::fromUtf8(edit.property);
    if (edit.value == "click")
    {
        auto* box = panel.findChild<BoolCheckBox*>(name);
        if (box == nullptr)
        {
            return false;
        }
        box->click();
        return true;
    }
    if (edit.value == "button")
    {
        // The button's objectName: the property with a lower-case first letter and "Button".
        const QString buttonName = name.left(1).toLower() + name.mid(1) + QStringLiteral("Button");
        auto*         button     = panel.findChild<QPushButton*>(buttonName);
        if (button == nullptr)
        {
            return false;
        }
        button->click();
        return true;
    }
    if (edit.value.starts_with("select:"))
    {
        const auto* combo = panel.findChild<EnumComboBox*>(name);
        return combo != nullptr && selectItem(*combo, edit.value);
    }
    if (edit.value.starts_with("int:"))
    {
        const auto* count = panel.findChild<IntSpinBox*>(name);
        if (count == nullptr)
        {
            return false;
        }
        IntBinding& binding = *count->binding();
        return binding.setSpinnerValue(editCount(edit.value));
    }
    const auto* spin = panel.findChild<UnitSpinBox*>(name);
    if (spin == nullptr)
    {
        return false;
    }
    DoubleBinding& binding = *spin->binding();
    return binding.commitText(edit.value);
}

PanelReplay::PanelReplay(OpenRocketDocument& document, const ConfigDialogServices& services,
                         PanelReplayLines lines)
  : m_document(&document), m_services(&services), m_printer(std::move(lines))
{
    for (RocketComponent& component : document.getRocket().subtree(true))
    {
        m_components.push_back(&component);
    }
}

void PanelReplay::panels()
{
    settle();
    for (std::size_t i = 0; i < m_components.size(); ++i)
    {
        RocketComponent& component = *m_components[i];
        if (!m_printer.isPanelKind(component))
        {
            continue;
        }
        settle();
        m_lines.push_back(std::format("== panel | {} | {}", i, m_printer.className(component)));
        std::vector<std::string> section;
        {
            const FactoryPanel<> made(*m_document, *m_services, component);
            m_printer.panel(*made, true, section);
        }
        m_printer.state(component, "", true, section);
        addSection(std::move(section));
    }
}

void PanelReplay::edits(std::size_t index, std::span<const PanelEdit> edits)
{
    settle();
    RocketComponent& component = *m_components.at(index);
    m_lines.push_back(std::format("== panel | {} | {}", index, m_printer.className(component)));
    const FactoryPanel<>     made(*m_document, *m_services, component);
    std::vector<std::string> section;
    m_printer.panel(*made, true, section);
    m_printer.state(component, "", false, section);
    addSection(std::move(section));
    for (const PanelEdit& edit : edits)
    {
        EXPECT_TRUE(applyPanelEdit(*made, edit)) << edit.property << ' ' << edit.value;
        m_lines.push_back(std::format("== edit | {} | {} {}", index, edit.property, edit.value));
        std::vector<std::string> edited;
        m_printer.panel(*made, false, edited);
        m_printer.state(component, "", false, edited);
        addSection(std::move(edited));
    }
}

void PanelReplay::multi(std::size_t main, std::size_t other, std::span<const PanelEdit> edits)
{
    settle();
    RocketComponent& mainComponent  = *m_components.at(main);
    RocketComponent& otherComponent = *m_components.at(other);
    m_lines.push_back(
        std::format("== multi | {} | {} | {}", main, other, m_printer.className(mainComponent)));
    addBothStates(mainComponent, otherComponent);
    const std::array<RocketComponent*, 1> others{&otherComponent};
    const FactoryPanel<>                  made(*m_document, *m_services, mainComponent, others);
    for (const PanelEdit& edit : edits)
    {
        EXPECT_TRUE(applyPanelEdit(*made, edit)) << edit.property << ' ' << edit.value;
        m_lines.push_back(std::format("== multi edit | {} {}", edit.property, edit.value));
        addBothStates(mainComponent, otherComponent);
    }
}

void PanelReplay::settle() const
{
    const Result<int> settled = settleAutomaticDimensions(m_document->getRocket());
    EXPECT_TRUE(settled.has_value()) << settled.error().message;
}

void PanelReplay::addBothStates(const RocketComponent& main, const RocketComponent& other)
{
    std::vector<std::string> section;
    m_printer.state(main, "main ", false, section);
    m_printer.state(other, "other ", false, section);
    addSection(std::move(section));
}

void PanelReplay::addSection(std::vector<std::string> section)
{
    std::ranges::sort(section);
    for (std::string& line : section)
    {
        m_lines.push_back(std::move(line));
    }
}

std::unique_ptr<OpenRocketDocument> pinnedDesignDocument(std::string_view source,
                                                         std::string_view design)
{
    if (source == "example")
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(design);
        if (example == nullptr)
        {
            return nullptr;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().message;
            return nullptr;
        }
        return std::move(loaded->document);
    }
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        if (maker.method == design)
        {
            return OpenRocketDocumentFactory::createDocumentFromRocket(maker.make());
        }
    }
    return nullptr;
}

std::unique_ptr<OpenRocketDocument> onBodyTube(const std::function<void(BodyTube&)>& addChildren)
{
    auto        rocket = std::make_unique<Rocket>();
    AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto        body   = std::make_unique<BodyTube>();
    body->setOuterRadiusAutomatic(false);
    body->setOuterRadius(0.021);
    body->setLength(0.3);
    addChildren(stage.addChild(std::move(body)));
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

std::vector<std::string_view> withDifference(std::span<const std::string_view> pinned,
                                             std::string_view header, std::string_view javaLine,
                                             std::string_view ourLine)
{
    std::vector<std::string_view> lines;
    bool                          after    = false;
    bool                          replaced = false;
    for (const std::string_view line : pinned)
    {
        after = after || line == header;
        if (after && line == javaLine)
        {
            lines.push_back(ourLine);
            replaced = true;
        }
        else
        {
            lines.push_back(line);
        }
    }
    EXPECT_TRUE(replaced) << "the pin has no " << javaLine << " after " << header;
    return lines;
}

}  // namespace QtRocket::Gui::Test
