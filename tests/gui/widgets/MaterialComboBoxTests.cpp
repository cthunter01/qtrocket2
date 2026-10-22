// The combo box of a material (Java: MaterialComboBox.createComboBox(), a
// GroupableAndSearchableComboBox on a MaterialModel).
//
// Expectations marked "probe" are OpenRocket's own results, printed by the Java probe of this part
// (scratchpad probes/m2r2-B4/java/src/probe/B4Probe.java -> b4-probe.out, run on OpenRocket
// 5f164fd0e with JDK 17): the combo box of a body tube's bulk material over a new rocket's
// document, after B2's probe additions (the user material "Aardvark resin" in the application's
// bulk materials, the document materials "Doc foam", set on the tube, and "Zz custom"). A group
// line is a submenu of the groups popup (its private groupsPopup, read by reflection): its title,
// " [checked]" when Java draws the check mark on it (containsSelectedItem()), and its items, "[x] "
// before a checked one. A search is GroupableAndSearchableComboBox.filter() of the text, the rows
// of its filtered list with Java's check mark before the selected material. The rows are
// generated from the probe's output (scripts/rows.py).

#include "QtRocket/gui/widgets/MaterialComboBox.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <QAction>
#include <QChar>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "WidgetTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Gui
{

namespace
{

using MaterialComboBoxTest = Test::GuiTest;
using Test::OffscreenPopupWarnings;
using Test::RecordingGuardHook;
using Test::showActiveWith;
using Test::turnWheel;

// probe: selected display.
constexpr std::string_view kSelectedDocFoam{"(ud) Doc foam (0.03 g/cm\u00b3)"};

// probe: "Doc foam groups popup".
constexpr std::array<std::string_view, 8> kDocFoamGroups{
    "Metals: Aluminum (2.7 g/cm\u00b3) | Brass (8.6 g/cm\u00b3) | Steel (7.85 g/cm\u00b3) | "
    "Titanium (4.5 g/cm\u00b3)",
    "Woods: Balsa (0.17 g/cm\u00b3) | Basswood (0.5 g/cm\u00b3) | Birch (0.67 g/cm\u00b3) | Cork "
    "(0.24 g/cm\u00b3) | Maple (0.755 g/cm\u00b3) | Pine (0.53 g/cm\u00b3) | Plywood (birch) (0.63 "
    "g/cm\u00b3) | Spruce (0.45 g/cm\u00b3)",
    "Plastics: ABS - 100% infill (1.05 g/cm\u00b3) | ASA - 100% infill (1.05 g/cm\u00b3) | (ud) "
    "Aardvark resin (1.5 g/cm\u00b3) | Acrylic (1.19 g/cm\u00b3) | Delrin (1.42 g/cm\u00b3) | PETG "
    "- 100% infill (1.25 g/cm\u00b3) | PLA - 100% infill (1.25 g/cm\u00b3) | PVC (1.39 g/cm\u00b3) "
    "| Polycarbonate (Lexan) (1.2 g/cm\u00b3) | Polystyrene (1.05 g/cm\u00b3) | Quantum tubing "
    "(1.05 g/cm\u00b3)",
    "Paper Products: Cardboard (0.68 g/cm\u00b3) | Paper (office) (0.82 g/cm\u00b3)",
    "Foams [checked]: Depron (XPS) (0.04 g/cm\u00b3) | [x] (ud) Doc foam (0.03 g/cm\u00b3) | "
    "Styrofoam \"Blue foam\" (XPS) (0.032 g/cm\u00b3) | Styrofoam (generic EPS) (0.02 g/cm\u00b3)",
    "Composites: Blue tube (1.3 g/cm\u00b3) | Carbon fiber (1.78 g/cm\u00b3) | Fiberglass (1.85 "
    "g/cm\u00b3) | Kraft phenolic (0.95 g/cm\u00b3)",
    "Fibers: Nylon (1.15 g/cm\u00b3)",
    "Custom: (ud) Zz custom (0.9 g/cm\u00b3)",
};

/// A search of the probe: the text and the rows of the filtered list.
struct Search
{
    std::string              text;
    std::vector<std::string> rows;
};

// probe: the searches "search '<text>'" with Doc foam selected.
[[nodiscard]] std::vector<Search> docFoamSearches()
{
    return {
        Search{.text = "alu", .rows = {"Aluminum (2.7 g/cm\u00b3)"}},
        Search{.text = "ALU", .rows = {"Aluminum (2.7 g/cm\u00b3)"}},
        Search{.text = "nylon", .rows = {"Nylon (1.15 g/cm\u00b3)"}},
        Search{
            .text = "(ud)",
            .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)",
                     "\u2713 (ud) Doc foam (0.03 g/cm\u00b3)", "(ud) Zz custom (0.9 g/cm\u00b3)"}},
        Search{
            .text = "UD",
            .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)",
                     "\u2713 (ud) Doc foam (0.03 g/cm\u00b3)", "(ud) Zz custom (0.9 g/cm\u00b3)"}},
        Search{.text = "g/cm",
               .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)",
                        "ABS - 100% infill (1.05 g/cm\u00b3)",
                        "Acrylic (1.19 g/cm\u00b3)",
                        "Aluminum (2.7 g/cm\u00b3)",
                        "ASA - 100% infill (1.05 g/cm\u00b3)",
                        "Balsa (0.17 g/cm\u00b3)",
                        "Basswood (0.5 g/cm\u00b3)",
                        "Birch (0.67 g/cm\u00b3)",
                        "Blue tube (1.3 g/cm\u00b3)",
                        "Brass (8.6 g/cm\u00b3)",
                        "Carbon fiber (1.78 g/cm\u00b3)",
                        "Cardboard (0.68 g/cm\u00b3)",
                        "Cork (0.24 g/cm\u00b3)",
                        "Delrin (1.42 g/cm\u00b3)",
                        "Depron (XPS) (0.04 g/cm\u00b3)",
                        "\u2713 (ud) Doc foam (0.03 g/cm\u00b3)",
                        "Fiberglass (1.85 g/cm\u00b3)",
                        "Kraft phenolic (0.95 g/cm\u00b3)",
                        "Maple (0.755 g/cm\u00b3)",
                        "Nylon (1.15 g/cm\u00b3)",
                        "Paper (office) (0.82 g/cm\u00b3)",
                        "PETG - 100% infill (1.25 g/cm\u00b3)",
                        "Pine (0.53 g/cm\u00b3)",
                        "PLA - 100% infill (1.25 g/cm\u00b3)",
                        "Plywood (birch) (0.63 g/cm\u00b3)",
                        "Polycarbonate (Lexan) (1.2 g/cm\u00b3)",
                        "Polystyrene (1.05 g/cm\u00b3)",
                        "PVC (1.39 g/cm\u00b3)",
                        "Quantum tubing (1.05 g/cm\u00b3)",
                        "Spruce (0.45 g/cm\u00b3)",
                        "Steel (7.85 g/cm\u00b3)",
                        "Styrofoam \"Blue foam\" (XPS) (0.032 g/cm\u00b3)",
                        "Styrofoam (generic EPS) (0.02 g/cm\u00b3)",
                        "Titanium (4.5 g/cm\u00b3)",
                        "(ud) Zz custom (0.9 g/cm\u00b3)"}},
        Search{.text = "an",
               .rows = {"Polycarbonate (Lexan) (1.2 g/cm\u00b3)",
                        "Quantum tubing (1.05 g/cm\u00b3)", "Titanium (4.5 g/cm\u00b3)"}},
        Search{.text = "re",
               .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)", "Polystyrene (1.05 g/cm\u00b3)"}},
        Search{.text = "PL",
               .rows = {"Maple (0.755 g/cm\u00b3)", "PLA - 100% infill (1.25 g/cm\u00b3)",
                        "Plywood (birch) (0.63 g/cm\u00b3)"}},
        Search{.text = "zzz", .rows = {}},
        Search{.text = " ",
               .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)",
                        "ABS - 100% infill (1.05 g/cm\u00b3)",
                        "Acrylic (1.19 g/cm\u00b3)",
                        "Aluminum (2.7 g/cm\u00b3)",
                        "ASA - 100% infill (1.05 g/cm\u00b3)",
                        "Balsa (0.17 g/cm\u00b3)",
                        "Basswood (0.5 g/cm\u00b3)",
                        "Birch (0.67 g/cm\u00b3)",
                        "Blue tube (1.3 g/cm\u00b3)",
                        "Brass (8.6 g/cm\u00b3)",
                        "Carbon fiber (1.78 g/cm\u00b3)",
                        "Cardboard (0.68 g/cm\u00b3)",
                        "Cork (0.24 g/cm\u00b3)",
                        "Delrin (1.42 g/cm\u00b3)",
                        "Depron (XPS) (0.04 g/cm\u00b3)",
                        "\u2713 (ud) Doc foam (0.03 g/cm\u00b3)",
                        "Fiberglass (1.85 g/cm\u00b3)",
                        "Kraft phenolic (0.95 g/cm\u00b3)",
                        "Maple (0.755 g/cm\u00b3)",
                        "Nylon (1.15 g/cm\u00b3)",
                        "Paper (office) (0.82 g/cm\u00b3)",
                        "PETG - 100% infill (1.25 g/cm\u00b3)",
                        "Pine (0.53 g/cm\u00b3)",
                        "PLA - 100% infill (1.25 g/cm\u00b3)",
                        "Plywood (birch) (0.63 g/cm\u00b3)",
                        "Polycarbonate (Lexan) (1.2 g/cm\u00b3)",
                        "Polystyrene (1.05 g/cm\u00b3)",
                        "PVC (1.39 g/cm\u00b3)",
                        "Quantum tubing (1.05 g/cm\u00b3)",
                        "Spruce (0.45 g/cm\u00b3)",
                        "Steel (7.85 g/cm\u00b3)",
                        "Styrofoam \"Blue foam\" (XPS) (0.032 g/cm\u00b3)",
                        "Styrofoam (generic EPS) (0.02 g/cm\u00b3)",
                        "Titanium (4.5 g/cm\u00b3)",
                        "(ud) Zz custom (0.9 g/cm\u00b3)"}},
        Search{
            .text = "1.05",
            .rows = {"ABS - 100% infill (1.05 g/cm\u00b3)", "ASA - 100% infill (1.05 g/cm\u00b3)",
                     "Polystyrene (1.05 g/cm\u00b3)", "Quantum tubing (1.05 g/cm\u00b3)"}},
        Search{.text = "cr\u00eape", .rows = {}},
        Search{.text = "\u00b3",
               .rows = {"(ud) Aardvark resin (1.5 g/cm\u00b3)",
                        "ABS - 100% infill (1.05 g/cm\u00b3)",
                        "Acrylic (1.19 g/cm\u00b3)",
                        "Aluminum (2.7 g/cm\u00b3)",
                        "ASA - 100% infill (1.05 g/cm\u00b3)",
                        "Balsa (0.17 g/cm\u00b3)",
                        "Basswood (0.5 g/cm\u00b3)",
                        "Birch (0.67 g/cm\u00b3)",
                        "Blue tube (1.3 g/cm\u00b3)",
                        "Brass (8.6 g/cm\u00b3)",
                        "Carbon fiber (1.78 g/cm\u00b3)",
                        "Cardboard (0.68 g/cm\u00b3)",
                        "Cork (0.24 g/cm\u00b3)",
                        "Delrin (1.42 g/cm\u00b3)",
                        "Depron (XPS) (0.04 g/cm\u00b3)",
                        "\u2713 (ud) Doc foam (0.03 g/cm\u00b3)",
                        "Fiberglass (1.85 g/cm\u00b3)",
                        "Kraft phenolic (0.95 g/cm\u00b3)",
                        "Maple (0.755 g/cm\u00b3)",
                        "Nylon (1.15 g/cm\u00b3)",
                        "Paper (office) (0.82 g/cm\u00b3)",
                        "PETG - 100% infill (1.25 g/cm\u00b3)",
                        "Pine (0.53 g/cm\u00b3)",
                        "PLA - 100% infill (1.25 g/cm\u00b3)",
                        "Plywood (birch) (0.63 g/cm\u00b3)",
                        "Polycarbonate (Lexan) (1.2 g/cm\u00b3)",
                        "Polystyrene (1.05 g/cm\u00b3)",
                        "PVC (1.39 g/cm\u00b3)",
                        "Quantum tubing (1.05 g/cm\u00b3)",
                        "Spruce (0.45 g/cm\u00b3)",
                        "Steel (7.85 g/cm\u00b3)",
                        "Styrofoam \"Blue foam\" (XPS) (0.032 g/cm\u00b3)",
                        "Styrofoam (generic EPS) (0.02 g/cm\u00b3)",
                        "Titanium (4.5 g/cm\u00b3)",
                        "(ud) Zz custom (0.9 g/cm\u00b3)"}},
    };
}

