// The pin tables of the figure's shape tests (BodyShapePins.h, FinMassShapePins.h and
// FigureGeometryPins.h, written by tools/openrocket-goldens/figure-pins/generate.sh) come from the
// OpenRocket of the goldens (tests/data/goldens/manifest.json), a clean checkout of it: the
// generator records the commit and whether the checkout had local changes in each header.

#include <gtest/gtest.h>

#include "figure/BodyShapePins.h"
#include "figure/FigureGeometryPins.h"
#include "figure/FinMassShapePins.h"
#include "goldens/GoldenData.h"

namespace
{

TEST(FigurePinsGolden, TheyComeFromACleanCheckoutOfTheOpenRocketOfTheGoldens)
{
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(QtRocket::Test::BodyShapePins::kOpenRocketCommit, manifest->openrocketCommit);
    EXPECT_EQ(QtRocket::Test::FinMassShapePins::kOpenRocketCommit, manifest->openrocketCommit);
    EXPECT_EQ(QtRocket::Test::FigureGeometryPins::kOpenRocketCommit, manifest->openrocketCommit);
    EXPECT_FALSE(QtRocket::Test::BodyShapePins::kOpenRocketDirty);
    EXPECT_FALSE(QtRocket::Test::FinMassShapePins::kOpenRocketDirty);
    EXPECT_FALSE(QtRocket::Test::FigureGeometryPins::kOpenRocketDirty);
}

}  // namespace
