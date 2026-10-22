#pragma once

#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include <QPointer>

#include "QtRocket/gui/binding/Binding.h"
#include "QtRocket/util/BugError.h"

class QWidget;

namespace QtRocket::Gui
{

class ComponentSelection;

/// The bindings of one dialog (a component configuration panel) and their lifetime: it owns the
/// bindings the dialog makes and the connections it adds, commits its fields' pending edits, and
/// ends it all before the components can go (OpenRocket: the panel's list of Invalidatables,
/// RocketComponentConfig.register() and invalidateModels(), and ModelInvalidator).
///
/// - make<B>() makes a binding with std::make_shared (which the bindings' keep-alive needs) and
///   keeps it; keep() keeps one made elsewhere (makeTransitionShapeBinding()); keepConnection()
///   keeps a connection the dialog adds (a listener of a binding or of the selection) until
///   release() disconnects it. The widgets hold their bindings too, so a binding lives as long
///   as the scope or its widgets, whichever goes last.
/// - commitAll() commits the pending edit of every field under the root widget: each UnitSpinBox
///   and IntSpinBox (commitEdit()) and each TextCommitter (commit()). The dialog calls it before
///   OK, Close, a save and an undo: a button that takes no focus (macOS) leaves the edit pending,
///   where Java's focus loss would have committed it. What a commit throws is reported (Guard,
///   with the field's objectName()), and the next field commits all the same.
/// - release() ends the scope: the connections kept are disconnected, the selection is released
///   (ComponentSelection::release(): every binding of it drops each write a widget makes from
///   then on, so the focus loss of a closing dialog's field reaches no component) and the bindings
///   are let go. The dialog calls it before it goes and before its components can go without an
///   event of the rocket; the destructor calls it too.
///
/// The scope holds no component pointer of its own: the selection drops its pointers inside the
/// TREE or UNDO change that ends it (ComponentSelection), and from then on commitAll() commits
/// nothing and isReleased() is true; the dialog, which closes on Phase::INVALIDATED, calls
/// release() as it goes. (Releasing inside that event would disconnect the dialog's own listener
/// before it is told.)
///
/// GUI thread only; not copyable or movable.
class BindingScope final
{
public:
    /// A scope of the bindings of @p selection (null for a dialog that edits no component: its
    /// bindings hold values of their own), committing the fields under @p root, which must
    /// outlive it or go first (the scope then commits nothing).
    BindingScope(QWidget& root, std::shared_ptr<ComponentSelection> selection);
    ~BindingScope();
    BindingScope(const BindingScope&)            = delete;
    BindingScope& operator=(const BindingScope&) = delete;
    BindingScope(BindingScope&&)                 = delete;
    BindingScope& operator=(BindingScope&&)      = delete;

    /// The selection of the dialog's bindings; null for a dialog that edits no component. Kept
    /// after release() (released, it is invalid).
    [[nodiscard]] const std::shared_ptr<ComponentSelection>& selection() const noexcept
    {
        return m_selection;
    }

    /// Makes a B with std::make_shared<B>(@p args...) and keeps it until release().
    /// @throws BugError once the scope is released
    template <std::derived_from<Binding> B, class... Args>
        requires std::constructible_from<B, Args...>
    std::shared_ptr<B> make(Args&&... args)
    {
        return keep(std::make_shared<B>(std::forward<Args>(args)...));
    }

    /// Keeps @p binding until release(); gives it back.
    /// @throws BugError when @p binding is null or the scope is released
    template <std::derived_from<Binding> B>
    std::shared_ptr<B> keep(std::shared_ptr<B> binding)
    {
        if (binding == nullptr)
        {
            bug("A binding scope keeps bindings, not null");
        }
        checkNotReleased();
        m_bindings.push_back(binding);
        return binding;
    }

    /// Keeps @p connection (a Signal's Connection or ScopedConnection) connected until release(),
    /// which disconnects it.
    /// @throws BugError once the scope is released
    template <class Connection>
        requires requires(Connection connection) { connection.disconnect(); }
    void keepConnection(Connection connection)
    {
        checkNotReleased();
        m_connections.push_back(
            std::make_unique<HeldConnection<Connection>>(std::move(connection)));
    }

    /// The number of bindings kept.
    [[nodiscard]] std::size_t bindingCount() const noexcept { return m_bindings.size(); }

    /// Commits the pending edit of every field under the root (see the class comment): true when
    /// a binding took a value. Nothing once the scope is released, its selection is invalid or its
    /// root has gone.
    bool commitAll();

    /// Ends the scope (see the class comment). Nothing when it is released already.
    void release() noexcept;

    /// Whether release() ended the scope or a TREE or UNDO change ended its selection.
    [[nodiscard]] bool isReleased() const noexcept;

private:
    /// A connection of any Signal, disconnected when destroyed.
    struct Held
    {
        Held()                       = default;
        virtual ~Held()              = default;
        Held(const Held&)            = delete;
        Held& operator=(const Held&) = delete;
        Held(Held&&)                 = delete;
        Held& operator=(Held&&)      = delete;
    };

    template <class Connection>
    struct HeldConnection final : Held
    {
        explicit HeldConnection(Connection held) : connection(std::move(held)) { }
        ~HeldConnection() override { connection.disconnect(); }
        HeldConnection(const HeldConnection&)            = delete;
        HeldConnection& operator=(const HeldConnection&) = delete;
        HeldConnection(HeldConnection&&)                 = delete;
        HeldConnection& operator=(HeldConnection&&)      = delete;

        Connection connection;
    };

    void checkNotReleased() const;

    QPointer<QWidget>                     m_root;
    std::shared_ptr<ComponentSelection>   m_selection;
    std::vector<std::shared_ptr<Binding>> m_bindings;
    std::vector<std::unique_ptr<Held>>    m_connections;
    bool                                  m_released{false};
};

}  // namespace QtRocket::Gui
