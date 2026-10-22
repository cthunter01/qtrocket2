#pragma once

#include <memory>
#include <optional>

#include <QObject>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/rocket/ComponentKind.h"

class QGridLayout;
class QHBoxLayout;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class DescriptionArea;
class IconToggleButton;
class StyledLabel;
class TextCommitter;

/// The contents of the component configuration dialog (OpenRocket's RocketComponentConfig, swing
/// gui/configdialog/RocketComponentConfig.java): the configuration of the selected components,
/// with the parts every component kind shares. The panel of a kind (run 3: BodyTubeConfig, ...)
/// derives from it and inserts its own tabs in front (insertTab(), as Java's tabbedPane.insertTab(
/// ..., 0)); several components of different classes get this class itself (ConfigPanelFactory).
///
/// - The header: "Component name:" and the name field, which commits on Enter and on the loss of
///   the focus and then commits the comment too (a TextCommitter of both: Java's
///   TextFieldListener); a name given to several components names them all.
/// - The tabs: the panel's own, then "Override" (the mass, the CG and the CD) and "Comment".
/// - Below them the component's description (DescriptionArea, ComponentInfo.<class>), hidden until
///   the info button shows it; not in a multi-component edit.
/// - The button row: the info button, the bold "Multi-component edit" label of a multi-component
///   edit (its tooltip names the components), the mass label "Component mass: <m>" with
///   "(overridden to <m>)" or "(overridden by <name>)" (only when every selected component is
///   massive and only one is selected; the preset "<manufacturer> <part>" in front of it), the
///   buttons a panel adds (addButton()), Cancel and OK. The dialog (ComponentConfigDialog) connects
///   Cancel and OK.
///
/// The Override tab: for the mass, the CG and the CD a check box, "Override for all subcomponents"
/// (enabled while the check box is on), a bold label "Mass overridden by <name>" when an ancestor
/// overrides it (the whole block disabled then, decided when the panel is made, as Java), and the
/// number field (mass: at least 0, slider 0, 30 g, 1 kg; CG: at least 0, slider from 0 to the
/// extent of the component and its children, computed when the panel is made, or to the
/// component's length; CD: any value, no unit selector, slider -1 to 1). The CD shows, while it is
/// not overridden, the CD the force analysis gives at the preferences' default Mach number
/// (ComponentDrag::getOverrideCD(); cached per event of the rocket by the binding, it is a whole
/// analysis), or the Error's text in its place; switching the override on stores that CD in every
/// selected component (each its own, all calculated before the first is switched: an override
/// changes the calculated CD of other components) before it switches it on (Java's
/// getOverrideCD() stores the calculated CD whenever it is read).
///
/// The bindings live in scope() (Java: register() and invalidateMe()); a subclass makes its own
/// there too, and the shared panels (PlacementPanel, MaterialPanel, ...) take the scope. The panel
/// keeps no component pointer: it reaches the components through the selection, which drops them
/// in the TREE or UNDO change that ends it. updateFields() refreshes what no binding shows (the
/// mass label); the dialog calls it after every other change of the rocket (Java's updateFields()).
///
/// Deviations from OpenRocket:
/// - No preset combo box, "Parts Library" button or header menu (Milestones 3, 5 and 8), and no
///   Appearance tab (Milestone 5).
/// - Component names never change the markup of a text: they are escaped in the multi-component
///   tooltip and in the CD's "overridden by" label (rich text), and the mass and CG "overridden by"
///   labels and the mass label are plain text (Java inserts the names into HTML, or into a label
///   that takes a text starting with <html> as HTML).
/// - Switching the CD override on gives every selected component its own calculated CD, all of
///   them calculated before the first is switched. Java's override starts from the CD each
///   component stored when it was last read: the main component's as the dialog showed it, the
///   others' from whenever something last read them.
/// As Java: the CG slider's maximum is computed from the main component, its maxL starting at
/// Double.MIN_VALUE (the smallest positive double); updateFields() hides the description again.
class RocketComponentConfig : public QWidget
{
    Q_OBJECT

public:
    /// The configuration of the components of @p selection, a selection of the rocket of
    /// @p document; @p services gives the preferences (the default Mach number, the default
    /// units) and the application's materials.
    /// @throws BugError when @p selection is null or invalid
    RocketComponentConfig(OpenRocketDocument& document, const GuiServices& services,
                          std::shared_ptr<ComponentSelection> selection, QWidget* parent);
    ~RocketComponentConfig() override;
    RocketComponentConfig(const RocketComponentConfig&)            = delete;
    RocketComponentConfig& operator=(const RocketComponentConfig&) = delete;
    RocketComponentConfig(RocketComponentConfig&&)                 = delete;
    RocketComponentConfig& operator=(RocketComponentConfig&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] const GuiServices&  services() const noexcept { return m_services; }
    [[nodiscard]] const std::shared_ptr<ComponentSelection>& selection() const noexcept
    {
        return m_scope.selection();
    }
    /// The bindings of the panel and of its shared panels.
    [[nodiscard]] BindingScope& scope() noexcept { return m_scope; }

    /// Whether the panel edits a component just added (Java: setNewComponent()); the dialog's
    /// Cancel then asks to undo adding it.
    void setNewComponent(bool newComponent) noexcept { m_newComponent = newComponent; }
    [[nodiscard]] bool isNewComponent() const noexcept { return m_newComponent; }

