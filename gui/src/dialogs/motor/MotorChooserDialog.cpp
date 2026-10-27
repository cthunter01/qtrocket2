#include "QtRocket/gui/dialogs/motor/MotorChooserDialog.h"

#include <memory>
#include <optional>
#include <utility>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/motor/ThrustCurveMotorSelectionPanel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's unrelated gap ("gapright unrel, gapbottom unrel" of the buttons).
constexpr int kUnrelatedGap = 12;

/// The context of the dialog's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motor chooser");
}

/// Whether the platform puts Cancel before OK (MigLayout's button order; QDialogButtonBox's
/// layouts, whose policy is the style's): macOS and GNOME do, Windows, KDE and Android put OK
/// first.
[[nodiscard]] bool cancelFirst(const QWidget& widget)
{
    const int layout = widget.style()->styleHint(QStyle::SH_DialogButtonLayout, nullptr, &widget);
    return layout == QDialogButtonBox::MacLayout || layout == QDialogButtonBox::GnomeLayout;
}

/// The chooser's answer after runModal(): nullopt without OK or a motor, or when @p chooser is
/// gone (destroyed with its window while it was open).
[[nodiscard]] std::optional<UserPrompts::MotorChoice> answerOf(
    const QPointer<MotorChooserDialog>& chooser)
{
    // Through a plain pointer checked once: GCC's -Wnull-dereference does not follow
    // QPointer::isNull() to its operator->().
    const MotorChooserDialog* const dialog = chooser.data();
    if (dialog == nullptr)
    {
        return std::nullopt;
    }
    std::shared_ptr<const ThrustCurveMotor> motor = dialog->getSelectedMotor();
    if (motor == nullptr)
    {
        return std::nullopt;
    }
    return UserPrompts::MotorChoice{.motor              = std::move(motor),
                                    .delay              = dialog->getSelectedDelay(),
                                    .nozzleExitDiameter = dialog->getSelectedNozzleExitDiameter()};
}

/// The chooser of @p window made with @p services, or null.
[[nodiscard]] MotorChooserDialog* existingChooser(const QWidget&     window,
                                                  const GuiServices& services)
{
    const QList<MotorChooserDialog*> choosers =
        window.findChildren<MotorChooserDialog*>(Qt::FindDirectChildrenOnly);
    for (MotorChooserDialog* chooser : choosers)
    {
        if (chooser->uses(services))
        {
            return chooser;
        }
    }
    return nullptr;
}

/// The question to @p chooser: opened for @p mount in @p configurationId, then its answer.
[[nodiscard]] std::optional<UserPrompts::MotorChoice> ask(
    MotorChooserDialog& chooser, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    const QPointer<MotorChooserDialog> guard(&chooser);
    chooser.setMotorMountAndConfig(configurationId, mount);
    chooser.runModal();
    return answerOf(guard);
}

}  // namespace

MotorChooserDialog::MotorChooserDialog(const GuiServices& services, QWidget* parent)
  : QDialog(parent),
    m_services(services),
    m_selectionPanel(makeChild<ThrustCurveMotorSelectionPanel>(this, services))
{
    setObjectName(QStringLiteral("motorChooserDialog"));
    //: MotorChooserDialog.title
    setWindowTitle(tr("Select a rocket motor"));
    // Dialog.ModalityType.APPLICATION_MODAL; closing only hides it (HIDE_ON_CLOSE): it is reused.
    setWindowModality(Qt::ApplicationModal);

    // MigLayout("fill, ins 0"): the panel takes the space, the buttons under it at the right.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    // selectionPanel.setMinimumSize(new Dimension(0, 0)): the dialog shrinks below the panel's
    // size hint (and squeezes it). A positive minimum: Qt's layouts take an explicit minimum of 0
    // as none, and use the minimum size hint instead.
    m_selectionPanel->setMinimumSize(1, 1);
    layout->addWidget(m_selectionPanel, 1);

    // "tag ok, split, align right, gapright unrel, gapbottom unrel" and "tag cancel".
    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, kUnrelatedGap, kUnrelatedGap);
    rowLayout->addStretch(1);
    //: dlg.but.ok
    m_okButton = makeChild<QPushButton>(row, tr("OK"));
    m_okButton->setObjectName(QStringLiteral("okButton"));
    //: dlg.but.cancel
    m_cancelButton = makeChild<QPushButton>(row, tr("Cancel"));
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    // QDialogButtonBox would make OK the default button when it is shown (whatever its
    // autoDefault), so the buttons are laid out here, in the platform's order.
    const bool         cancelBeforeOk = cancelFirst(*this);
    QPushButton* const first          = cancelBeforeOk ? m_cancelButton : m_okButton;
    QPushButton* const second         = cancelBeforeOk ? m_okButton : m_cancelButton;
    rowLayout->addWidget(first);
    rowLayout->addWidget(second);
    QWidget::setTabOrder(first, second);
    layout->addWidget(row);
    // No default button (OpenRocket's dialog has none): Return in the search field, the delay
    // editor or the manufacturer list, which pass it on, must press no button. Every QPushButton
    // in a QDialog is autoDefault, and the dialog makes the first one in the focus chain its
    // default button when it is shown (the filter panel's "Clear All", which Return would then
    // click), or a focused one: none of the chooser's is.
    const QList<QPushButton*> buttons = findChildren<QPushButton*>();
    for (QPushButton* button : buttons)
    {
        button->setAutoDefault(false);
        button->setDefault(false);
    }
    connect(m_okButton, &QPushButton::clicked, this,
            guarded([this] { closeDialog(true); }, context()));
    connect(m_cancelButton, &QPushButton::clicked, this,
            guarded([this] { closeDialog(false); }, context()));
    // The table's double click (Java: dialog.close(true)).
    connect(m_selectionPanel, &ThrustCurveMotorSelectionPanel::okRequested, this,
            guarded([this] { closeDialog(true); }, context()));

    // The search field has the focus first (Java grabs it here, once).
    m_selectionPanel->getDefaultFocus().setFocus();
}