// probe: "search 'nylon'".
constexpr std::string_view kSearchNylon{"Nylon (1.15 g/cm\u00b3)"};

// probe: "Balsa groups popup", its Woods and Foams lines (the others are kDocFoamGroups').
constexpr std::string_view kBalsaWoods{
    "Woods [checked]: [x] Balsa (0.17 g/cm\u00b3) | Basswood (0.5 g/cm\u00b3) | Birch (0.67 "
    "g/cm\u00b3) | Cork (0.24 g/cm\u00b3) | Maple (0.755 g/cm\u00b3) | Pine (0.53 g/cm\u00b3) | "
    "Plywood (birch) (0.63 g/cm\u00b3) | Spruce (0.45 g/cm\u00b3)"};
constexpr std::string_view kBalsaFoams{
    "Foams: Depron (XPS) (0.04 g/cm\u00b3) | (ud) Doc foam (0.03 g/cm\u00b3) | Styrofoam \"Blue "
    "foam\" (XPS) (0.032 g/cm\u00b3) | Styrofoam (generic EPS) (0.02 g/cm\u00b3)"};

// probe: "search 'bal'" with Balsa selected.
constexpr std::string_view kSearchBalsa{"\u2713 Balsa (0.17 g/cm\u00b3)"};

// probe: loose: selected display, and the Woods and Foams lines of the "Loose groups popup".
constexpr std::string_view kSelectedLoose{"(ud) Loose (0.123 g/cm\u00b3)"};
constexpr std::string_view kLooseWoods{
    "Woods [checked]: Balsa (0.17 g/cm\u00b3) | Basswood (0.5 g/cm\u00b3) | Birch (0.67 "
    "g/cm\u00b3) | Cork (0.24 g/cm\u00b3) | Maple (0.755 g/cm\u00b3) | Pine (0.53 g/cm\u00b3) | "
    "Plywood (birch) (0.63 g/cm\u00b3) | Spruce (0.45 g/cm\u00b3)"};
