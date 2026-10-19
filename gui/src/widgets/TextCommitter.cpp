#include "QtRocket/gui/widgets/TextCommitter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QChar>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>
#include <QTextDocument>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/SelectionKeyFilter.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The text of the field @p edit: a line edit's text, a text area's raw text with its paragraph
/// and line separators as '\n' (see the class comment). qobject_cast finds neither while the
/// field is being destroyed (its type is then a QWidget's): an empty text, never read.
[[nodiscard]] QString readText(const QObject& edit)
{
    if (const auto* line = qobject_cast<const QLineEdit*>(&edit))
    {
        return line->text();
    }
    if (const auto* area = qobject_cast<const QPlainTextEdit*>(&edit))
    {
        QString text = area->document()->toRawText();
        text.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
        text.replace(QChar::LineSeparator, QLatin1Char('\n'));
        return text;
    }
    return {};
}

/// Whether @p edit is a whole field: a line edit or a text area, not one being destroyed.
[[nodiscard]] bool isWholeField(const QObject& edit)
{
    return qobject_cast<const QLineEdit*>(&edit) != nullptr ||
           qobject_cast<const QPlainTextEdit*>(&edit) != nullptr;
}

/// Shows @p text in the field @p edit.
void writeText(QWidget& edit, const QString& text)
{
    if (auto* line = qobject_cast<QLineEdit*>(&edit))
    {
        line->setText(text);
    }
    else if (auto* area = qobject_cast<QPlainTextEdit*>(&edit))
    {
        if (readText(*area) != text)
        {
            area->setPlainText(text);
        }
    }
}

/// Whether @p event is Enter in a line edit (Java's action: the plain key).
[[nodiscard]] bool isEnterInLine(const QObject& watched, const QEvent& event)
{
    if (event.type() != QEvent::KeyPress || qobject_cast<const QLineEdit*>(&watched) == nullptr)
    {
        return false;
    }
    const auto* const key = dynamic_cast<const QKeyEvent*>(&event);
    if (key == nullptr)
    {
        return false;
    }
    const Qt::KeyboardModifiers modifiers = key->modifiers() & ~Qt::KeypadModifier;
    return (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
           modifiers == Qt::NoModifier;
}

}  // namespace

TextCommitter::TextCommitter(QObject* parent) : QObject(parent) { }

TextCommitter::~TextCommitter()
{
    for (const Field& field : m_fields)
    {
        if (!field.edit.isNull())
        {
            field.edit->removeEventFilter(this);
        }
    }
}

void TextCommitter::addField(QLineEdit& edit, std::shared_ptr<StringBinding> binding)
{
    addAnyField(edit, std::move(binding));
}

void TextCommitter::addField(QPlainTextEdit& edit, std::shared_ptr<StringBinding> binding)
{
    edit.setTabChangesFocus(true);
    addAnyField(edit, std::move(binding));
}

void TextCommitter::addAnyField(QWidget& edit, std::shared_ptr<StringBinding> binding)
{
    if (binding == nullptr)
    {
        bug("A text field needs a binding");
    }
    removeField(edit);
    if (edit.findChild<SelectionKeyFilter*>(Qt::FindDirectChildrenOnly) == nullptr)
    {
        SelectionKeyFilter::install(edit);
    }
    Field field;
    field.edit            = &edit;
    field.binding         = std::move(binding);
    const QWidget* target = &edit;
    field.connection      = field.binding->addChangeListener(
        [this, target] { Guard::run([this, target] { bindingChanged(target); }); });
    show(field);
    m_fields.push_back(std::move(field));
    edit.installEventFilter(this);
}

void TextCommitter::removeField(const QWidget& edit)
{
    const auto found = std::ranges::find_if(
        m_fields, [&edit](const Field& field) { return field.edit.data() == &edit; });
    if (found == m_fields.end())
    {
        return;
    }
    if (!found->edit.isNull())
    {
        found->edit->removeEventFilter(this);
    }
    m_fields.erase(found);  // its ScopedConnection lets go of the binding
}

bool TextCommitter::hasField(const QWidget& edit) const
{
    return find(&edit) != nullptr;
}

std::shared_ptr<StringBinding> TextCommitter::bindingOf(const QWidget& edit) const
{
    const Field* field = find(&edit);
    return field != nullptr ? field->binding : nullptr;
}

TextCommitter::Field* TextCommitter::find(const QObject* edit)
{
    const auto found = std::ranges::find_if(m_fields, [edit](const Field& field) {
        return !field.edit.isNull() && field.edit.data() == edit;
    });
    return found != m_fields.end() ? &*found : nullptr;
}

const TextCommitter::Field* TextCommitter::find(const QObject* edit) const
{
    const auto found = std::ranges::find_if(m_fields, [edit](const Field& field) {
        return !field.edit.isNull() && field.edit.data() == edit;
    });
    return found != m_fields.end() ? &*found : nullptr;
}

void TextCommitter::show(Field& field)
{
    QWidget& edit = *field.edit;
    writeText(edit, QtStrings::fromUtf8(field.binding->value()));
    field.shown = readText(edit);
    edit.setEnabled(field.binding->isEnabled());
}

void TextCommitter::bindingChanged(const QWidget* edit)
{
    Field* field = find(edit);
    if (field == nullptr || !isWholeField(*field->edit))
    {
        return;
    }
    if (readText(*field->edit) == field->shown)
    {
        show(*field);
    }
    else
    {
        field->edit->setEnabled(field->binding->isEnabled());  // the edit stays
    }
}

bool TextCommitter::commit()
{
    // Every text is read first: a commit's event may show another field's text anew.
    struct Edit
    {
        QPointer<QWidget> edit;
        std::string       text;
    };
    std::vector<Edit> edits;
    for (const Field& field : m_fields)
    {
        if (field.edit.isNull() || !isWholeField(*field.edit))
        {
            continue;
        }
        const QString text = readText(*field.edit);
        if (text != field.shown)
        {
            edits.push_back(Edit{.edit = field.edit, .text = QtStrings::toUtf8(text)});
        }
    }
    bool taken = false;
    for (const Edit& edit : edits)
    {
        const Field* field = find(edit.edit.data());
        if (field == nullptr)
        {
            continue;  // a listener took it out, or it went
        }
        // The binding's listeners may drop the field and with it the last owner of the binding.
        const std::shared_ptr<StringBinding> binding = field->binding;
        taken                                        = binding->setText(edit.text) || taken;
    }
    for (const Edit& edit : edits)
    {
        Field* field = find(edit.edit.data());
        if (field != nullptr && isWholeField(*field->edit))
        {
            show(*field);  // whatever came of it
        }
    }
    return taken;
}

void TextCommitter::guardedCommit(const QObject* edit)
{
    const QString context = edit->objectName();
    Guard::run([this] { static_cast<void>(commit()); }, context);
}

bool TextCommitter::eventFilter(QObject* watched, QEvent* event)
{
    if (find(watched) == nullptr || !isWholeField(*watched))
    {
        return QObject::eventFilter(watched, event);
    }
    // The binding's listeners may delete the field: then the event must stop here.
    const QPointer<QObject> alive(watched);
    if (isEnterInLine(*watched, *event))
    {
        guardedCommit(watched);
        event->accept();
        return true;  // taken: a dialog's default button stays unpressed
    }
    if (event->type() == QEvent::FocusOut)
    {
        guardedCommit(watched);
        if (alive.isNull())
        {
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace QtRocket::Gui
