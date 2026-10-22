#pragma once

#include <memory>
#include <optional>

#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/DesignType.h"

class QEvent;
class QGridLayout;
class QLabel;
class QPlainTextEdit;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class ComponentSelection;
class EnumComboBox;
class GuiServices;
class TextCommitter;

/// The configuration of the rocket (OpenRocket's RocketConfig, swing
/// gui/configdialog/RocketConfig.java): no tabs, but rows in their place, label and field:
/// "Design name:" (the name field of RocketComponentConfig), "Designer:", "Design type:" (the
/// seven DesignType values by their names, kAllDesignTypes), "Kit name:" (shown unless the design
/// type is ORIGINAL; the dialog then fits its size), "Comments:" (the comment area of
/// RocketComponentConfig) and "Revision history:". The info button, the mass label, Cancel and OK
/// stay below them.
///
/// Java's RocketConfig removes everything its base put into the panel and lays it out anew; the
/// base's tabbed pane, no longer shown, keeps its tabs (Override, Comment) and their models. Here
/// the base's header and tab widget are hidden in the same way, with their tabs (tabs() and
/// selectedTabName() give them, as Java's getSelectedTabName() does), and the name field and the
/// comment area move into the rows.
///
/// The designer, the kit name and the revision history are text areas that commit on the loss of
/// their focus (a second TextCommitter: a commit of one commits all three, as Java's
/// TextFieldListener of RocketConfig); the name field and the comment area keep the base's
/// committer. The design type is set when the user chooses one.
///
/// The name field has OpenRocket's easter egg (Java: addEasterEgg()): when a typed character makes
/// the field's text (the text before the key and the key's character, as Java's keyTyped()
/// computes it) "SA-508", "SA-506", "Vega" or "Ariane 5", a message box tells a joke
/// (easterEggMessage(), shown through UserPrompts::showMessage() once the key is handled; Java
/// shows it before the character arrives in the field).
///
/// Deviation from OpenRocket: choosing the design type the combo box shows already sets nothing
/// (EnumBinding); Java's action listener sets it again, which fires an event (D2cProbe), marks the
/// dialog modified and refits it.
class RocketConfig : public RocketComponentConfig
{
    Q_OBJECT

public:
    /// The configuration of the rocket of @p selection.
    /// @throws BugError when @p selection is null or invalid, or its main component is no Rocket
    RocketConfig(OpenRocketDocument& document, const GuiServices& services,
                 std::shared_ptr<ComponentSelection> selection, QWidget* parent);
    ~RocketConfig() override;
    RocketConfig(const RocketConfig&)            = delete;
    RocketConfig& operator=(const RocketConfig&) = delete;
    RocketConfig(RocketConfig&&)                 = delete;
    RocketConfig& operator=(RocketConfig&&)      = delete;

    /// The widget of the rows (see the class comment).
    [[nodiscard]] QWidget&        rowsPage() const noexcept { return *m_rows; }
    [[nodiscard]] QPlainTextEdit& designerArea() const noexcept { return *m_designer; }
    [[nodiscard]] EnumComboBox&   designTypeComboBox() const noexcept { return *m_designType; }
    [[nodiscard]] QLabel&         kitNameLabel() const noexcept { return *m_kitNameLabel; }
    [[nodiscard]] QPlainTextEdit& kitNameArea() const noexcept { return *m_kitName; }
    [[nodiscard]] QPlainTextEdit& revisionArea() const noexcept { return *m_revision; }
    /// The committer of the designer, the kit name and the revision history.
    [[nodiscard]] TextCommitter& designTextCommitter() const noexcept { return *m_committer; }
    /// Whether the kit name's row is shown (the design type is not ORIGINAL).
    [[nodiscard]] bool isKitNameShown() const;

    /// The easter egg's message box for the name field's text @p name; nullopt for every other
    /// text (see the class comment).
    [[nodiscard]] static std::optional<UserPrompts::Message> easterEggMessage(const QString& name);

    /// The rocket's designer (Java: Rocket.getDesigner() and setDesigner()).
    [[nodiscard]] static StringSource designerSource();
    /// The rocket's kit name (Java: Rocket.getKitName() and setKitName()).
    [[nodiscard]] static StringSource kitNameSource();
    /// The rocket's revision history (Java: Rocket.getRevision() and setRevision()).
    [[nodiscard]] static StringSource revisionSource();
    /// The rocket's design type (Java: Rocket.getDesignType() and setDesignType()).
    [[nodiscard]] static EnumSource<DesignType> designTypeSource();

    /// The easter egg's key presses of the name field (see the class comment).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Adds the row @p row: the label @p text, at the top, and @p field.
    QLabel& addRow(QGridLayout& grid, int row, const QString& text, QWidget& field);
    /// A text area of the rows (Java: a JTextArea that wraps words, at least 400 wide and
    /// @p height high).
    [[nodiscard]] QPlainTextEdit& makeTextArea(int height);
    /// Shows the kit name's row unless the design type is ORIGINAL; refits a shown dialog.
    void updateKitName();

    QWidget*        m_rows{nullptr};
    QPlainTextEdit* m_designer{nullptr};
    EnumComboBox*   m_designType{nullptr};
    QLabel*         m_kitNameLabel{nullptr};
    QPlainTextEdit* m_kitName{nullptr};
    QPlainTextEdit* m_revision{nullptr};
    TextCommitter*  m_committer{nullptr};
};

}  // namespace QtRocket::Gui
