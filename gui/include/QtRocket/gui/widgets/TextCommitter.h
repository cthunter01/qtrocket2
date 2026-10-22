#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QPointer>
#include <QString>

#include "QtRocket/util/Signal.h"

class QEvent;
class QLineEdit;
class QPlainTextEdit;
class QWidget;

namespace QtRocket::Gui
{

class StringBinding;

/// Commits the text fields of a dialog to their StringBindings (OpenRocket: the private
/// TextFieldListener of RocketComponentConfig, swing gui/configdialog/RocketComponentConfig.java
/// 1025-1047, on the component name field and the comment area; that of RocketConfig.java:209-232
/// on the designer, revision and kit name areas; and the public gui/TextFieldListener, whose
/// behaviour TextFieldListenerTest pins).
///
/// A committer has fields: line edits (the name) and text areas (the comment). Each shows its
/// binding's text when it is added. A commit of any of them commits them all, as Java's one
/// listener does (setName() sets the name and the comment): every field whose text was edited
/// since it showed its binding's text gives that text to its binding (StringBinding::setText(),
/// which sets it on every selected component unless it is the main component's already, Java's
/// equals() check). Afterwards, whatever came of it, each such field shows its binding's text
/// again (a blank name becomes the component's default name; a binding that dropped the text, its
/// selection gone, shows its last text).
///
/// A commit comes from:
/// - Enter in a line edit (Java: actionPerformed()). The key is taken, so that a dialog's default
///   button stays unpressed (Java's text field takes Enter while it has an action listener). In a
///   text area Enter is a new line, as in Java.
/// - Any focus loss of a field (Java: focusLost(), also a temporary one), save the one to the
///   field's own context menu (Qt's rule for QLineEdit::editingFinished()): the edit stays pending
///   while the user picks Paste, until the field loses the focus for good, Enter or commit().
///   Java's text fields have no context menu, so a pending stage name (a TREE change, which closes
///   the dialog) must not be committed under it. A popup elsewhere (a unit selector's menu, a combo
///   box's list) commits, as in Java, whose focusable popups take the focus from the field
///   (a temporary focus loss, which RocketComponentConfig's listener does not tell apart). A
///   focus gain commits nothing (Java: focusGained() does nothing).
/// - commit() (BindingScope::commitAll(), before OK, Close, a save or an undo: a button that takes
///   no focus, as on macOS, leaves the edit pending).
///
/// Esc is left to the dialog in a line edit and a text area (Java: neither handles it; the
/// component dialog then closes, and the focus loss commits). Each field gets the
/// SelectionKeyFilter (Java: TextComponentSelectionKeyListener on the name field and the comment
/// area); a text area moves the focus on Tab (Java: GUIUtil.setTabToFocusing()).
///
/// When a binding tells of a change, its field shows the new text, unless the field holds an edit
/// in progress (the config-dialogs scout's rule). Once the binding's selection is invalid, the
/// field is disabled.
///
/// A field is taken out with removeField() (Java: listenTo() of another field, or of null): it
/// then neither commits nor is committed. A field that is destroyed is left out; a committer that
/// goes first takes itself off its fields. The committer reads a field only while it is whole: a
/// focus loss that a field's destructor sends (QWidget::~QWidget clears the focus, after the line
/// edit's own part is gone) commits nothing.
///
/// Deviations from OpenRocket:
/// - Java's updateFields() writes the component's name into the name field on every change of the
///   rocket, an edit in progress too, and never the comment into the comment area; here both
///   fields show a changed text unless they hold an edit.
/// - Every text area gets the SelectionKeyFilter (Java's designer, revision and kit name areas
///   have no TextComponentSelectionKeyListener).
/// - A field commits only an edited text (Java's listener compares the field's text with the
///   component's on every commit; the same unless the component changed meanwhile, which here
///   re-shows an unedited field). A text area's text is its document's raw text with its paragraph
///   and line separators as '\n', so that a no-break space survives (QPlainTextEdit's
///   toPlainText() would make it a space).
class TextCommitter : public QObject
{
    Q_OBJECT

public:
    explicit TextCommitter(QObject* parent);
    ~TextCommitter() override;
    TextCommitter(const TextCommitter&)            = delete;
    TextCommitter& operator=(const TextCommitter&) = delete;
    TextCommitter(TextCommitter&&)                 = delete;
    TextCommitter& operator=(TextCommitter&&)      = delete;

    /// Makes @p edit a field of @p binding (see the class comment) and shows its text; a field
    /// added again takes the new binding.
    /// @throws BugError when @p binding is null
    void addField(QLineEdit& edit, std::shared_ptr<StringBinding> binding);
    void addField(QPlainTextEdit& edit, std::shared_ptr<StringBinding> binding);

    /// Takes @p edit out (see the class comment); nothing when it is no field.
    void removeField(const QWidget& edit);

    /// Whether @p edit is a field of the committer.
    [[nodiscard]] bool hasField(const QWidget& edit) const;

    /// The binding of the field @p edit; null when it is none.
    [[nodiscard]] std::shared_ptr<StringBinding> bindingOf(const QWidget& edit) const;

    /// Commits every edited field (see the class comment): true when a binding took a text.
    bool commit();

    /// Commits on Enter in a line edit and on the focus loss of a field.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Field
    {
        QPointer<QWidget>              edit;
        std::shared_ptr<StringBinding> binding;
        Signal<>::ScopedConnection     connection;
        /// The text the field showed last, as the field reads it.
        QString shown;
    };

    void                       addAnyField(QWidget& edit, std::shared_ptr<StringBinding> binding);
    [[nodiscard]] Field*       find(const QObject* edit);
    [[nodiscard]] const Field* find(const QObject* edit) const;
    /// Shows the binding's text in @p field.
    static void show(Field& field);
    /// The binding told of a change: @p edit shows it unless it holds an edit.
    void bindingChanged(const QWidget* edit);
    /// commit() with what it throws reported (an event handler's call into the core).
    void guardedCommit(const QObject* edit);

    std::vector<Field> m_fields;
};

}  // namespace QtRocket::Gui
