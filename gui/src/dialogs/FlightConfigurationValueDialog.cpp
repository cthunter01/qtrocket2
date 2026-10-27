#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"

#include <algorithm>

#include <QButtonGroup>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

FlightConfigurationValueDialog::FlightConfigurationValueDialog(
    const QString& windowTitle, const ScopeTexts& texts, const QString& configurationName,
    UserPrompts::ConfigurationScope scope, QWidget* parent)
  : QDialog(parent),
    m_layout(makeChild<QVBoxLayout>(this)),
    m_scopeLabel(makeChild<QLabel>(this, texts.title)),
    m_scopeButtons(makeChild<QWidget>(this)),
    // An '&' of the texts (a configuration named "A&B") is shown as it is, as by Swing.
    m_defaultButton(
        makeChild<QRadioButton>(m_scopeButtons, MenuText::escaped(texts.allConfigurations))),
    m_overrideButton(makeChild<QRadioButton>(
        m_scopeButtons,
        MenuText::escaped(
            QString(texts.thisConfiguration).replace(QStringLiteral("{0}"), configurationName)))),
    m_values(makeChild<QWidget>(this)),
    m_valueGrid(makeChild<QGridLayout>(m_values)),
    m_bindings(*this, nullptr)
{
    setWindowTitle(windowTitle);
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);

    // "span, wrap rel", then the radio buttons ("span, gapleft para, wrap rel" and "..., wrap
    // para"); the button group makes them exclusive.
    m_scopeLabel->setObjectName(QStringLiteral("scopeLabel"));
    m_layout->addWidget(m_scopeLabel);
    m_scopeButtons->setObjectName(QStringLiteral("scopeButtons"));
    auto* buttonsLayout = makeChild<QVBoxLayout>(m_scopeButtons);
    buttonsLayout->setContentsMargins(kParagraphGap, 0, 0, 0);
    auto* group = makeChild<QButtonGroup>(this);
    group->addButton(m_defaultButton);
    group->addButton(m_overrideButton);
    m_defaultButton->setObjectName(QStringLiteral("defaultButton"));
    buttonsLayout->addWidget(m_defaultButton);
    m_overrideButton->setObjectName(QStringLiteral("overrideButton"));
    buttonsLayout->addWidget(m_overrideButton);
    (scope == UserPrompts::ConfigurationScope::THIS_CONFIGURATION ? m_overrideButton
                                                                  : m_defaultButton)
        ->setChecked(true);
    m_layout->addWidget(m_scopeButtons);
    m_layout->addSpacing(kParagraphGap);

    m_values->setObjectName(QStringLiteral("values"));
    m_valueGrid->setContentsMargins(0, 0, 0, 0);
    m_layout->addWidget(m_values);
}

FlightConfigurationValueDialog::~FlightConfigurationValueDialog()
{
    // The subclass is gone: its listeners (updateState()) are disconnected before ~QDialog hides
    // the dialog, whose focus loss can still commit a spin box's edit into its binding.
    m_bindings.release();
}

UserPrompts::ConfigurationScope FlightConfigurationValueDialog::scope() const noexcept
{
    return m_overrideButton->isChecked() ? UserPrompts::ConfigurationScope::THIS_CONFIGURATION
                                         : UserPrompts::ConfigurationScope::ALL_CONFIGURATIONS;
}

QPushButton& FlightConfigurationValueDialog::okButton() const
{
    if (m_ok == nullptr)
    {
        bug("A value dialog's buttons are added by its constructor");
    }
    return *m_ok;
}

QPushButton& FlightConfigurationValueDialog::cancelButton() const
{
    if (m_cancel == nullptr)
    {
        bug("A value dialog's buttons are added by its constructor");
    }
    return *m_cancel;
}

void FlightConfigurationValueDialog::addButtons()
{
    if (m_ok != nullptr)
    {
        bug("A value dialog's buttons are added once");
    }
    // The values' last row ends with "wrap unrel" or "wrap para"; then "span, split, growx" (an
    // empty panel that pushes the buttons to the right) and the two buttons of one size group.
    m_layout->addSpacing(kParagraphGap);
    auto* row       = makeChild<QWidget>(this);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    //: button.ok
    m_ok = makeChild<QPushButton>(row, tr("OK"));
    m_ok->setObjectName(QStringLiteral("okButton"));
    m_ok->setDefault(true);
    rowLayout->addWidget(m_ok);
    //: button.cancel
    m_cancel = makeChild<QPushButton>(row, tr("Cancel"));
    m_cancel->setObjectName(QStringLiteral("cancel"));
    rowLayout->addWidget(m_cancel);
    const int width = std::max(m_ok->sizeHint().width(), m_cancel->sizeHint().width());
    m_ok->setMinimumWidth(width);
    m_cancel->setMinimumWidth(width);
    m_layout->addWidget(row);

    connect(m_ok, &QPushButton::clicked, this, guarded([this] { ok(); }, objectName()));
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    // pack().
    adjustSize();
}

void FlightConfigurationValueDialog::ok()
{
    // A button that takes no focus (macOS), and Escape, leave a spin box's edit pending.
    static_cast<void>(m_bindings.commitAll());
    accept();
}

void FlightConfigurationValueDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Cancel))
    {
        event->accept();
        Guard::run([this] { ok(); }, objectName());
        return;
    }
    QDialog::keyPressEvent(event);
}

}  // namespace QtRocket::Gui
