#include "QtRocket/gui/main/RecentFiles.h"

#include <optional>
#include <string>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

RecentFiles::RecentFiles(Preferences& preferences, QObject* parent)
  : QObject(parent), m_preferences(&preferences)
{
    retrieve();
}

std::optional<QString> RecentFiles::lastEditedDesignFile() const
{
    if (m_files.isEmpty())
    {
        return std::nullopt;
    }
    return m_files.front();
}

void RecentFiles::setFiles(const QStringList& files)
{
    m_files = files.mid(0, kMaxSize);
    storeAndTell();
}

void RecentFiles::removeFile(const QString& file)
{
    m_files.removeOne(file);
    storeAndTell();
}

void RecentFiles::addFile(const QString& file)
{
    // Remove the old, add to the top.
    m_files.removeOne(file);
    m_files.prepend(file);
    while (m_files.size() > kMaxSize)
    {
        m_files.removeLast();
    }
    storeAndTell();
}

void RecentFiles::store()
{
    // clear(): putString(key, null) for each of the nine keys.
    for (int index = 0; index < kMaxSize; ++index)
    {
        m_preferences->putString(GuiPreferenceKeys::mruFileListKey(index), std::nullopt);
    }
    for (int index = 0; index < m_files.size(); ++index)
    {
        m_preferences->putString(GuiPreferenceKeys::mruFileListKey(index),
                                 QtStrings::toUtf8(m_files.at(index)));
    }
}

void RecentFiles::retrieve()
{
    m_files.clear();
    for (int index = 0; index < kMaxSize; ++index)
    {
        const std::optional<std::string> file =
            m_preferences->get(GuiPreferenceKeys::mruFileListKey(index));
        if (!file.has_value())
        {
            break;
        }
        m_files.append(QtStrings::fromUtf8(*file));
    }
}

void RecentFiles::storeAndTell()
{
    store();
    Q_EMIT changed();
}

}  // namespace QtRocket::Gui