constexpr std::string_view kLooseFoams{
    "Foams: Depron (XPS) (0.04 g/cm\u00b3) | (ud) Doc foam (0.03 g/cm\u00b3) | Styrofoam \"Blue "
    "foam\" (XPS) (0.032 g/cm\u00b3) | Styrofoam (generic EPS) (0.02 g/cm\u00b3)"};

/// The source of an external component's material (Java: "Material").
[[nodiscard]] MaterialSource externalMaterial()
{
    return MaterialSource::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getMaterial(); },
        .set = [](ExternalComponent& component,
                  const Material&    material) { component.setMaterial(material); },
    });
}

/// The probe's design: the application's built-in materials and a new rocket's document with
/// two body tubes in its stage, the probe's additions made.
struct MaterialDesign
{
    MaterialStorage                     application;
    std::unique_ptr<OpenRocketDocument> document;
    BodyTube*                           tube{nullptr};
    BodyTube*                           other{nullptr};

    MaterialDesign();

    /// A binding of the tube's material, with @p others selected too.
    [[nodiscard]] std::shared_ptr<MaterialBinding> bind(
        std::span<RocketComponent* const> others = {});
};

MaterialDesign::MaterialDesign() : document(OpenRocketDocumentFactory::createNewRocket())
{
    addBuiltinMaterials(application);
    RocketComponent& stage = document->getRocket().getChild(0);
    tube                   = &stage.addChild(std::make_unique<BodyTube>());
    other                  = &stage.addChild(std::make_unique<BodyTube>());
    application.addMaterial(Material::newMaterial(Material::Type::BULK, "Aardvark resin", 1500,
                                                  MaterialGroup::PLASTICS, true));
    tube->setMaterial(Material::newMaterial(Material::Type::BULK, "Doc foam", 30,
                                            MaterialGroup::FOAMS, true, true));
    document->getDocumentMaterials().addMaterial(Material::newMaterial(
        Material::Type::BULK, "Zz custom", 900, MaterialGroup::CUSTOM, true, true));
}

