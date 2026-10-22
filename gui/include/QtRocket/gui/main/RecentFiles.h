#pragma once

#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui
{

/// The recently used design files (MRUDesignFile), kept in the preference store: the keys
/// "MRUFileList0" (the newest) to "MRUFileList8" of its root node, each a file's absolute path
/// as the file operations give it (GuiPreferenceKeys::mruFileListKey()). The list is read from
/// the store when this object is made (retrieve(): up to the first key that is missing, nine at
/// most) and kept in memory; every change rewrites the nine keys (store(): each removed, then the
/// list written from "MRUFileList0" on; a key beyond the ninth stays as it is) and is told by
/// changed(), which the Open recent menus follow (RecentFilesMenu).
///
/// Added are the files opened (File > Open, the command line, the platform's requests, Open
/// recent, the last design at the start) and the files a design was saved as (Save as), by the
/// application (ApplicationSession, from DocumentFileOperations' designFileOpened() and
/// designFileSavedAs()); removed is a file of the list that could not be opened (Open recent,
/// the last design at the start).
///
/// As in OpenRocket, the list is the one in memory: "Reset all preferences" (the preferences
/// dialog) removes the keys from the store, the menus keep their files, and the next change
/// stores the list again. The texts are compared as they are (Java's String.equals()): the same
/// file by another path is another entry.
///
/// One per application (ApplicationSession; Java's static instance); the store must outlive it.
/// The GUI thread's.
class RecentFiles final : public QObject
{
    Q_OBJECT

public:
    /// MRUDesignFile.MAX_SIZE: the number of files kept.
    static constexpr int kMaxSize = GuiPreferenceKeys::kMruFileListMaxSize;

    /// The list stored in @p preferences (retrieve()).
    explicit RecentFiles(Preferences& preferences, QObject* parent = nullptr);

    /// getMRUFileList(): the files, the newest first.
    [[nodiscard]] const QStringList& files() const noexcept { return m_files; }

    /// getLastEditedDesignFile(): the newest file, or nullopt when the list is empty.
    [[nodiscard]] std::optional<QString> lastEditedDesignFile() const;

    /// setMRUFileList(): the first nine of @p files become the list.
    void setFiles(const QStringList& files);

    /// removeFile(): takes the first entry that is @p file out of the list (stored and told
    /// even when there is none).
    void removeFile(const QString& file);

    /// addFile(): puts @p file at the top of the list, taking its first earlier entry out; the
    /// oldest beyond nine go.
    void addFile(const QString& file);

Q_SIGNALS:
    /// The list changed (MRUDesignFile's property change MRU_FILE_LIST_PROPERTY); it is stored
    /// already.
    void changed();

private:
    /// store(): clear() (the nine keys removed), then the list written.
    void store();
    /// retrieve(): the stored list, up to the first missing key.
    void retrieve();
    /// store(), then changed() (Java fires first, then stores: no listener reads the store).
    void storeAndTell();

    Preferences* m_preferences;
    QStringList  m_files;
};

}  // namespace QtRocket::Gui
