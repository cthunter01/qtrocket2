#include "QtRocket/models/WindModelType.h"

#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/models/WindModel.h"

namespace
{

using QtRocket::altitudeReferenceFromString;
using QtRocket::kAllWindModelTypes;
using QtRocket::orkName;
using QtRocket::toStringValue;
using QtRocket::WindModel;
using QtRocket::WindModelType;
using QtRocket::windModelTypeFromString;
using QtRocket::windModelTypeName;

// OpenRocket has no WindModelTypeTest; these follow GravityModelTypeTest.

TEST(WindModelType, ToStringValue)
{
    EXPECT_EQ(toStringValue(WindModelType::AVERAGE), "Average");
    EXPECT_EQ(toStringValue(WindModelType::MULTI_LEVEL), "MultiLevel");
}

TEST(WindModelType, FromString)
{
    EXPECT_EQ(windModelTypeFromString("Average"), WindModelType::AVERAGE);
    EXPECT_EQ(windModelTypeFromString("average"), WindModelType::AVERAGE);
    EXPECT_EQ(windModelTypeFromString("AVERAGE"), WindModelType::AVERAGE);
    EXPECT_EQ(windModelTypeFromString("MultiLevel"), WindModelType::MULTI_LEVEL);
    EXPECT_EQ(windModelTypeFromString("multilevel"), WindModelType::MULTI_LEVEL);
}

TEST(WindModelType, FromStringInvalid)
{
    // Java throws IllegalArgumentException; the port returns nullopt.
    EXPECT_EQ(windModelTypeFromString("invalid"), std::nullopt);
    // The enum name of MULTI_LEVEL is not a string value, and nothing is trimmed.
    EXPECT_EQ(windModelTypeFromString("MULTI_LEVEL"), std::nullopt);
    EXPECT_EQ(windModelTypeFromString(" Average"), std::nullopt);
    EXPECT_EQ(windModelTypeFromString(""), std::nullopt);
}

TEST(WindModelType, ValuesInDeclarationOrder)
{
    ASSERT_EQ(kAllWindModelTypes.size(), 2U);
    EXPECT_EQ(kAllWindModelTypes[0], WindModelType::AVERAGE);
    EXPECT_EQ(kAllWindModelTypes[1], WindModelType::MULTI_LEVEL);
}

TEST(WindModelType, EnumNamesAndOrkSpellings)
{
    EXPECT_EQ(windModelTypeName(WindModelType::AVERAGE), "AVERAGE");
    EXPECT_EQ(windModelTypeName(WindModelType::MULTI_LEVEL), "MULTI_LEVEL");
    EXPECT_EQ(orkName(WindModelType::AVERAGE), "average");
    EXPECT_EQ(orkName(WindModelType::MULTI_LEVEL), "multilevel");
}

TEST(WindModelType, SpellingsParseBack)
{
    for (const WindModelType type : kAllWindModelTypes)
    {
        EXPECT_EQ(windModelTypeFromString(toStringValue(type)), type);
        EXPECT_EQ(windModelTypeFromString(orkName(type)), type);
    }
}

TEST(WindModelType, AltitudeReferenceOrkSpellings)
{
    using Reference = WindModel::AltitudeReference;
    EXPECT_EQ(toString(Reference::MSL), "msl");
    EXPECT_EQ(toString(Reference::AGL), "agl");
    EXPECT_EQ(altitudeReferenceFromString("msl"), Reference::MSL);
    EXPECT_EQ(altitudeReferenceFromString(" agl "), Reference::AGL);
    // DocumentConfig.findEnum compares with the lower-cased names exactly.
    EXPECT_EQ(altitudeReferenceFromString("AGL"), std::nullopt);
    EXPECT_EQ(altitudeReferenceFromString("ground"), std::nullopt);
}

}  // namespace
