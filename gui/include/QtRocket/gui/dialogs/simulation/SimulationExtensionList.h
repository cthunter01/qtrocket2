#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <QObject>
#include <QScrollArea>
#include <QSize>

class QEvent;
class QFrame;
class QLabel;
class QPushButton;
class QWidget;

namespace QtRocket
{
class Simulation;
class SimulationExtension;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class StyledLabel;

/// The extensions of a simulation, as the simulation options tab lists them (OpenRocket's
/// SimulationOptionsPanel, swing gui/simulation/: its currentExtensions in a scroll pane, each
/// extension a SimulationExtensionPanel): for every extension of Simulation::
/// getSimulationExtensions(), in order, a panel with a line border in the dim text colour
/// (OR.colors.text.dim) holding the extension's name (SimulationExtension::getName()) and, on the
/// right below it, a "Help" button (Icons::HELP) when the extension has a description and a
/// "Delete" button (Icons::EDIT_DELETE); without extensions the dim italic line "No simulation
/// extensions defined". The scroll area is 140 pixels high at first and at least 90 (Java's
/// EXTENSION_LIST_HEIGHT and minimum size).
///
/// - Help (showHelp()): a dialog titled with the extension's name, modal for the application,
///   with the extension's description in a DescriptionArea of 10 rows (400 pixels wide) and
///   "Close", the default button, which Escape clicks too (Java: GUIUtil.
///   setDisposableDialogOptions()); a ModalDialog child of the list's window.
/// - Delete (deleteExtension()): removes the extension from the simulation's list (by identity,
///   as Java), lists the rest and emits extensionsChanged(). As in Java the simulation tells
///   nobody (Simulation emits no changed() for its list, and its status does not depend on it).
///
/// Milestone 4 shows the extensions with Help and Delete only (the user's decision U-M4-3):
/// Java's "Configure" button of an extension that has a configurator (Air-start, Roll control,
/// the scripting and Java code extensions), the box's "Add extension" button and its "Copy
/// extension" menu come with Milestone 7. An extension QtRocket cannot run (the scripting
/// extension, whose run reports that QtRocket does not support the scripting language) is listed
/// as Java lists it, with its name, Help and Delete.
///
/// The simulation is held by its std::shared_ptr: the list may outlive the simulation's place in
/// its document (the settings dialog closes before it undoes).
///
/// Deviation from OpenRocket: the names and descriptions are the core's English texts
/// (SimulationExtension::getName(), getDescription()), untranslated: an extension's name holds its
/// configuration ("Air-start (100 m, 50 m/s)"), which no translation key covers (Java formats
/// translated texts).
class SimulationExtensionList final : public QScrollArea
{
    Q_OBJECT

public:
    /// Java's EXTENSION_LIST_HEIGHT: the height the list asks for.
    static constexpr int kPreferredHeight = 140;
    /// The list's minimum height (Java: setMinimumSize(0, 90)).
    static constexpr int kMinimumHeight = 90;
    /// The rows of the help dialog's description (Java: DescriptionArea(text, 10, 0)).
    static constexpr int kHelpRows = 10;
    /// The width of the help dialog's description (Java: "width 400lp").
    static constexpr int kHelpWidth = 400;

    /// The list of @p simulation's extensions.
    /// @throws BugError when @p simulation is null
    SimulationExtensionList(std::shared_ptr<Simulation> simulation, QWidget* parent);
    ~SimulationExtensionList() override;
    SimulationExtensionList(const SimulationExtensionList&)            = delete;
    SimulationExtensionList& operator=(const SimulationExtensionList&) = delete;
    SimulationExtensionList(SimulationExtensionList&&)                 = delete;
    SimulationExtensionList& operator=(SimulationExtensionList&&)      = delete;

    /// The simulation whose extensions are listed.
    [[nodiscard]] const std::shared_ptr<Simulation>& simulation() const noexcept
    {
        return m_simulation;
    }

    /// The number of extensions listed (the panels).
    [[nodiscard]] std::size_t extensionCount() const noexcept { return m_rows.size(); }
    /// The panel of the extension at @p index (objectName extensionPanel).
    /// @throws BugError for an index out of range (for all of these)
    [[nodiscard]] QFrame& extensionPanel(std::size_t index) const;
    /// Its name (objectName extensionName).
    [[nodiscard]] QLabel& nameLabel(std::size_t index) const;
    /// Its "Help" button (objectName helpButton), null for an extension without a description.
    [[nodiscard]] QPushButton* helpButton(std::size_t index) const;
    /// Its "Delete" button (objectName deleteButton).
    [[nodiscard]] QPushButton& deleteButton(std::size_t index) const;
    /// "No simulation extensions defined" (objectName noExtensionsLabel), null while there are
    /// extensions.
    [[nodiscard]] StyledLabel* noExtensionsLabel() const noexcept { return m_noExtensions; }

    /// Lists the simulation's extensions again (Java: updateCurrentExtensions()).
    void refresh();

    /// The help dialog of the extension at @p index (see the class comment); returns when it is
    /// closed. Nothing for an extension without a description.
    void showHelp(std::size_t index);

    /// Deletes the extension at @p index (see the class comment).
    void deleteExtension(std::size_t index);

    /// The list's preferred size: its content's width, kPreferredHeight high.
    [[nodiscard]] QSize sizeHint() const override;

Q_SIGNALS:
    /// The list changed: an extension was deleted (QtRocket's; Java tells nobody).
    void extensionsChanged();

protected:
    void changeEvent(QEvent* event) override;

private:
    /// The widgets of an extension's panel, with the extension it shows.
    struct Row
    {
        std::shared_ptr<SimulationExtension> extension;
        QFrame*                              panel  = nullptr;
        QLabel*                              name   = nullptr;
        QPushButton*                         help   = nullptr;
        QPushButton*                         remove = nullptr;
    };

    void addRow(const std::shared_ptr<SimulationExtension>& extension);
    /// The help dialog of @p extension (showHelp()).
    void showHelpOf(const SimulationExtension& extension);
    /// Deletes @p extension, found by identity (deleteExtension()).
    void deleteExtensionObject(const SimulationExtension& extension);
    /// The panels' border and the empty line in the dim text colour of the palette.
    void                     updateColors();
    [[nodiscard]] const Row& rowAt(std::size_t index) const;

    std::shared_ptr<Simulation> m_simulation;
    QWidget*                    m_content{nullptr};
    std::vector<Row>            m_rows;
    StyledLabel*                m_noExtensions{nullptr};
};

}  // namespace QtRocket::Gui
