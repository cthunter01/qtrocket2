#include "QtRocket/util/BugError.h"

#include <functional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

namespace
{

using QtRocket::BugError;

/// Runs @p body and returns the BugError it throws. A body that does not throw is a test
/// failure in the making: the returned error then says so, and no expectation on it holds.
BugError catchBug(const std::function<void()>& body)
{
    try
    {
        body();
    }
    catch (const BugError& error)
    {
        return error;
    }
    return BugError{"nothing was thrown"};
}

TEST(BugError, MessageNamesTheBugAndWhereItWasRaised)
{
    // The error is raised on the line right after this one.
    const auto     before = std::source_location::current();
    const BugError error{"the tree has a cycle"};
    const auto     line = before.line() + 1;

    const std::string what = error.what();
    EXPECT_EQ(what.substr(0, 5), "BUG: ");
    EXPECT_NE(what.find("the tree has a cycle"), std::string::npos);
    EXPECT_NE(what.find("BugErrorTests.cpp:" + std::to_string(line) + ")"), std::string::npos);
    EXPECT_EQ(what.find('/'), std::string::npos);  // the base name only, not the whole path
    EXPECT_EQ(error.where().line(), line);
    EXPECT_EQ(std::string_view(error.where().file_name()), std::string_view(before.file_name()));
}

TEST(BugError, IsALogicError)
{
    static_assert(std::is_base_of_v<std::logic_error, BugError>);
    EXPECT_THROW(QtRocket::bug("boom"), std::logic_error);
}

TEST(BugError, BugThrowsFromTheCallSite)
{
    std::source_location before;
    const BugError       error = catchBug([&before] {
        // bug() is called on the line right after this one.
        before = std::source_location::current();
        QtRocket::bug("explicit");
    });
    // The reported location is the caller's, not bug()'s own.
    EXPECT_EQ(error.where().line(), before.line() + 1);
    EXPECT_NE(std::string_view(error.what())
                  .find("BugErrorTests.cpp:" + std::to_string(before.line() + 1) + ")"),
              std::string_view::npos);
    EXPECT_EQ(std::string_view(error.where().file_name()), std::string_view(before.file_name()));
    EXPECT_NE(std::string_view(error.what()).find("BUG: explicit"), std::string_view::npos);
}

TEST(BugError, ExplicitLocationIsKept)
{
    const std::source_location where = std::source_location::current();
    const BugError             error{"elsewhere", where};
    EXPECT_EQ(error.where().line(), where.line());
    EXPECT_NE(std::string_view(error.what()).find(std::to_string(where.line())),
              std::string_view::npos);
}

TEST(BugError, AssertPassesWithoutThrowing)
{
    int calls = 0;
    QTROCKET_ASSERT(++calls == 1);
    EXPECT_EQ(calls, 1);  // the condition is evaluated exactly once
    const int* pointer = &calls;
    QTROCKET_ASSERT(pointer != nullptr);
    QTROCKET_ASSERT(true);
    EXPECT_NE(std::string_view(catchBug([] { QTROCKET_ASSERT(1 + 1 == 2); }).what())
                  .find("nothing was thrown"),
              std::string_view::npos);
}

TEST(BugError, AssertThrowsWithTheConditionText)
{
    int                    calls = 0;
    const BugError         error = catchBug([&calls] { QTROCKET_ASSERT(++calls == 100); });
    const std::string_view what  = error.what();
    EXPECT_NE(what.find("BUG: assertion failed: ++calls == 100"), std::string_view::npos);
    EXPECT_NE(what.find("BugErrorTests.cpp:"), std::string_view::npos);
    EXPECT_EQ(calls, 1);  // evaluated once even when it fails
    EXPECT_NE(std::string_view(catchBug([] { QTROCKET_ASSERT(false); }).what())
                  .find("assertion failed: false"),
              std::string_view::npos);
}

TEST(BugError, AssertReportsTheAssertingLine)
{
    std::source_location before;
    const BugError       error = catchBug([&before] {
        // The assertion is on the line right after this one.
        before = std::source_location::current();
        QTROCKET_ASSERT(before.line() == 0);
    });
    EXPECT_EQ(error.where().line(), before.line() + 1);
}

TEST(BugError, UnreachableThrows)
{
    const BugError         error = catchBug([] { QTROCKET_UNREACHABLE(); });
    const std::string_view what  = error.what();
    EXPECT_NE(what.find("BUG: unreachable code reached"), std::string_view::npos);
    EXPECT_NE(what.find("BugErrorTests.cpp:"), std::string_view::npos);
}

TEST(BugError, AssertIsASingleExpression)
{
    // The macro takes a trailing semicolon and can stand in a branch of an if or a ternary.
    const bool flag = true;
    if (flag)
    {
        QTROCKET_ASSERT(flag);
    }
    else
    {
        QTROCKET_UNREACHABLE();
    }
    flag ? QTROCKET_ASSERT(flag) : QTROCKET_UNREACHABLE();
    SUCCEED();
}

}  // namespace