std::shared_ptr<MaterialBinding> MaterialDesign::bind(std::span<RocketComponent* const> others)
{
    return std::make_shared<MaterialBinding>(std::make_shared<ComponentSelection>(*tube, others),
                                             externalMaterial(), Material::Type::BULK, application,
                                             *document);
}

/// The probe's line of each group submenu the popup of @p combo shows, in order.
[[nodiscard]] std::vector<std::string> groupLines(const MaterialComboBox& combo)
{
    std::vector<std::string> lines;
    for (const QAction* action : combo.popupMenu().actions())
    {
        const QMenu* menu = action->menu();
        if (menu == nullptr || !action->isVisible())
        {
            continue;
        }
        QString line = action->text();
        if (action->isChecked())
        {
            line += QStringLiteral(" [checked]");
        }
        QString separator = QStringLiteral(": ");
        for (const QAction* item : menu->actions())
        {
            line += separator;
            if (item->isChecked())
            {
                line += QStringLiteral("[x] ");
            }
            line += item->text();
            separator = QStringLiteral(" | ");
        }
        lines.push_back(QtStrings::toUtf8(line));
    }
    return lines;
}

/// The rows of the search list of @p combo.
[[nodiscard]] std::vector<std::string> searchRows(const MaterialComboBox& combo)
{
    std::vector<std::string> rows;
    const QListWidget&       list = combo.searchList();
    rows.reserve(static_cast<std::size_t>(list.count()));
    for (int row = 0; row < list.count(); ++row)
    {
        rows.push_back(QtStrings::toUtf8(list.item(row)->text()));
    }
    return rows;
}

