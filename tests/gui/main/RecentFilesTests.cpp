#include "QtRocket/gui/main/RecentFiles.h"

#include <optional>
#include <string>

#include <QChar>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "TestTempDir.h"

// The recent files over a store in memory and over a preferences file. The lists, change counts
// and stored keys after each step are those of OpenRocket's MRUDesignFile in the Java probe
// RecentExamplesProbe (probes/m2r3-P4/java, out-default.txt: the "MRU" lines).

namespace QtRocket::Gui
{

namespace
{

using RecentFilesTest = Gui::Test::GuiTest;

/// The keys MRUFileList0 to MRUFileList10 of @p store that hold a value, as the probe prints
/// them: "<index>=<value>".
[[nodiscard]] QStringList storedFiles(const Preferences& store)
{
    QStringList stored;
    for (int index = 0; index < 11; ++index)
    {
        const std::optional<std::string> value =
            store.get(GuiPreferenceKeys::mruFileListKey(index));
        if (value.has_value())
        {
            stored.append(QString::number(index) + QChar(u'=') + QtStrings::fromUtf8(*value));
        }
    }
    return stored;
}

/// Stores the files of @p files under MRUFileList0, MRUFileList1, ... of @p store.
void storeFiles(Preferences& store, const QStringList& files)
{
    for (int index = 0; index < files.size(); ++index)
    {
        store.put(GuiPreferenceKeys::mruFileListKey(index), QtStrings::toUtf8(files.at(index)));
    }
}

/// Counts the changes a RecentFiles tells.
struct CountChanges
{
    int* count;

    void operator()() const { ++*count; }
};

/// "/d/design<index>.ork".
[[nodiscard]] QString design(int index)
{
    return QStringLiteral("/d/design%1.ork").arg(index);
}

/// Adds each of @p files to @p recent, in order.
void addEach(RecentFiles& recent, const QStringList& files)
{
    for (const QString& file : files)
    {
        recent.addFile(file);
    }
}

/// The probe's eleven files: "/d/design0.ork" to "/d/design10.ork", or with @p pattern
/// ("/k/%1.ork").
[[nodiscard]] QStringList elevenFiles(const QString& pattern = QStringLiteral("/d/design%1.ork"))
{
    QStringList files;
    for (int index = 0; index < 11; ++index)
    {
        files.append(pattern.arg(index));
    }
    return files;
}

/// The newest file of @p recent, or "null" (as Java prints it).
[[nodiscard]] QString lastText(const RecentFiles& recent)
{
    const std::optional<QString> last = recent.lastEditedDesignFile();
    return last.has_value() ? *last : QStringLiteral("null");
}

TEST_F(RecentFilesTest, AnEmptyStoreHasNoRecentFile)
{
    // "MRU empty: list=[] last=null events=0 stored=[]".
    InMemoryPreferences store;
    const RecentFiles   recent(store);
    EXPECT_TRUE(recent.files().isEmpty());
    EXPECT_EQ(recent.lastEditedDesignFile(), std::nullopt);
    EXPECT_TRUE(storedFiles(store).isEmpty());
    EXPECT_EQ(RecentFiles::kMaxSize, 9);
}

TEST_F(RecentFilesTest, KeepsTheNineNewestFilesNewestFirst)
{
    // "MRU 11 adds": design10 ... design2, each stored under its index; 11 changes.
    InMemoryPreferences store;
    RecentFiles         recent(store);
    int                 changes = 0;
    QObject::connect(&recent, &RecentFiles::changed, &recent, CountChanges{.count = &changes});
    addEach(recent, elevenFiles());
    const QStringList newest{design(10), design(9), design(8), design(7), design(6),
                             design(5),  design(4), design(3), design(2)};
    EXPECT_EQ(recent.files(), newest);
    EXPECT_EQ(lastText(recent), design(10));
    EXPECT_EQ(changes, 11);
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/d/design10.ork"), QStringLiteral("1=/d/design9.ork"),
                           QStringLiteral("2=/d/design8.ork"), QStringLiteral("3=/d/design7.ork"),
                           QStringLiteral("4=/d/design6.ork"), QStringLiteral("5=/d/design5.ork"),
                           QStringLiteral("6=/d/design4.ork"), QStringLiteral("7=/d/design3.ork"),
                           QStringLiteral("8=/d/design2.ork")}));
}

