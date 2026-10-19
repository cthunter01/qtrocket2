#include "QtRocket/gui/settings/SettingsPreferences.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QByteArrayView>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMetaType>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/gui/util/BuildInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// QSettings whose event handler runs under the store's mutex: QSettings writes the file there
/// after a change (its deferred sync, an UpdateRequest event it posts to itself), on the thread
/// it lives in, while another thread may be in a store operation.
class LockedSettings final : public QSettings
{
public:
    LockedSettings(const QString& fileName, std::mutex& mutex)
      : QSettings(fileName, QSettings::IniFormat), m_mutex(&mutex)
    {
    }

protected:
    bool event(QEvent* event) override
    {
        const std::scoped_lock lock(*m_mutex);
        return QSettings::event(event);
    }

private:
    std::mutex* m_mutex;
};

/// Enters the group @p group (an absolute path; empty for the top) of @p settings for its
/// lifetime. QSettings starts at the top and every scope ends before the next begins.
class GroupScope
{
public:
    GroupScope(QSettings& settings, const QString& group)
      : m_settings(&settings), m_entered(!group.isEmpty())
    {
        if (m_entered)
        {
            settings.beginGroup(group);
        }
    }
    ~GroupScope()
    {
        if (m_entered)
        {
            m_settings->endGroup();
        }
    }
    GroupScope(const GroupScope&)            = delete;
    GroupScope& operator=(const GroupScope&) = delete;
    GroupScope(GroupScope&&)                 = delete;
    GroupScope& operator=(GroupScope&&)      = delete;

private:
    QSettings* m_settings;
    bool       m_entered;
};

/// The name of the empty key or node in the file (a '%' that escapes nothing).
constexpr std::string_view kEmptyName = "%";

/// The escapes of the three characters QSettings does not keep in a name, all as long.
constexpr std::size_t      kEscapeLength     = 3;
constexpr std::string_view kEscapedPercent   = "%25";
constexpr std::string_view kEscapedSlash     = "%2F";
constexpr std::string_view kEscapedBackslash = "%5C";

/// @p name as it is stored in the file (see the class comment).
[[nodiscard]] QString encodeName(std::string_view name)
{
    if (name.empty())
    {
        return QtStrings::fromUtf8(kEmptyName);
    }
    std::string escaped;
    escaped.reserve(name.size());
    for (const char c : name)
    {
        switch (c)
        {
            case '%':
                escaped += kEscapedPercent;
                break;
            case '/':
                escaped += kEscapedSlash;
                break;
            case '\\':
                escaped += kEscapedBackslash;
                break;
            default:
                escaped += c;
                break;
        }
    }
    return QtStrings::fromUtf8(escaped);
}

/// The character the escape at the start of @p text stands for, or nullopt when @p text starts
/// with no escape of encodeName().
[[nodiscard]] std::optional<char> escapedCharacter(std::string_view text)
{
    if (text.starts_with(kEscapedPercent))
    {
        return '%';
    }
    if (text.starts_with(kEscapedSlash))
    {
        return '/';
    }
    if (text.starts_with(kEscapedBackslash))
    {
        return '\\';
    }
    return std::nullopt;
}

/// The name @p stored (a name of the file) stands for: encodeName() undone. nullopt for a name
/// encodeName() does not make (a '%' that starts no escape, in a file written by hand): it
/// would not be found again under the name it decodes to, so the store does not show it.
[[nodiscard]] std::optional<std::string> decodeName(const QString& stored)
{
    const std::string text = QtStrings::toUtf8(stored);
    if (text == kEmptyName)
    {
        return std::string();
    }
    std::string name;
    name.reserve(text.size());
    std::string_view rest = text;
    while (!rest.empty())
    {
        if (rest.front() != '%')
        {
            name += rest.front();
            rest.remove_prefix(1);
            continue;
        }
        const std::optional<char> escaped = escapedCharacter(rest);
        if (!escaped.has_value())
        {
            return std::nullopt;
        }
        name += *escaped;
        rest.remove_prefix(kEscapeLength);
    }
    return name;
}

/// Whether @p name can name a child node: a node name is not empty and has no '/' (the path
/// separator), which a section of a file written by hand may have.
[[nodiscard]] bool isNodeName(std::string_view name)
{
    return !name.empty() && !name.contains('/');
}

/// @p value as it is stored: UTF-8 text as a string, anything else as bytes.
[[nodiscard]] QVariant storedValue(std::string_view value)
{
    const QByteArrayView bytes(value);
    if (bytes.isValidUtf8())
    {
        return QString::fromUtf8(bytes);
    }
    return bytes.toByteArray();
}