/// @p texts as strings.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> strings(const std::array<std::string_view, N>& texts)
{
    return {texts.begin(), texts.end()};
}

/// The action of the item @p text in the submenu of @p group of @p combo; null when none.
[[nodiscard]] QAction* itemAction(const MaterialComboBox& combo, MaterialGroup group,
                                  const QString& text)
{
    for (QAction* action : combo.groupMenu(group).actions())
    {
        if (action->text() == text)
        {
            return action;
        }
    }
    return nullptr;
}

/// A shown, active window holding a material combo box and a line edit to take the focus.
class Window
{
public:
    explicit Window(std::shared_ptr<MaterialBinding> binding)
      : m_combo(makeChild<MaterialComboBox>(&m_window, std::move(binding))),
        m_other(makeChild<QLineEdit>(&m_window)),
        m_active(showActiveWith(m_window, {m_combo, m_other}))
    {
    }

    [[nodiscard]] bool              isActive() const noexcept { return m_active; }
    [[nodiscard]] MaterialComboBox& combo() const noexcept { return *m_combo; }
    [[nodiscard]] QLineEdit&        other() const noexcept { return *m_other; }

private:
    QWidget           m_window;
    MaterialComboBox* m_combo{nullptr};
    QLineEdit*        m_other{nullptr};
    bool              m_active{false};
};

TEST_F(MaterialComboBoxTest, NeedsABinding)
{
    EXPECT_THROW(std::ignore = MaterialComboBox(nullptr, nullptr), BugError);
}

/// The items of @p combo.
[[nodiscard]] std::vector<std::string> itemTexts(const MaterialComboBox& combo)
{
    std::vector<std::string> texts;
    texts.reserve(static_cast<std::size_t>(combo.count()));
    for (int index = 0; index < combo.count(); ++index)
    {
        texts.push_back(QtStrings::toUtf8(combo.itemText(index)));
    }
    return texts;
}

/// The texts of the materials of @p binding.
[[nodiscard]] std::vector<std::string> displayStrings(const MaterialBinding& binding)
{
    std::vector<std::string> texts;
    texts.reserve(binding.materials().size());
    for (const Material& material : binding.materials())
    {
        texts.push_back(MaterialBinding::displayString(material));
    }
    return texts;
}

TEST_F(MaterialComboBoxTest, TheItemsAndTheShownMaterialAsOpenRocket)
{
    // probe: selected display='(ud) Doc foam (0.03 g/cm3)'; the items are the model's materials.
    MaterialDesign         design;
    const auto             binding = design.bind();
    const MaterialComboBox combo(binding, nullptr);
    EXPECT_EQ(itemTexts(combo), displayStrings(*binding));
    EXPECT_EQ(combo.currentIndex(), binding->indexOf(binding->value()));
    EXPECT_EQ(QtStrings::toUtf8(combo.shownText()), kSelectedDocFoam);
    EXPECT_EQ(combo.focusPolicy(), Qt::StrongFocus);
    EXPECT_FALSE(combo.isSearching());
}