TEST_F(RecentFilesTest, ChangesAsMruDesignFileMakesThem)
{
    // The probe's steps after its 11 adds, each with its list, newest file and stored keys.
    InMemoryPreferences store;
    RecentFiles         recent(store);
    addEach(recent, elevenFiles());
    int changes = 0;
    QObject::connect(&recent, &RecentFiles::changed, &recent, CountChanges{.count = &changes});

    // "add design5 again": it moves to the top.
    recent.addFile(design(5));
    const QStringList afterAdd{design(5), design(10), design(9), design(8), design(7),
                               design(6), design(4),  design(3), design(2)};
    EXPECT_EQ(recent.files(), afterAdd);
    EXPECT_EQ(changes, 1);

    // "remove absent": nothing goes, but the change is told and stored all the same.
    recent.removeFile(QStringLiteral("/d/absent.ork"));
    EXPECT_EQ(recent.files(), afterAdd);
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(storedFiles(store).size(), 9);

    // "remove design7".
    recent.removeFile(design(7));
    const QStringList afterRemove{design(5), design(10), design(9), design(8),
                                  design(6), design(4),  design(3), design(2)};
    EXPECT_EQ(recent.files(), afterRemove);
    EXPECT_EQ(changes, 3);
    EXPECT_EQ(
        storedFiles(store),
        (QStringList{QStringLiteral("0=/d/design5.ork"), QStringLiteral("1=/d/design10.ork"),
                     QStringLiteral("2=/d/design9.ork"), QStringLiteral("3=/d/design8.ork"),
                     QStringLiteral("4=/d/design6.ork"), QStringLiteral("5=/d/design4.ork"),
                     QStringLiteral("6=/d/design3.ork"), QStringLiteral("7=/d/design2.ork")}));

    // "add relative": a file is kept as it is given.
    recent.addFile(QStringLiteral("relative.ork"));
    EXPECT_EQ(recent.files().front(), QStringLiteral("relative.ork"));
    EXPECT_EQ(recent.files().size(), 9);
    EXPECT_EQ(lastText(recent), QStringLiteral("relative.ork"));
    EXPECT_EQ(storedFiles(store).front(), QStringLiteral("0=relative.ork"));
    EXPECT_EQ(storedFiles(store).back(), QStringLiteral("8=/d/design2.ork"));
    EXPECT_EQ(changes, 4);

    // "set 12": the first nine.
    recent.setFiles({QStringLiteral("/a1"), QStringLiteral("/a2"), QStringLiteral("/a3"),
                     QStringLiteral("/a4"), QStringLiteral("/a5"), QStringLiteral("/a6"),
                     QStringLiteral("/a7"), QStringLiteral("/a8"), QStringLiteral("/a9"),
                     QStringLiteral("/a10"), QStringLiteral("/a11"), QStringLiteral("/a12")});
    EXPECT_EQ(recent.files(),
              (QStringList{QStringLiteral("/a1"), QStringLiteral("/a2"), QStringLiteral("/a3"),
                           QStringLiteral("/a4"), QStringLiteral("/a5"), QStringLiteral("/a6"),
                           QStringLiteral("/a7"), QStringLiteral("/a8"), QStringLiteral("/a9")}));
    EXPECT_EQ(lastText(recent), QStringLiteral("/a1"));
    EXPECT_EQ(
        storedFiles(store),
        (QStringList{QStringLiteral("0=/a1"), QStringLiteral("1=/a2"), QStringLiteral("2=/a3"),
                     QStringLiteral("3=/a4"), QStringLiteral("4=/a5"), QStringLiteral("5=/a6"),
                     QStringLiteral("6=/a7"), QStringLiteral("7=/a8"), QStringLiteral("8=/a9")}));
    EXPECT_EQ(changes, 5);

    // "set empty": every key goes.
    recent.setFiles({});
    EXPECT_TRUE(recent.files().isEmpty());
    EXPECT_EQ(lastText(recent), QStringLiteral("null"));
    EXPECT_TRUE(storedFiles(store).isEmpty());
    EXPECT_EQ(changes, 6);
}

TEST_F(RecentFilesTest, TheStoredListEndsAtTheFirstMissingKey)
{
    // "MRU gap retrieved" and "gap add c": the key after the gap is not read, and the next
    // change removes it.
    InMemoryPreferences store;
    store.put("MRUFileList0", "/g/a.ork");
    store.put("MRUFileList1", "/g/b.ork");
    store.put("MRUFileList3", "/g/d.ork");
    RecentFiles recent(store);
    EXPECT_EQ(recent.files(),
              (QStringList{QStringLiteral("/g/a.ork"), QStringLiteral("/g/b.ork")}));
    EXPECT_EQ(lastText(recent), QStringLiteral("/g/a.ork"));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/g/a.ork"), QStringLiteral("1=/g/b.ork"),
                           QStringLiteral("3=/g/d.ork")}));

    recent.addFile(QStringLiteral("/g/c.ork"));
    EXPECT_EQ(recent.files(), (QStringList{QStringLiteral("/g/c.ork"), QStringLiteral("/g/a.ork"),
                                           QStringLiteral("/g/b.ork")}));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/g/c.ork"), QStringLiteral("1=/g/a.ork"),
                           QStringLiteral("2=/g/b.ork")}));
}

