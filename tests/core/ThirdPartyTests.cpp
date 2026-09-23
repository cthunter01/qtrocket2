// Canary tests: every third-party library links and works on this platform.

#include <sqlite3.h>

#include <string>

#include <gtest/gtest.h>
#include <pugixml.hpp>

#include "TestPaths.h"

namespace
{

TEST(ThirdParty, PugixmlParsesAndWrites)
{
    pugi::xml_document doc;
    const auto         result = doc.load_string(
        R"(<openrocket version="1.11"><rocket><name>Alpha</name></rocket></openrocket>)");
    ASSERT_TRUE(result);
    EXPECT_STREQ(doc.child("openrocket").attribute("version").as_string(), "1.11");
    EXPECT_STREQ(doc.child("openrocket").child("rocket").child("name").text().as_string(), "Alpha");
}

TEST(ThirdParty, SqliteReadsBundledMotorDatabase)
{
    const std::string path = (QtRocket::Test::dataDir() / "motors" / "initial_motors.db").string();
    sqlite3*          db   = nullptr;
    ASSERT_EQ(sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK) << path;

    sqlite3_stmt* stmt = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT value FROM meta WHERE key = 'schema_version'", -1,
                                 &stmt, nullptr),
              SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): sqlite returns unsigned char*
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), "2");
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM motors", -1, &stmt, nullptr), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(stmt, 0), 1458);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

}  // namespace
