#include "FileMenusTestSupport.h"

#include <filesystem>
#include <memory>
#include <string_view>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QList>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket::Gui::Test
{

void FileMenusTest::TearDown()
{
    m_operations.reset();
    m_manager.reset();
    deleteLaterNow();
    GuiTest::TearDown();
}

WindowManager& FileMenusTest::manager(WindowManager::LastWindowBehaviour behaviour)
{
    if (m_manager == nullptr)
    {
        m_manager    = std::make_unique<WindowManager>(m_services.services(), behaviour);
        m_operations = std::make_unique<DocumentFileOperations>(*m_manager, m_services.services());
    }
    return *m_manager;
}

DocumentFileOperations& FileMenusTest::operations()
{
    static_cast<void>(manager());
    EXPECT_NE(m_operations, nullptr);
    return *m_operations;
}

void FileMenusTest::destroyOperations()
{
    m_operations.reset();
}

std::filesystem::path FileMenusTest::writeDesign(const std::filesystem::path& name,
                                                 std::string_view             rocketName)
{
    const std::filesystem::path         file     = m_directory.resolve(name);
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    document->getRocket().setName(rocketName);
    GeneralRocketSaver saver(m_services.preferences());
    EXPECT_TRUE(saver.save(file, *document).has_value());
    return file;
}

void FileMenusTest::deleteLaterNow()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

QStringList menuTexts(const QMenu& menu)
{
    QStringList texts;
    for (const QAction* const item : menu.actions())
    {
        texts.append(item->isSeparator() ? QStringLiteral("----") : item->text());
    }
    return texts;
}

QAction* menuItem(const QMenu& menu, const QString& text)
{
    for (QAction* const item : menu.actions())
    {
        if (!item->isSeparator() && item->text() == text)
        {
            return item;
        }
    }
    ADD_FAILURE() << "no item " << QtStrings::toUtf8(text) << " in the menu";
    return nullptr;
}

}  // namespace QtRocket::Gui::Test
