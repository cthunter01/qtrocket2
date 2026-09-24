#include "QtRocket/util/Mutable.h"

#include <format>
#include <functional>
#include <source_location>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Mutable;

/// Runs @p body and returns the BugError it throws, or a marker error when nothing is thrown.
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

TEST(Mutable, StartsMutableAndChecksQuietly)
{
    const Mutable mutableState;
    EXPECT_TRUE(mutableState.isMutable());
    EXPECT_NO_THROW(mutableState.check());
}

TEST(Mutable, ImmuteIsForGood)
{
    Mutable state;
    state.immute();
    EXPECT_FALSE(state.isMutable());
    EXPECT_THROW(state.check(), BugError);
    // Repeated calls change nothing.
    state.immute();
    EXPECT_FALSE(state.isMutable());
    EXPECT_THROW(state.check(), BugError);
}

TEST(Mutable, CheckNamesWhereImmuteWasCalledFirst)
{
    Mutable                    state;
    const std::source_location before = std::source_location::current();
    state.immute();
    state.immute();
    const std::string first  = std::format("MutableTests.cpp:{}", before.line() + 1);
    const std::string second = std::format("MutableTests.cpp:{}", before.line() + 2);

    std::source_location checkedAt;
    const BugError       error = catchBug([&state, &checkedAt] {
        // check() is called on the line right after this one.
        checkedAt = std::source_location::current();
        state.check();
    });
    const std::string    what  = error.what();
    EXPECT_NE(what.find("Object has been made immutable at " + first), std::string::npos) << what;
    EXPECT_EQ(what.find(second), std::string::npos) << what;
    // The error is raised at the caller of check().
    EXPECT_EQ(error.where().line(), checkedAt.line() + 1);
    EXPECT_EQ(std::string_view(error.where().file_name()), std::string_view(checkedAt.file_name()));
}

TEST(Mutable, ExplicitImmuteLocationIsReported)
{
    Mutable                    state;
    const std::source_location where = std::source_location::current();
    state.immute(where);
    const std::string what = catchBug([&state] { state.check(); }).what();
    EXPECT_NE(what.find(std::format("MutableTests.cpp:{}", where.line())), std::string::npos)
        << what;
}

TEST(Mutable, CopyKeepsTheState)
{
    // Java: clone() copies the immute trace.
    Mutable mutableState;
    Mutable mutableCopy = mutableState;
    EXPECT_TRUE(mutableCopy.isMutable());
    mutableCopy.immute();
    EXPECT_TRUE(mutableState.isMutable());  // the copies are independent

    Mutable    frozen;
    const auto line = std::source_location::current().line() + 1;
    frozen.immute();
    const Mutable frozenCopy = frozen;
    EXPECT_FALSE(frozenCopy.isMutable());
    const std::string what = catchBug([&frozenCopy] { frozenCopy.check(); }).what();
    EXPECT_NE(what.find(std::format("MutableTests.cpp:{}", line)), std::string::npos) << what;
}

}  // namespace