TEST_F(MaterialComboBoxTest, TheGroupsPopupAsOpenRocket)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    window.combo().showPopup();
    EXPECT_TRUE(window.combo().popupMenu().isVisible());
    EXPECT_EQ(window.combo().searchField().placeholderText(),
              QStringLiteral("Enter the material name"));  // probe: placeholder
    EXPECT_TRUE(window.combo().searchField().hasFocus());
    EXPECT_FALSE(window.combo().searchList().isVisible());
    EXPECT_EQ(groupLines(window.combo()), strings(kDocFoamGroups));
    window.combo().hidePopup();
    EXPECT_FALSE(window.combo().popupMenu().isVisible());
}

TEST_F(MaterialComboBoxTest, AGroupItemSetsTheMaterial)
{
    // probe: setSelectedItem(Balsa): the tube's material is Balsa; the popup then checks Woods
    // and Balsa.
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QAction* balsa =
        itemAction(combo, MaterialGroup::WOODS,
                   QStringLiteral("Balsa (0.17 g/cm") + QChar(0x00b3) + QStringLiteral(")"));
    ASSERT_NE(balsa, nullptr);
    balsa->trigger();
    EXPECT_EQ(design.tube->getMaterial().getName(), "Balsa");
    EXPECT_FALSE(combo.popupMenu().isVisible());
    EXPECT_EQ(combo.currentText(), balsa->text());

    combo.showPopup();
    const std::vector<std::string> lines = groupLines(combo);
    ASSERT_EQ(lines.size(), kDocFoamGroups.size());
    EXPECT_EQ(lines.at(1), kBalsaWoods);
    EXPECT_EQ(lines.at(4), kBalsaFoams);
    QTest::keyClicks(&combo.searchField(), QStringLiteral("bal"));
    EXPECT_EQ(searchRows(combo), (std::vector<std::string>{std::string(kSearchBalsa)}));
}

TEST_F(MaterialComboBoxTest, TheSearchAsOpenRocket)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    for (const auto& [text, rows] : docFoamSearches())
    {
        // Typed at once (a paste: QTest types ASCII only), which edits the text as keys do.
        combo.searchField().clear();
        combo.searchField().insert(QtStrings::fromUtf8(text));
        EXPECT_TRUE(combo.isSearching()) << text;
        EXPECT_EQ(searchRows(combo), rows) << text;
        EXPECT_EQ(combo.searchList().currentRow(), rows.empty() ? -1 : 0) << text;
    }
}

TEST_F(MaterialComboBoxTest, TheSearchListTakesTheKeys)
{
    // Java: Down and Up move the highlight (not beyond the list), Enter takes the highlighted
    // material and closes the popup.
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QLineEdit& field = combo.searchField();
    QTest::keyClicks(&field, QStringLiteral("PL"));
    ASSERT_EQ(combo.searchList().count(), 3);  // Maple, PLA, Plywood
    QTest::keyClick(&field, Qt::Key_Up);
    EXPECT_EQ(combo.searchList().currentRow(), 0);
    QTest::keyClick(&field, Qt::Key_Down);
    QTest::keyClick(&field, Qt::Key_Down);
    QTest::keyClick(&field, Qt::Key_Down);
    EXPECT_EQ(combo.searchList().currentRow(), 2);
    QTest::keyClick(&field, Qt::Key_Up);
    EXPECT_EQ(field.text(), QStringLiteral("PL"));
    QTest::keyClick(&field, Qt::Key_Return);
    EXPECT_EQ(design.tube->getMaterial().getName(), "PLA - 100% infill");
    EXPECT_FALSE(combo.popupMenu().isVisible());
}

TEST_F(MaterialComboBoxTest, TheMouseTakesASearchRow)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("1.05"));
    ASSERT_EQ(combo.searchList().count(), 4);
    Q_EMIT combo.searchList().itemEntered(combo.searchList().item(2));
    EXPECT_EQ(combo.searchList().currentRow(), 2);
    Q_EMIT combo.searchList().itemClicked(combo.searchList().item(3));
    EXPECT_EQ(design.tube->getMaterial().getName(), "Quantum tubing");
    EXPECT_FALSE(combo.popupMenu().isVisible());
}

