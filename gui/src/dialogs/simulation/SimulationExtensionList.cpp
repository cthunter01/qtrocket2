#include "QtRocket/gui/dialogs/simulation/SimulationExtensionList.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the list's reports (Guard).
QString context()
{
    return QStringLiteral("Simulation extensions");
}

/// The padding of the empty line (Java: "pad 5 5 5 5").
constexpr int kEmptyPadding = 5;

}  // namespace

SimulationExtensionList::SimulationExtensionList(std::shared_ptr<Simulation> simulation,
                                                 QWidget*                    parent)
  : QScrollArea(parent), m_simulation(std::move(simulation))
{
    if (m_simulation == nullptr)
    {
        bug("The list of a simulation's extensions needs the simulation");
    }
    setObjectName(QStringLiteral("currentExtensions"));
    setWidgetResizable(true);
    setMinimumHeight(kMinimumHeight);
    m_content = makeChild<QWidget>(this);
    // Java: MigLayout("fillx, gap 0 0, ins 0"), the panels from the top.
    auto* column = makeChild<QVBoxLayout>(m_content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    column->addStretch(1);
    setWidget(m_content);
    refresh();
}

SimulationExtensionList::~SimulationExtensionList() = default;

void SimulationExtensionList::refresh()
{
    auto* column = dynamic_cast<QVBoxLayout*>(m_content->layout());
    QTROCKET_ASSERT(column != nullptr);
    // The old panels go later: a Delete button of one of them may be what is being clicked.
    std::vector<QWidget*> old;
    old.reserve(m_rows.size() + 1);
    for (const Row& row : m_rows)
    {
        old.push_back(row.panel);
    }
    if (m_noExtensions != nullptr)
    {
        old.push_back(m_noExtensions);
    }
    for (QWidget* widget : old)
    {
        column->removeWidget(widget);
        widget->hide();
        widget->deleteLater();
    }
    m_rows.clear();
    m_noExtensions = nullptr;

    const std::vector<std::shared_ptr<SimulationExtension>>& extensions =
        m_simulation->getSimulationExtensions();
    if (extensions.empty())
    {
        //: simedtdlg.SimExt.noExtensions
        m_noExtensions = makeChild<StyledLabel>(m_content, tr("No simulation extensions defined"),
                                                StyledLabel::Style::ITALIC);
        m_noExtensions->setObjectName(QStringLiteral("noExtensionsLabel"));
        m_noExtensions->setContentsMargins(kEmptyPadding, kEmptyPadding, kEmptyPadding,
                                           kEmptyPadding);
        column->insertWidget(0, m_noExtensions);
    }
    else
    {
        for (const std::shared_ptr<SimulationExtension>& extension : extensions)
        {
            addRow(extension);
        }
    }
    updateColors();
}

void SimulationExtensionList::addRow(const std::shared_ptr<SimulationExtension>& extension)
{
    QTROCKET_ASSERT(extension != nullptr);
    auto* column = dynamic_cast<QVBoxLayout*>(m_content->layout());
    QTROCKET_ASSERT(column != nullptr);

    // Java: SimulationExtensionPanel, MigLayout("fillx, gapx 0") with a line border: the name
    // across the panel, then the buttons on the right.
    Row row;
    row.extension = extension;
    row.panel     = makeChild<QFrame>(m_content);
    row.panel->setObjectName(QStringLiteral("extensionPanel"));
    row.panel->setFrameShape(QFrame::Box);
    row.panel->setFrameShadow(QFrame::Plain);
    row.panel->setLineWidth(1);
    auto* lines = makeChild<QVBoxLayout>(row.panel);
    row.name    = makeChild<QLabel>(row.panel, QtStrings::fromUtf8(extension->getName()));
    row.name->setObjectName(QStringLiteral("extensionName"));
    lines->addWidget(row.name);
    auto* buttonRow = makeChild<QWidget>(row.panel);
    auto* buttons   = makeChild<QHBoxLayout>(buttonRow);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(0);
    buttons->addStretch(1);
    const std::weak_ptr<SimulationExtension> weak = extension;
    // Java's Configure button, for an extension with a configurator, comes with Milestone 7.
    if (extension->getDescription().has_value())
    {
        row.help = makeChild<QPushButton>(buttonRow, Icons::icon(Icons::Id::HELP), QString());
        row.help->setObjectName(QStringLiteral("helpButton"));
        row.help->setAutoDefault(false);
        buttons->addWidget(row.help);
        connect(row.help, &QPushButton::clicked, this,
                guarded(
                    [this, weak] {
                        if (const auto shown = weak.lock())
                        {
                            showHelpOf(*shown);
                        }
                    },
                    context()));
    }
    row.remove = makeChild<QPushButton>(buttonRow, Icons::icon(Icons::Id::EDIT_DELETE), QString());
    row.remove->setObjectName(QStringLiteral("deleteButton"));
    row.remove->setAutoDefault(false);
    buttons->addWidget(row.remove);
    connect(row.remove, &QPushButton::clicked, this,
            guarded(
                [this, weak] {
                    if (const auto deleted = weak.lock())
                    {
                        deleteExtensionObject(*deleted);
                    }
                },
                context()));
    lines->addWidget(buttonRow);

    column->insertWidget(static_cast<int>(m_rows.size()), row.panel);
    m_rows.push_back(row);
}

void SimulationExtensionList::showHelp(std::size_t index)
{
    showHelpOf(*rowAt(index).extension);
}

void SimulationExtensionList::showHelpOf(const SimulationExtension& extension)
{
    const std::optional<std::string> description = extension.getDescription();
    if (!description.has_value())
    {
        return;
    }
    // Java: a JDialog of the panel's window titled with the extension's name, modal for the
    // application: the description and "Close" on the right (GUIUtil.setDisposableDialogOptions():
    // the default button, Escape closes).
    ModalDialog<QDialog> dialog(window());
    QDialog* const       help = dialog.get();
    help->setObjectName(QStringLiteral("extensionHelpDialog"));
    help->setWindowTitle(QtStrings::fromUtf8(extension.getName()));
    help->setWindowModality(Qt::ApplicationModal);
    auto* layout = makeChild<QVBoxLayout>(help);
    auto* area =
        makeChild<DescriptionArea>(help, QtStrings::fromUtf8(*description), kHelpRows, 0.0);
    area->setObjectName(QStringLiteral("descriptionArea"));
    area->setMinimumWidth(kHelpWidth);
    layout->addWidget(area);
    //: button.close
    auto* close = makeChild<QPushButton>(help, tr("Close"));
    close->setObjectName(QStringLiteral("closeButton"));
    close->setDefault(true);
    connect(close, &QPushButton::clicked, help, &QDialog::accept);
    layout->addWidget(close, 0, Qt::AlignRight);
    dialog.exec();
}

void SimulationExtensionList::deleteExtension(std::size_t index)
{
    // A copy: the row goes with the list.
    const std::shared_ptr<SimulationExtension> extension = rowAt(index).extension;
    deleteExtensionObject(*extension);
}

void SimulationExtensionList::deleteExtensionObject(const SimulationExtension& extension)
{
    std::vector<std::shared_ptr<SimulationExtension>>& extensions =
        m_simulation->getSimulationExtensions();
    // Compare with identity (Java: iter.next() == extension).
    for (auto it = extensions.begin(); it != extensions.end(); ++it)
    {
        if (it->get() == &extension)
        {
            extensions.erase(it);
            break;
        }
    }
    refresh();
    Q_EMIT extensionsChanged();
}

QFrame& SimulationExtensionList::extensionPanel(std::size_t index) const
{
    return *rowAt(index).panel;
}

QLabel& SimulationExtensionList::nameLabel(std::size_t index) const
{
    return *rowAt(index).name;
}

QPushButton* SimulationExtensionList::helpButton(std::size_t index) const
{
    return rowAt(index).help;
}

QPushButton& SimulationExtensionList::deleteButton(std::size_t index) const
{
    return *rowAt(index).remove;
}

const SimulationExtensionList::Row& SimulationExtensionList::rowAt(std::size_t index) const
{
    if (index >= m_rows.size())
    {
        bug("No simulation extension at that index of the list");
    }
    return m_rows[index];
}

QSize SimulationExtensionList::sizeHint() const
{
    return {QScrollArea::sizeHint().width(), kPreferredHeight};
}

void SimulationExtensionList::changeEvent(QEvent* event)
{
    QScrollArea::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
    {
        updateColors();
    }
}

void SimulationExtensionList::updateColors()
{
    const QColor dim = ThemeColors::color(ThemeColors::Key::TEXT_DIM, palette());
    for (const Row& row : m_rows)
    {
        // The border in the dim colour (Java: createLineBorder(dimTextColor)), the name in the
        // text colour.
        QPalette border = palette();
        border.setColor(QPalette::WindowText, dim);
        row.panel->setPalette(border);
        QPalette text = palette();
        text.setColor(QPalette::WindowText, palette().color(QPalette::WindowText));
        row.name->setPalette(text);
    }
    if (m_noExtensions != nullptr)
    {
        m_noExtensions->setFontColor(dim);
    }
}

}  // namespace QtRocket::Gui
