#pragma once

// The replay of the Java probes' panel sections (LugPanelProbe and FinPanelProbe of
// tools/openrocket-panel-pins, run on OpenRocket's Swing panels under Xvfb): the panel of every
// component of a kind in a design, edits through a panel as a user makes them, and edits of a panel
// with a second component selected, each section a header and its sorted lines, as the probes print
// them. lug_panel_golden_tests.cpp and fin_panel_golden_tests.cpp compare what QtRocket's panels
// give with the pinned transcripts.

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{
class BodyTube;
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class RocketComponentConfig;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

class ConfigDialogServices;

/// Java's Double.toString() of @p value (JDK 17, which ran the probes).
[[nodiscard]] std::string javaDouble(double value);

/// An edit of a probe: the property (the widget's objectName) and what is done to it: the text its
/// spin box commits (in its unit: "3" is 3 cm), "int:<n>" for a count, "click" for a check box,
/// "select:<NAME>" for the item of a combo box (the enum constant's Java name), "button" for the
/// push button whose objectName is the property with a lower-case first letter and "Button"
/// ("AutoCalc": autoCalcButton).
struct PanelEdit
{
    std::string_view property;
    std::string_view value;
};

/// Makes @p edit through the widgets of @p panel, as the probes' apply() does through the Swing
/// panel's models (the spinner model's setValue() of the text or the count, doClick(), the combo
/// box's setSelectedItem()): whether the widget is there and took the edit (a combo box item that
/// is selected already is taken without a change, as Java's).
[[nodiscard]] bool applyPanelEdit(const RocketComponentConfig& panel, const PanelEdit& edit);

/// What a replay prints of a design's panels and components.
struct PanelReplayLines
{
    /// Whether a component gets a panel in panels().
    std::function<bool(const RocketComponent&)> isPanelKind;
    /// The class name the probe prints for a component ("TubeFinSet").
    std::function<std::string_view(const RocketComponent&)> className;
    /// The lines of a panel, with the labels (when a section starts) or without them (after an
    /// edit).
    std::function<void(const RocketComponentConfig&, bool withLabels, std::vector<std::string>&)>
        panel;
    /// The probe's state() of a component: its values, each line after the prefix ("main ") of
    /// a multi section; @p fromDesign is true for panels() (the design as loaded or built) and
    /// false after edits.
    std::function<void(const RocketComponent&, std::string_view prefix, bool fromDesign,
                       std::vector<std::string>&)>
        state;
};

/// The probes' replay of one design: the sections they print, each a header and its sorted lines.
class PanelReplay
{
public:
    PanelReplay(OpenRocketDocument& document, const ConfigDialogServices& services,
                PanelReplayLines lines);

    [[nodiscard]] const std::vector<std::string>& lines() const noexcept { return m_lines; }

    /// The probes' design(): "== panel | <index> | <class>" for every component of the panel kind,
    /// in tree order, each after the design was settled (settleAutomaticDimensions()).
    void panels();

    /// The probes' edits(): the panel of the component @p index, then @p edits through it, each
    /// followed by "== edit | <index> | <property> <value>".
    void edits(std::size_t index, std::span<const PanelEdit> edits);

    /// The probes' multi(): the panel of the component @p main with the component @p other
    /// selected too, and @p edits through it, after each the values of both.
    void multi(std::size_t main, std::size_t other, std::span<const PanelEdit> edits);

private:
    void settle() const;
    void addBothStates(const RocketComponent& main, const RocketComponent& other);
    void addSection(std::vector<std::string> section);

    OpenRocketDocument*           m_document;
    const ConfigDialogServices*   m_services;
    PanelReplayLines              m_printer;
    std::vector<RocketComponent*> m_components;
    std::vector<std::string>      m_lines;
};

/// The document of a design the probes opened: "example" (a file of data/examples, loaded
/// without presets and settled) or "testrocket" (a TestRockets maker); null when there is none.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> pinnedDesignDocument(std::string_view source,
                                                                       std::string_view design);

/// The probes' onBodyTube(): Rocket > Stage > a body tube 30 cm long with a fixed radius of
/// 2.1 cm, to which @p addChildren adds the components; with its events on.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onBodyTube(
    const std::function<void(BodyTube&)>& addChildren);

/// @p pinned with @p javaLine, a line of the sections after the header @p header, replaced by
/// @p ourLine: where QtRocket differs from the pin by design. The replacement must happen.
[[nodiscard]] std::vector<std::string_view> withDifference(std::span<const std::string_view> pinned,
                                                           std::string_view                  header,
                                                           std::string_view javaLine,
                                                           std::string_view ourLine);

}  // namespace QtRocket::Gui::Test