TEST_F(MaterialComboBoxTest, AnEmptySearchFieldBringsTheGroupsBack)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("ba"));
    EXPECT_TRUE(combo.isSearching());
    EXPECT_TRUE(groupLines(combo).empty());
    EXPECT_TRUE(combo.searchList().isVisible());
    QTest::keyClick(&combo.searchField(), Qt::Key_Backspace);
    QTest::keyClick(&combo.searchField(), Qt::Key_Backspace);
    EXPECT_FALSE(combo.isSearching());
    EXPECT_FALSE(combo.searchList().isVisible());
    EXPECT_EQ(groupLines(combo), strings(kDocFoamGroups));
    EXPECT_EQ(design.tube->getMaterial().getName(), "Doc foam");

    // Opened again, the popup starts with an empty search field and the groups.
    QTest::keyClicks(&combo.searchField(), QStringLiteral("ba"));
    combo.hidePopup();
    combo.showPopup();
    EXPECT_FALSE(combo.isSearching());
    EXPECT_TRUE(combo.searchField().text().isEmpty());
}

TEST_F(MaterialComboBoxTest, EscClosesThePopupAndSetsNothing)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QTest::keyClicks(&combo.searchField(), QStringLiteral("alu"));
    QTest::keyClick(&combo.searchField(), Qt::Key_Escape);
    EXPECT_FALSE(combo.popupMenu().isVisible());
    EXPECT_EQ(design.tube->getMaterial().getName(), "Doc foam");
}

TEST_F(MaterialComboBoxTest, AKeyTypedIntoThePopupGoesToTheSearchField)
{
    // The popup takes the keyboard when the mouse moves over a submenu.
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    QTest::keyClicks(&combo.popupMenu(), QStringLiteral("nyl"));
    EXPECT_EQ(combo.searchField().text(), QStringLiteral("nyl"));
    EXPECT_TRUE(combo.isSearching());
    EXPECT_EQ(searchRows(combo), (std::vector<std::string>{std::string(kSearchNylon)}));
}

TEST_F(MaterialComboBoxTest, AMaterialInNoDatabaseIsShownApart)
{
    // probe: loose: selected display='(ud) Loose (0.123 g/cm3)' index=-1; the popup checks
    // Woods and no item.
    MaterialDesign design;
    design.tube->setMaterial(Material::newMaterial(Material::Type::BULK, "Loose", 123,
                                                   MaterialGroup::WOODS, true, false));
    const Window window(design.bind());
    ASSERT_TRUE(window.isActive());
    MaterialComboBox& combo = window.combo();
    EXPECT_EQ(combo.currentIndex(), -1);
    EXPECT_EQ(QtStrings::toUtf8(combo.shownText()), kSelectedLoose);
    const OffscreenPopupWarnings warnings;
    combo.showPopup();
    const std::vector<std::string> lines = groupLines(combo);
    ASSERT_EQ(lines.size(), kDocFoamGroups.size());
    EXPECT_EQ(lines.at(1), kLooseWoods);
    EXPECT_EQ(lines.at(4), kLooseFoams);
}

TEST_F(MaterialComboBoxTest, AChoiceOnTheClosedComboBoxSetsEverySelectedComponent)
{
    MaterialDesign                        design;
    const std::array<RocketComponent*, 1> others{design.other};
    MaterialComboBox                      combo(design.bind(others), nullptr);
    const int                             balsa =
        combo.findText(QStringLiteral("Balsa (0.17 g/cm") + QChar(0x00b3) + QStringLiteral(")"));
    ASSERT_GE(balsa, 0);
    Q_EMIT combo.activated(balsa);
    EXPECT_EQ(design.tube->getMaterial().getName(), "Balsa");
    EXPECT_EQ(design.other->getMaterial().getName(), "Balsa");
    EXPECT_EQ(combo.currentIndex(), balsa);
}

TEST_F(MaterialComboBoxTest, ADatabaseChangeRebuildsTheItems)
{
    MaterialDesign         design;
    const MaterialComboBox combo(design.bind(), nullptr);
    const int              before = combo.count();
    design.application.addMaterial(
        Material::newMaterial(Material::Type::BULK, "Added", 100, MaterialGroup::WOODS, true));
    EXPECT_EQ(combo.count(), before + 1);
    EXPECT_EQ(combo.currentText(), combo.itemText(combo.currentIndex()));
    EXPECT_EQ(QtStrings::toUtf8(combo.shownText()), kSelectedDocFoam);
}