/// The text of the stored @p value (storedValue() undone).
[[nodiscard]] std::string valueText(const QVariant& value)
{
    if (value.metaType() == QMetaType::fromType<QByteArray>())
    {
        return value.toByteArray().toStdString();
    }
    if (value.metaType() == QMetaType::fromType<QStringList>())
    {
        return QtStrings::toUtf8(value.toStringList().join(u','));
    }
    return QtStrings::toUtf8(value.toString());
}

/// Removes the key @p name of the current group of @p settings and nothing else: QSettings'
/// remove() also removes a group of the same name, which is put back.
void removeKeyKeepingGroup(QSettings& settings, const QString& name)
{
    if (!settings.childGroups().contains(name))
    {
        settings.remove(name);
        return;
    }
    std::vector<std::pair<QString, QVariant>> kept;
    settings.beginGroup(name);
    for (const QString& key : settings.allKeys())
    {
        kept.emplace_back(key, settings.value(key));
    }
    settings.endGroup();
    settings.remove(name);
    for (const auto& [key, value] : kept)
    {
        settings.setValue(name + u'/' + key, value);
    }
}

/// Copies the keys and the child nodes of @p source into @p target, recursively, through the
/// interface of Preferences (one operation at a time).
void copyTree(const Preferences& source, InMemoryPreferences& target)
{
    for (const std::string& key : source.keys())
    {
        const std::optional<std::string> value = source.get(key);
        if (value.has_value())
        {
            target.put(key, *value);
        }
    }
    for (const std::string& name : source.childrenNames())
    {
        const Preferences* const child = source.findNode(name);
        if (child != nullptr && isNodeName(name))
        {
            copyTree(*child, target.getNode(name));
        }
    }
}

}  // namespace

/// What every node of a store shares: the one QSettings object (whose group state is the
/// store's), and the mutex that guards it and the flags and the child lists of the node objects.
struct SettingsPreferences::Store
{
    explicit Store(const QString& fileName) : settings(fileName, mutex) { }

    std::mutex     mutex;
    LockedSettings settings;
    /// The theme of the application's palette, as last seen on the application's thread.
    std::atomic<ThemeColors::Theme> theme{ThemeColors::Theme::LIGHT};
};

SettingsPreferences::SettingsPreferences(const QString& fileName)
  : m_ownStore(std::make_unique<Store>(fileName)), m_store(m_ownStore.get()), m_made(true)
{
    // The theme as the application's thread sees it now, for the other threads.
    static_cast<void>(SettingsPreferences::defaultComponentColors());
}

SettingsPreferences::SettingsPreferences(ChildKey /*key*/, Store& store, QString group)
  : m_store(&store), m_group(std::move(group)), m_made(false)
{
}

SettingsPreferences::~SettingsPreferences() = default;

QString SettingsPreferences::defaultFileName()
{
    const QSettings place(QSettings::IniFormat, QSettings::UserScope, BuildInfo::organizationName(),
                          BuildInfo::applicationName());
    return place.fileName();
}

std::unique_ptr<SettingsPreferences> SettingsPreferences::openDefault()
{
    return std::make_unique<SettingsPreferences>(defaultFileName());
}

// ------------------------------------------------------------------------------- the store

std::optional<std::string> SettingsPreferences::get(std::string_view key) const
{
    const std::scoped_lock lock(m_store->mutex);
    return getLocked(key);
}

std::optional<std::string> SettingsPreferences::getLocked(std::string_view key) const
{
    QSettings&       settings = m_store->settings;
    const GroupScope group(settings, m_group);
    const QString    name = encodeName(key);
    if (!settings.contains(name))
    {
        return std::nullopt;
    }
    return valueText(settings.value(name));
}

void SettingsPreferences::put(std::string_view key, std::string_view value)
{
    // The Qt strings of the call are made and released under the lock too: QSettings keeps
    // shared copies of them, which another thread releases later (under the lock), so that every
    // release of their data comes after the others in the order of the lock.
    const std::scoped_lock lock(m_store->mutex);
    putLocked(key, value);
}

void SettingsPreferences::putLocked(std::string_view key, std::string_view value)
{
    QSettings&       settings = m_store->settings;
    const GroupScope group(settings, m_group);
    settings.setValue(encodeName(key), storedValue(value));
}