MotorChooserDialog::~MotorChooserDialog() = default;

MotorChooserDialog& MotorChooserDialog::forWindow(QWidget& window, const GuiServices& services)
{
    if (MotorChooserDialog* const chooser = existingChooser(window, services); chooser != nullptr)
    {
        return *chooser;
    }
    return *makeChild<MotorChooserDialog>(&window, services);
}

std::optional<UserPrompts::MotorChoice> MotorChooserDialog::choose(
    QWidget* parent, const GuiServices& services, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    QWidget* const window = parent != nullptr ? parent->window() : nullptr;
    if (window == nullptr)
    {
        // A chooser for this question alone.
        const ModalDialog<MotorChooserDialog> dialog(nullptr, services);
        return ask(*dialog.get(), mount, configurationId);
    }
    MotorChooserDialog* const existing = existingChooser(*window, services);
    if (existing != nullptr && existing->isVisible())
    {
        // Asked from the chooser's own event loop: it cannot be opened twice.
        return std::nullopt;
    }
    return ask(existing != nullptr ? *existing : forWindow(*window, services), mount,
               configurationId);
}

void MotorChooserDialog::setMotorMountAndConfig(const FlightConfigurationId& fcid,
                                                const MotorMount&            mount)
{
    m_selectionPanel->setMotorMountAndConfig(fcid, mount);
}

void MotorChooserDialog::runModal()
{
    m_okClicked = false;
    // Update the motor selection based on the motor table value that was already selected in a
    // previous session.
    m_selectionPanel->selectMotorFromTable();
    // Nothing after exec(): the dialog may be gone when it returns.
    exec();
}

void MotorChooserDialog::closeDialog(bool ok)
{
    if (ok)
    {
        m_selectionPanel->commitEdits();
        accept();
    }
    else
    {
        reject();
    }
}

std::shared_ptr<const ThrustCurveMotor> MotorChooserDialog::getSelectedMotor() const
{
    if (!m_okClicked)
    {
        return nullptr;
    }
    return m_selectionPanel->getSelectedMotor();
}

double MotorChooserDialog::getSelectedDelay() const
{
    return m_selectionPanel->getSelectedDelay();
}

double MotorChooserDialog::getSelectedNozzleExitDiameter() const
{
    return m_selectionPanel->getSelectedNozzleExitDiameter();
}

bool MotorChooserDialog::uses(const GuiServices& services) const noexcept
{
    return &m_services.motors() == &services.motors() &&
           &m_services.preferences() == &services.preferences() &&
           &m_services.prompts() == &services.prompts();
}

void MotorChooserDialog::done(int result)
{
    // Every close sets it: the close button cancels also after an earlier OK (bug 7), where
    // Java's dialog is only hidden and keeps okClicked.
    m_okClicked = result == QDialog::Accepted;
    QDialog::done(result);
    if (const std::shared_ptr<const ThrustCurveMotor> selected = getSelectedMotor();
        selected != nullptr)
    {
        m_selectionPanel->selectedMotor(*selected);
    }
}

}  // namespace QtRocket::Gui