TEST_F(MaterialComboBoxTest, TheTextsFollowTheDensityUnitOnRefresh)
{
    // A unit change fires nothing: refresh() reads the texts anew (the preferences' Apply).
    const QtRocket::Test::DefaultUnitsGuard units;
    MaterialDesign                          design;
    MaterialComboBox                        combo(design.bind(), nullptr);
    ASSERT_TRUE(
        unitGroup(UnitGroupId::DENSITY_BULK).setDefaultUnit("kg/m" + std::string("\xc2\xb3")));
    combo.refresh();
    EXPECT_EQ(combo.shownText(),
              QStringLiteral("(ud) Doc foam (30 kg/m") + QChar(0x00b3) + QStringLiteral(")"));
}

TEST_F(MaterialComboBoxTest, AnInvalidSelectionDisablesItAndClosesThePopup)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    const OffscreenPopupWarnings warnings;
    MaterialComboBox&            combo = window.combo();
    combo.showPopup();
    std::ignore = design.document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
    EXPECT_FALSE(combo.isEnabled());
    EXPECT_FALSE(combo.popupMenu().isVisible());
    combo.showPopup();
    EXPECT_FALSE(combo.popupMenu().isVisible());
    Q_EMIT combo.activated(0);
    EXPECT_EQ(design.tube->getMaterial().getName(), "Doc foam");
    EXPECT_EQ(QtStrings::toUtf8(combo.shownText()), kSelectedDocFoam);
}

TEST_F(MaterialComboBoxTest, TheBindingAndTheFieldEnableItTogether)
{
    MaterialDesign   design;
    MaterialComboBox combo(design.bind(), nullptr);
    combo.setFieldEnabled(false);
    EXPECT_FALSE(combo.isEnabled());
    EXPECT_FALSE(combo.isFieldEnabled());
    combo.showPopup();  // disabled: no popup
    EXPECT_FALSE(combo.popupMenu().isVisible());
    combo.setFieldEnabled(true);
    EXPECT_TRUE(combo.isEnabled());
}

TEST_F(MaterialComboBoxTest, TheWheelChoosesOnlyWithTheFocus)
{
    MaterialDesign design;
    const Window   window(design.bind());
    ASSERT_TRUE(window.isActive());
    MaterialComboBox& combo = window.combo();
    window.other().setFocus();
    EXPECT_FALSE(turnWheel(combo, -120));
    EXPECT_EQ(design.tube->getMaterial().getName(), "Doc foam");
    EXPECT_FALSE(combo.hasFocus());

    combo.setFocus();
    const bool scrolls =
        combo.style()->styleHint(QStyle::SH_ComboBox_AllowWheelScrolling, nullptr, &combo) != 0;
    const int         next = combo.currentIndex() + 1;
    const std::string expected =
        scrolls ? combo.binding()->materialAt(next)->getName() : std::string("Doc foam");
    static_cast<void>(turnWheel(combo, -120));
    EXPECT_EQ(design.tube->getMaterial().getName(), expected);
}

TEST_F(MaterialComboBoxTest, ACoreErrorOfAChoiceIsReported)
{
    MaterialDesign           design;
    const RecordingGuardHook hook;
    const auto               throwing = MaterialSource::of<ExternalComponent>({
        .get = [](const ExternalComponent& c) { return c.getMaterial(); },
        .set = [](ExternalComponent&, const Material&) { bug("a broken setter"); },
    });
    MaterialComboBox         combo(std::make_shared<MaterialBinding>(
                                       std::make_shared<ComponentSelection>(*design.tube), throwing,
                                       Material::Type::BULK, design.application, *design.document),
                                   nullptr);
    combo.setObjectName(QStringLiteral("Material"));
    Q_EMIT combo.activated(0);
    ASSERT_EQ(hook.failures().size(), 1U);
    EXPECT_EQ(hook.failures().front().context, QStringLiteral("Material"));
    EXPECT_EQ(QtStrings::toUtf8(combo.shownText()), kSelectedDocFoam);
}

}  // namespace

}  // namespace QtRocket::Gui