void SettingsPreferences::remove(std::string_view key)
{
    const std::scoped_lock lock(m_store->mutex);
    removeLocked(key);
}

void SettingsPreferences::removeLocked(std::string_view key)
{
    QSettings&       settings = m_store->settings;
    const GroupScope group(settings, m_group);
    const QString    name = encodeName(key);
    if (settings.contains(name))
    {
        removeKeyKeepingGroup(settings, name);
    }
}

void SettingsPreferences::clear()
{
    const std::scoped_lock lock(m_store->mutex);
    QSettings&             settings = m_store->settings;
    const GroupScope       group(settings, m_group);
    for (const QString& name : settings.childKeys())
    {
        removeKeyKeepingGroup(settings, name);
    }
}

std::vector<std::string> SettingsPreferences::keys() const
{
    const std::scoped_lock   lock(m_store->mutex);
    QSettings&               settings = m_store->settings;
    const GroupScope         group(settings, m_group);
    std::vector<std::string> names;
    for (const QString& stored : settings.childKeys())
    {
        std::optional<std::string> name = decodeName(stored);
        if (name.has_value())
        {
            names.push_back(std::move(*name));
        }
    }
    std::ranges::sort(names);
    return names;
}

std::vector<std::string> SettingsPreferences::childrenNames() const
{
    const std::scoped_lock lock(m_store->mutex);
    return childrenNamesLocked();
}

std::vector<std::string> SettingsPreferences::childrenNamesLocked() const
{
    std::vector<std::string> names;
    {
        QSettings&       settings = m_store->settings;
        const GroupScope group(settings, m_group);
        for (const QString& stored : settings.childGroups())
        {
            std::optional<std::string> name = decodeName(stored);
            if (name.has_value() && isNodeName(*name))
            {
                names.push_back(std::move(*name));
            }
        }
    }
    for (const auto& [name, child] : m_children)
    {
        if (child->m_made)
        {
            names.push_back(name);
        }
    }
    std::ranges::sort(names);
    const auto [first, last] = std::ranges::unique(names);
    names.erase(first, last);
    return names;
}

SettingsPreferences& SettingsPreferences::childLocked(std::string_view name) const
{
    auto it = m_children.find(name);
    if (it == m_children.end())
    {
        QString group = m_group.isEmpty() ? encodeName(name) : m_group + u'/' + encodeName(name);
        it            = m_children
                            .emplace(std::string(name), std::make_unique<SettingsPreferences>(
                                                            ChildKey{}, *m_store, std::move(group)))
                            .first;
    }
    return *it->second;
}

bool SettingsPreferences::existsLocked() const
{
    if (m_made)
    {
        return true;
    }
    QSettings&       settings = m_store->settings;
    const GroupScope group(settings, m_group);
    return !settings.childKeys().isEmpty() || !settings.childGroups().isEmpty();
}

SettingsPreferences& SettingsPreferences::getNode(std::string_view name)
{
    // java.util.prefs: a relative path, one node name per '/'-separated segment.
    const std::scoped_lock lock(m_store->mutex);
    SettingsPreferences*   node = this;
    std::string_view       rest = name;
    while (true)
    {
        const std::size_t      slash = rest.find('/');
        const std::string_view head  = rest.substr(0, slash);
        QTROCKET_ASSERT(!head.empty());
        node         = &node->childLocked(head);
        node->m_made = true;
        if (slash == std::string_view::npos)
        {
            return *node;
        }
        rest = rest.substr(slash + 1);
    }
}

const SettingsPreferences* SettingsPreferences::findNode(std::string_view name) const noexcept
{
    const std::scoped_lock lock(m_store->mutex);
    return findNodeLocked(name);
}

const SettingsPreferences* SettingsPreferences::findNodeLocked(std::string_view name) const
{
    const SettingsPreferences* node = this;
    std::string_view           rest = name;
    while (true)
    {
        const std::size_t      slash = rest.find('/');
        const std::string_view head  = rest.substr(0, slash);
        if (head.empty())
        {
            return nullptr;
        }
        node = &node->childLocked(head);
        if (!node->existsLocked())
        {
            return nullptr;
        }
        if (slash == std::string_view::npos)
        {
            return node;
        }
        rest = rest.substr(slash + 1);
    }
}

// ---------------------------------------------------------------- snapshot, restore, reset

std::unique_ptr<InMemoryPreferences> SettingsPreferences::snapshot() const
{
    auto                   copy = std::make_unique<InMemoryPreferences>();
    const std::scoped_lock lock(m_store->mutex);
    snapshotLocked(*copy);
    return copy;
}

