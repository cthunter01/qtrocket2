#pragma once

#include <optional>
#include <string>

#include <QObject>
#include <QString>

#include "QtRocket/util/Signal.h"

class QAction;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

/// The Undo and Redo actions of a design window's Edit menu (OpenRocket's UndoRedoAction, one
/// object for both). Each shows what it would do, "Undo" or "Undo (<description>)" (the texts of
/// "OpenRocketDocument.Undo" and "OpenRocketDocument.Redo" and the document's undo and redo
/// descriptions), is enabled when the document can undo or redo, and undoes or redoes when
/// triggered. As in Java the texts and states are set when the actions are made and again
/// whenever the document says they may have changed (undoRedoChanged(), Java's
/// UndoRedoListener.setAllValues()), and at no other time. The keys are the platform's Undo and
/// Redo keys, window shortcuts (they act anywhere in the window, but a focused text field keeps
/// them for its own undo), and the mnemonics are BasicFrame's (U, R).
///
/// The document's undo errors (OpenRocketDocument::undoErrorOccurred()) are reported through the
/// error hook (Guard::reportErrorCondition()), the first one of the process only, as OpenRocket's
/// document reports only its first one per run (OpenRocketDocument.undoErrorReported).
///
/// Deviations: the keys are the platform's standard keys (QKeySequence::Undo and Redo: Redo is
/// Ctrl+Shift+Z on Linux and Cmd+Shift+Z on macOS, Ctrl+Y on Windows) where BasicFrame binds
/// Menu+Y everywhere; an '&' of a description is shown as it is (MenuText::escaped()); an undo
/// error is reported once the event loop runs again, not in the middle of the document's event
/// (an error dialog there would run an event loop inside a rocket event); the edits still being
/// typed are committed first (aboutToUndoOrRedo()), so that an action finds nothing more to do
/// when that commit removed the redo history (decision of plan 9.3, for the macOS menu bar, which
/// takes no focus; OpenRocket's spinner fields commit on a permanent focus loss only, so a number
/// still being typed is lost in its undo or redo).
class UndoRedoActions final : public QObject
{
    Q_OBJECT

public:
    /// The actions of @p document, which must outlive this object.
    UndoRedoActions(OpenRocketDocument& document, QObject* parent);

    /// The Undo action ("undoAction").
    [[nodiscard]] QAction* undoAction() const noexcept { return m_undo; }
    /// The Redo action ("redoAction").
    [[nodiscard]] QAction* redoAction() const noexcept { return m_redo; }

    /// UndoRedoAction.setAllValues() of both actions: their texts and enabled states from the
    /// document as it is now.
    void refresh();

    /// The text of the Undo action for an undo step with @p description (none: nullopt):
    /// "&Undo" or "&Undo (<description>)".
    [[nodiscard]] static QString undoText(const std::optional<std::string>& description);
    /// The text of the Redo action: "&Redo" or "&Redo (<description>)".
    [[nodiscard]] static QString redoText(const std::optional<std::string>& description);

    /// Whether an undo error has been reported in this process.
    [[nodiscard]] static bool undoErrorReported() noexcept;
    /// Forgets that an undo error was reported, so that the next one is reported again; for the
    /// tests, which must leave the process as they found it.
    static void resetUndoErrorReported() noexcept;

Q_SIGNALS:
    /// Emitted right before an action undoes or redoes, so that edits still being typed can be
    /// committed first (the configuration dialog's: ConfigDialogRequests connects
    /// ComponentConfigDialog::commitPendingEdits()). An edit committed then is a change of the
    /// design, which removes the redo history: the action then does nothing more (a redo is not
    /// there any more; the edit stays the latest change).
    void aboutToUndoOrRedo();

private:
    /// Undoes (@p undo) or redoes.
    void perform(bool undo);
    /// Reports @p message as the process's undo error, unless one has been reported.
    void reportUndoError(const QString& message);

    OpenRocketDocument*                          m_document;
    QAction*                                     m_undo;
    QAction*                                     m_redo;
    Signal<>::ScopedConnection                   m_undoRedoChanged;
    Signal<const std::string&>::ScopedConnection m_undoError;
};

}  // namespace QtRocket::Gui