    /// Whether every selected component is of the main component's class.
    [[nodiscard]] bool isAllSameType() const noexcept { return m_allSameType; }
    /// Whether every selected component is massive.
    [[nodiscard]] bool isAllMassive() const noexcept { return m_allMassive; }
    /// The kind of the main component when the panel was made.
    [[nodiscard]] ComponentKind mainKind() const noexcept { return m_mainKind; }

    // ---------------------------------------------------------------------------- the tabs

    [[nodiscard]] QTabWidget& tabs() const noexcept { return *m_tabs; }

    /// Inserts @p page as the tab @p index titled @p title with the tooltip @p toolTip (Java:
    /// tabbedPane.insertTab()).
    void insertTab(int index, QWidget& page, const QString& title, const QString& toolTip);

    /// The title of the selected tab (Java: getSelectedTabName()); empty without tabs.
    [[nodiscard]] QString selectedTabName() const;
    [[nodiscard]] int     selectedTabIndex() const;
    /// Selects the tab titled @p title, the first one when there is none (Java: setSelectedTab()).
    void setSelectedTab(const QString& title);
    void setSelectedTabIndex(int index);

    // ---------------------------------------------------------------------------- the rows

    [[nodiscard]] QLineEdit&      nameField() const noexcept { return *m_nameField; }
    [[nodiscard]] QPlainTextEdit& commentArea() const noexcept { return *m_commentArea; }
    [[nodiscard]] TextCommitter&  textCommitter() const noexcept { return *m_textCommitter; }
    /// The info button and the component's description; null for a multi-component edit and for
    /// a kind without a description.
    [[nodiscard]] IconToggleButton* componentInfoButton() const noexcept { return m_infoButton; }
    [[nodiscard]] DescriptionArea*  componentInfo() const noexcept { return m_componentInfo; }
    [[nodiscard]] StyledLabel&      multiComponentEditLabel() const noexcept
    {
        return *m_multiCompEditLabel;
    }
    [[nodiscard]] StyledLabel& infoLabel() const noexcept { return *m_infoLabel; }
    [[nodiscard]] QPushButton& okButton() const noexcept { return *m_okButton; }
    [[nodiscard]] QPushButton& cancelButton() const noexcept { return *m_cancelButton; }

    /// Adds @p button to the button row, before Cancel (Java: addButtons(JButton...)).
    void addButton(QPushButton& button);

    /// Refreshes what no binding shows: the mass label (Java: updateFields(); a subclass that
    /// overrides it calls this one).
    virtual void updateFields();

    /// Commits the pending edits of every field (BindingScope::commitAll()); a panel with edits of
    /// its own that no binding holds commits them too (FreeformFinSetConfig: the point table's
    /// open cell editor) and calls this one.
    virtual bool commitAll();

    /// Ends the bindings before the dialog goes (BindingScope::release()).
    void release() noexcept;

    /// Puts Cancel and OK last in the focus chain, after the fields the panel and its subclass
    /// made (Java: the "order" list with Cancel and OK at its end).
    void orderFocus();

    /// The description of @p kind (ComponentInfo.<class>), HTML without <html>.
    [[nodiscard]] static QString componentInfoText(ComponentKind kind);

    /// The source of the override CD as the Override tab shows it (see the class comment):
    /// ComponentDrag::getOverrideCD() at @p preferences' default Mach number, read at every read.
    [[nodiscard]] static DoubleSource overrideCDSource(const Preferences& preferences);
    /// The source of "Override coefficient of drag" of the components of @p selection: switching
    /// it on stores each component's calculated CD first, all of them calculated before any of
    /// them is switched (see the class comment).
    [[nodiscard]] static BoolSource cdOverriddenSource(
        const Preferences& preferences, const std::shared_ptr<ComponentSelection>& selection);

    /// The maximum of the CG override's slider when it is a constant (Java, overrideTab()): the
    /// extent of @p component and its subtree, or a childless fin set's bounding box; nullopt when
    /// the slider ends at the component's length.
    [[nodiscard]] static std::optional<double> overrideCGExtent(const RocketComponent& component);

private:
    void addHeader();
    void addOverrideTab();
    void addMassOverride(QGridLayout& grid);
    void addCGOverride(QGridLayout& grid);
    void addCDOverride(QGridLayout& grid);
    void addCommentTab();
    void addButtonRow();
    void addComponentInfo(QHBoxLayout& buttons);

    OpenRocketDocument* m_document;
    GuiServices         m_services;
    BindingScope        m_scope;
    ComponentKind       m_mainKind{ComponentKind::ROCKET};
    bool                m_allSameType{true};
    bool                m_allMassive{false};
    bool                m_newComponent{false};

    QVBoxLayout*      m_layout{nullptr};
    QLineEdit*        m_nameField{nullptr};
    QPlainTextEdit*   m_commentArea{nullptr};
    TextCommitter*    m_textCommitter{nullptr};
    QTabWidget*       m_tabs{nullptr};
    DescriptionArea*  m_componentInfo{nullptr};
    IconToggleButton* m_infoButton{nullptr};
    StyledLabel*      m_multiCompEditLabel{nullptr};
    StyledLabel*      m_infoLabel{nullptr};
    QHBoxLayout*      m_buttonRow{nullptr};
    QPushButton*      m_cancelButton{nullptr};
    QPushButton*      m_okButton{nullptr};
};

}  // namespace QtRocket::Gui
