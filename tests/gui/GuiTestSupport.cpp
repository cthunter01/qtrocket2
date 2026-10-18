#include "GuiTestSupport.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <gtest/gtest.h>

#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The class of @p widget, with its object name and its window title when it has them.
std::string describeWidget(const QWidget& widget)
{
    std::string text = widget.metaObject()->className();
    if (!widget.objectName().isEmpty())
    {
        text += " '" + widget.objectName().toStdString() + "'";
    }
    if (!widget.windowTitle().isEmpty())
    {
        text += " titled '" + widget.windowTitle().toStdString() + "'";
    }
    return text;
}

}  // namespace

void GuiTest::SetUp()
{
    for (std::size_t index = 0; index < kAllUnitGroupIds.size(); ++index)
    {
        m_defaultUnits.at(index) = unitGroup(kAllUnitGroupIds.at(index)).getDefaultUnitIndex();
    }
    std::error_code error;
    m_currentDirectory = std::filesystem::current_path(error);
}

void GuiTest::TearDown()
{
    // A widget given to deleteLater() stays a top-level widget until the event loop deletes it.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto leftOver = QApplication::topLevelWidgets();
    for (const QWidget* widget : leftOver)
    {
        ADD_FAILURE() << "the test left a top-level widget behind: " << describeWidget(*widget);
    }

    QClipboard*      clipboard = QGuiApplication::clipboard();
    const QMimeData* data      = clipboard->mimeData();
    if (data != nullptr && !data->formats().isEmpty())
    {
        ADD_FAILURE() << "the test left data on the clipboard: "
                      << data->formats().join(QStringLiteral(", ")).toStdString();
        clipboard->clear();
    }

    // The fixture's DefaultUnitsGuard puts the defaults back afterwards.
    for (std::size_t index = 0; index < kAllUnitGroupIds.size(); ++index)
    {
        EXPECT_EQ(unitGroup(kAllUnitGroupIds.at(index)).getDefaultUnitIndex(),
                  m_defaultUnits.at(index))
            << "the test changed the default unit of " << unitGroupName(kAllUnitGroupIds.at(index));
    }

    std::error_code             error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    if (current != m_currentDirectory)
    {
        ADD_FAILURE() << "the test changed the current directory to " << pathToUtf8(current);
        std::filesystem::current_path(m_currentDirectory, error);
    }
}

}  // namespace QtRocket::Gui::Test