TEST_F(RecentFilesTest, DuplicatesAndAnEmptyEntryAreKeptAsStored)
{
    // "MRU duplicates retrieved" ... "remove empty": an entry is compared as a text, and only
    // its first occurrence goes.
    InMemoryPreferences store;
    storeFiles(store, {QStringLiteral("/u/a.ork"), QStringLiteral("/u/b.ork"),
                       QStringLiteral("/u/a.ork"), QString(), QStringLiteral("/u/e.ork")});
    RecentFiles recent(store);
    EXPECT_EQ(recent.files(),
              (QStringList{QStringLiteral("/u/a.ork"), QStringLiteral("/u/b.ork"),
                           QStringLiteral("/u/a.ork"), QString(), QStringLiteral("/u/e.ork")}));

    recent.removeFile(QStringLiteral("/u/a.ork"));
    EXPECT_EQ(recent.files(), (QStringList{QStringLiteral("/u/b.ork"), QStringLiteral("/u/a.ork"),
                                           QString(), QStringLiteral("/u/e.ork")}));
    EXPECT_EQ(lastText(recent), QStringLiteral("/u/b.ork"));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/u/b.ork"), QStringLiteral("1=/u/a.ork"),
                           QStringLiteral("2="), QStringLiteral("3=/u/e.ork")}));

    recent.addFile(QStringLiteral("/u/e.ork"));
    EXPECT_EQ(recent.files(), (QStringList{QStringLiteral("/u/e.ork"), QStringLiteral("/u/b.ork"),
                                           QStringLiteral("/u/a.ork"), QString()}));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/u/e.ork"), QStringLiteral("1=/u/b.ork"),
                           QStringLiteral("2=/u/a.ork"), QStringLiteral("3=")}));

    recent.removeFile(QString());
    EXPECT_EQ(recent.files(), (QStringList{QStringLiteral("/u/e.ork"), QStringLiteral("/u/b.ork"),
                                           QStringLiteral("/u/a.ork")}));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/u/e.ork"), QStringLiteral("1=/u/b.ork"),
                           QStringLiteral("2=/u/a.ork")}));
}

TEST_F(RecentFilesTest, KeysBeyondTheNinthAreNeitherReadNorRemoved)
{
    // "MRU eleven keys retrieved" and "eleven keys add new": clear() removes the nine keys only.
    InMemoryPreferences store;
    const QStringList   eleven = elevenFiles(QStringLiteral("/k/%1.ork"));
    storeFiles(store, eleven);
    RecentFiles recent(store);
    EXPECT_EQ(recent.files(), eleven.mid(0, 9));

    recent.addFile(QStringLiteral("/k/new.ork"));
    QStringList expected{QStringLiteral("/k/new.ork")};
    expected.append(eleven.mid(0, 8));
    EXPECT_EQ(recent.files(), expected);
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/k/new.ork"), QStringLiteral("1=/k/0.ork"),
                           QStringLiteral("2=/k/1.ork"), QStringLiteral("3=/k/2.ork"),
                           QStringLiteral("4=/k/3.ork"), QStringLiteral("5=/k/4.ork"),
                           QStringLiteral("6=/k/5.ork"), QStringLiteral("7=/k/6.ork"),
                           QStringLiteral("8=/k/7.ork"), QStringLiteral("9=/k/9.ork"),
                           QStringLiteral("10=/k/10.ork")}));
}

TEST_F(RecentFilesTest, TheListOutlivesAResetOfTheStore)
{
    // MRUDesignFile keeps its list in memory: "Reset all preferences" removes the keys, the
    // list stays, and the next change stores it again.
    InMemoryPreferences store;
    RecentFiles         recent(store);
    recent.addFile(QStringLiteral("/r/a.ork"));
    recent.addFile(QStringLiteral("/r/b.ork"));
    store.reset();
    EXPECT_TRUE(storedFiles(store).isEmpty());
    EXPECT_EQ(recent.files(),
              (QStringList{QStringLiteral("/r/b.ork"), QStringLiteral("/r/a.ork")}));
    recent.addFile(QStringLiteral("/r/c.ork"));
    EXPECT_EQ(storedFiles(store),
              (QStringList{QStringLiteral("0=/r/c.ork"), QStringLiteral("1=/r/b.ork"),
                           QStringLiteral("2=/r/a.ork")}));
}

TEST_F(RecentFilesTest, TheListPersistsInThePreferencesFile)
{
    // The keys of the root node of the user's preferences file, read by the next start.
    const QtRocket::Test::TempDir directory;
    const QString                 file   = QtStrings::fromPath(directory.resolve("QtRocket.ini"));
    const QString                 first  = QtStrings::fromPath(directory.resolve("first.ork"));
    const QString                 second = QtStrings::fromPath(
        directory.resolve(QtStrings::toPath(QStringLiteral(u"s\u00E9cond & third.ork"))));
    {
        SettingsPreferences store(file);
        RecentFiles         recent(store);
        recent.addFile(first);
        recent.addFile(second);
        EXPECT_TRUE(store.sync().has_value());
    }
    SettingsPreferences store(file);
    EXPECT_EQ(store.get("MRUFileList0"), std::optional(QtStrings::toUtf8(second)));
    EXPECT_EQ(store.get("MRUFileList1"), std::optional(QtStrings::toUtf8(first)));
    const RecentFiles recent(store);
    EXPECT_EQ(recent.files(), (QStringList{second, first}));
}

}  // namespace

}  // namespace QtRocket::Gui
