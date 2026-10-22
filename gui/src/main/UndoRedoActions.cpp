#include "QtRocket/gui/main/UndoRedoActions.h"

#include <optional>
#include <string>

#include <QAction>
#include <QChar>
#include <QKeySequence>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/MenuText.h"
#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

namespace
{

/// Whether the process has reported an undo error (OpenRocketDocument.undoErrorReported). The GUI
/// thread's only.
bool& undoErrorReportedFlag() noexcept
{
    static bool s_reported = false;
    return s_reported;
}

/// UndoRedoAction's name: @p name with the mnemonic @p mnemonic, and " (<description>)" when
/// there is one.
QString actionText(const QString& name, QChar mnemonic,
                   const std::optional<std::string>& description)
{
    QString text = MenuText::withMnemonic(name, mnemonic);
    if (description.has_value())
    {
        text += QStringLiteral(" (") + MenuText::escaped(QtStrings::fromUtf8(*description)) +
                QStringLiteral(")");
    }
    return text;
}

/// What both actions share: the object name, the icon, the keys and the description of
/// BasicFrame's menu item.
void setUp(QAction& action, const QString& name, Icons::Id icon, QKeySequence::StandardKey keys,
           const QString& description)
{
    action.setObjectName(name);
    action.setIcon(Icons::icon(icon));
    action.setShortcuts(keys);
    action.setShortcutContext(Qt::WindowShortcut);
    action.setMenuRole(QAction::NoRole);
    action.setStatusTip(description);
}

}  // namespace

UndoRedoActions::UndoRedoActions(OpenRocketDocument& document, QObject* parent)
  : QObject(parent),
    m_document(&document),
    m_undo(makeChild<QAction>(this)),
    m_redo(makeChild<QAction>(this))
{
    setUp(*m_undo, QStringLiteral("undoAction"), Icons::Id::EDIT_UNDO, QKeySequence::Undo,
          //: main.menu.edit.undo.desc
          tr("Undo the previous operation"));
    setUp(*m_redo, QStringLiteral("redoAction"), Icons::Id::EDIT_REDO, QKeySequence::Redo,
          //: main.menu.edit.redo.desc
          tr("Redo the previously undone operation"));
    connect(m_undo, &QAction::triggered, this,
            guarded([this] { perform(true); }, QStringLiteral("Undo")));
    connect(m_redo, &QAction::triggered, this,
            guarded([this] { perform(false); }, QStringLiteral("Redo")));

    m_undoRedoChanged = m_document->undoRedoChanged().connect(
        guarded([this] { refresh(); }, QStringLiteral("Undo and redo")));
    m_undoError = m_document->undoErrorOccurred().connect(guarded(
        [this](const std::string& message) { reportUndoError(QtStrings::fromUtf8(message)); },
        QStringLiteral("Undo and redo")));
    refresh();
}

void UndoRedoActions::refresh()
{
    m_undo->setText(undoText(m_document->getUndoDescription()));
    m_undo->setEnabled(m_document->isUndoAvailable());
    m_redo->setText(redoText(m_document->getRedoDescription()));
    m_redo->setEnabled(m_document->isRedoAvailable());
}

QString UndoRedoActions::undoText(const std::optional<std::string>& description)
{
    //: OpenRocketDocument.Undo
    return actionText(tr("Undo"), QChar(u'U'), description);
}

QString UndoRedoActions::redoText(const std::optional<std::string>& description)
{
    //: OpenRocketDocument.Redo
    return actionText(tr("Redo"), QChar(u'R'), description);
}

bool UndoRedoActions::undoErrorReported() noexcept
{
    return undoErrorReportedFlag();
}

void UndoRedoActions::resetUndoErrorReported() noexcept
{
    undoErrorReportedFlag() = false;
}

void UndoRedoActions::perform(bool undo)
{
    Q_EMIT aboutToUndoOrRedo();
    // An edit committed now is a change of the design, which removes the redo history: the redo
    // is not there any more (the document would report an undo error), and the edit stays as the
    // latest change.
    if (undo ? !m_document->isUndoAvailable() : !m_document->isRedoAvailable())
    {
        return;
    }
    if (undo)
    {
        m_document->undo();
    }
    else
    {
        m_document->redo();
    }
}

void UndoRedoActions::reportUndoError(const QString& message)
{
    if (undoErrorReportedFlag())
    {
        return;
    }
    // Later, from the event loop: the error may come in the middle of a rocket event, and the
    // hook may open a dialog.
    QMetaObject::invokeMethod(
        this,
        [message] {
            if (!undoErrorReportedFlag())
            {
                undoErrorReportedFlag() = true;
                Guard::reportErrorCondition(message);
            }
        },
        Qt::QueuedConnection);
}

}  // namespace QtRocket::Gui
