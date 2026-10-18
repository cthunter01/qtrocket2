#include "QtRocket/gui/util/BuildInfo.h"

#include <string>

#include <QString>

#include <gtest/gtest.h>

#include "QtRocket/util/Version.h"

namespace QtRocket::Gui
{

namespace
{

TEST(BuildInfo, NamesTheApplicationAndItsOrganisationQtRocket)
{
    EXPECT_EQ(BuildInfo::applicationName(), QStringLiteral("QtRocket"));
    EXPECT_EQ(BuildInfo::organizationName(), QStringLiteral("QtRocket"));
}

TEST(BuildInfo, HasTheProjectVersion)
{
    EXPECT_EQ(BuildInfo::applicationVersion().toStdString(), std::string(version()));
    EXPECT_FALSE(BuildInfo::applicationVersion().isEmpty());
}

TEST(BuildInfo, VersionTextIsTheCreatorStringOfSavedFiles)
{
    EXPECT_EQ(BuildInfo::versionText().toStdString(), std::string(creatorString()));
    EXPECT_EQ(BuildInfo::versionText(),
              QStringLiteral("QtRocket ") + BuildInfo::applicationVersion());
}

}  // namespace

}  // namespace QtRocket::Gui
