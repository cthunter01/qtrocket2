// The icon resources of the GUI (gui/resources/CMakeLists.txt): every file listed there is in
// the program and loads, and no file is there that nothing names.

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QString>
#include <QStringList>
#include <QSvgRenderer>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"

namespace QtRocket::Gui
{

namespace
{

using IconResources = Test::GuiTest;
using Theme         = ThemeColors::Theme;

/// Every file under :/icons, sorted.
QStringList iconResources()
{
    QStringList  files;
    QDirIterator iterator(QStringLiteral(":/icons"), QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext())
    {
        files.append(iterator.next());
    }
    files.sort();
    return files;
}

/// The resources that do not load: an SVG QSvgRenderer cannot read, a PNG QImage cannot.
QStringList unreadableResources(const QStringList& files)
{
    QStringList unreadable;
    for (const QString& file : files)
    {
        const bool svg      = file.endsWith(QStringLiteral(".svg"));
        const bool readable = svg ? QSvgRenderer(file).isValid() : !QImage(file).isNull();
        if (!readable)
        {
            unreadable.append(file);
        }
    }
    return unreadable;
}

/// Every resource the code names: the icons, the component icons of both themes, the window
/// icons.
QStringList namedResources()
{
    QStringList named;
    for (const Icons::Id id : Icons::kAllIds)
    {
        named.append(Icons::resourcePath(id));
    }
    for (const Theme theme : {Theme::LIGHT, Theme::DARK})
    {
        for (const ComponentKind kind : kAllComponentKinds)
        {
            const QString name = ComponentIcons::iconName(kind, theme);
            if (!name.isEmpty())
            {
                named.append(ComponentIcons::resourcePath(name, false));
                named.append(ComponentIcons::resourcePath(name, true));
            }
        }
        for (const MassComponent::MassComponentType type : MassComponent::kAllMassComponentTypes)
        {
            named.append(
                ComponentIcons::resourcePath(ComponentIcons::massTypeIconName(type, theme), false));
        }
    }
    named.append(Icons::windowIconResources());
    named.sort();
    named.removeDuplicates();
    return named;
}

TEST_F(IconResources, HoldTheThirtyNineSvgsTheSeventyOneComponentIconsAndTheFourWindowIcons)
{
    const QStringList files = iconResources();
    EXPECT_EQ(files.size(), 39 + 71 + 4);
    EXPECT_EQ(files.filter(QStringLiteral(":/icons/actions/")).size(), 39);
    EXPECT_EQ(files.filter(QStringLiteral(":/icons/components/")).size(), 71);
    EXPECT_EQ(files.filter(QStringLiteral(":/icons/app/")).size(), 4);
}

TEST_F(IconResources, EveryResourceLoads)
{
    EXPECT_EQ(unreadableResources(iconResources()), QStringList());
}

TEST_F(IconResources, EveryResourceIsNamedByTheCodeAndEveryNameIsAResource)
{
    EXPECT_EQ(namedResources(), iconResources());
}

TEST_F(IconResources, EveryResourceNameIsInLowerCase)
{
    const QStringList files = iconResources();
    QStringList       upper;
    for (const QString& file : files)
    {
        if (QFileInfo(file).fileName() != QFileInfo(file).fileName().toLower())
        {
            upper.append(file);
        }
    }
    EXPECT_EQ(upper, QStringList());
}

}  // namespace

}  // namespace QtRocket::Gui