void SettingsPreferences::snapshotLocked(InMemoryPreferences& target) const
{
    {
        QSettings&       settings = m_store->settings;
        const GroupScope group(settings, m_group);
        for (const QString& stored : settings.childKeys())
        {
            const std::optional<std::string> key = decodeName(stored);
            if (key.has_value())
            {
                target.put(*key, valueText(settings.value(stored)));
            }
        }
    }
    for (const std::string& name : childrenNamesLocked())
    {
        childLocked(name).snapshotLocked(target.getNode(name));
    }
}

void SettingsPreferences::restore(const Preferences& snapshot)
{
    // Copied first, without the lock: the snapshot may be this store itself.
    InMemoryPreferences copy;
    copyTree(snapshot, copy);
    const std::scoped_lock lock(m_store->mutex);
    resetLocked();
    restoreLocked(copy);
}

void SettingsPreferences::restoreLocked(const InMemoryPreferences& source)
{
    for (const std::string& key : source.keys())
    {
        const std::optional<std::string> value = source.get(key);
        if (value.has_value())
        {
            putLocked(key, *value);
        }
    }
    for (const std::string& name : source.childrenNames())
    {
        const InMemoryPreferences* const child = source.findNode(name);
        if (child != nullptr)
        {
            SettingsPreferences& node = childLocked(name);
            node.m_made               = true;
            node.restoreLocked(*child);
        }
    }
}

void SettingsPreferences::reset()
{
    const std::scoped_lock lock(m_store->mutex);
    resetLocked();
}

void SettingsPreferences::resetLocked()
{
    QSettings& settings = m_store->settings;
    if (m_group.isEmpty())
    {
        settings.clear();
    }
    else
    {
        // QSettings::remove() of the group also removes the parent's key of the same name, which
        // is put back.
        const qsizetype         slash  = m_group.lastIndexOf(u'/');
        const QString           parent = slash < 0 ? QString() : m_group.left(slash);
        const QString           name   = m_group.mid(slash + 1);
        const GroupScope        group(settings, parent);
        std::optional<QVariant> parentKey;
        if (settings.contains(name))
        {
            parentKey = settings.value(name);
        }
        settings.remove(name);
        if (parentKey.has_value())
        {
            settings.setValue(name, *parentKey);
        }
    }
    forgetChildrenLocked();
}

void SettingsPreferences::forgetChildrenLocked()
{
    for (const auto& entry : m_children)
    {
        entry.second->m_made = false;
        entry.second->forgetChildrenLocked();
    }
}

void SettingsPreferences::clearPreferences()
{
    reset();
    UnitGroup::resetDefaultUnits();
    storeDefaultUnits();
}

// ------------------------------------------------------------------------------ the file

Result<void> SettingsPreferences::sync()
{
    const std::scoped_lock lock(m_store->mutex);
    QSettings&             settings = m_store->settings;
    settings.sync();
    if (settings.status() == QSettings::AccessError)
    {
        return fail(ErrorCode::IO, std::format("cannot write the preferences file {}",
                                               QtStrings::toUtf8(settings.fileName())));
    }
    return {};
}

Result<void> SettingsPreferences::status() const
{
    const std::scoped_lock lock(m_store->mutex);
    const QSettings&       settings = m_store->settings;
    switch (settings.status())
    {
        case QSettings::NoError:
            return {};
        case QSettings::AccessError:
            return fail(ErrorCode::IO, std::format("cannot write the preferences file {}",
                                                   QtStrings::toUtf8(settings.fileName())));
        case QSettings::FormatError:
            return fail(ErrorCode::PARSE,
                        std::format("the preferences file {} is not a valid INI file",
                                    QtStrings::toUtf8(settings.fileName())));
    }
    QTROCKET_UNREACHABLE();
}

QString SettingsPreferences::fileName() const
{
    const std::scoped_lock lock(m_store->mutex);
    return m_store->settings.fileName();
}

ComponentDefaults SettingsPreferences::defaultComponentColors() const noexcept
{
    const QCoreApplication* const application = QCoreApplication::instance();
    if (qobject_cast<const QGuiApplication*>(application) != nullptr &&
        QThread::currentThread() == application->thread())
    {
        m_store->theme.store(ThemeColors::currentTheme());
    }
    return ThemeColors::defaultComponentColors(m_store->theme.load());
}

}  // namespace QtRocket::Gui
