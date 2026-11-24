#include "QtRocket/models/WindModelType.h"

#include <optional>

#include <gtest/gtest.h>

#include "QtRocket/models/WindModel.h"

namespace
{

using QtRocket::altitudeReferenceFromString;
using QtRocket::description;
using QtRocket::descriptionKey;
using QtRocket::displayKey;
using QtRocket::displayName;
using QtRocket::kAllWindModelTypes;
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
    EXPECT_EQ(toString(WindModelType::AVERAGE), "average");
    EXPECT_EQ(toString(WindModelType::MULTI_LEVEL), "multilevel");
}

TEST(WindModelType, SpellingsParseBack)
{
    for (const WindModelType type : kAllWindModelTypes)
    {
        EXPECT_EQ(windModelTypeFromString(toStringValue(type)), type);
        EXPECT_EQ(windModelTypeFromString(toString(type)), type);
    }
}

TEST(WindModelType, DisplayKeysAndNamesAreTheWindModelRadioButtons)
{
    // OpenRocket shows the types as the simulation dialog's radio buttons, with their tooltips.
    EXPECT_EQ(displayKey(WindModelType::AVERAGE), "simedtdlg.radio.AverageWind");
    EXPECT_EQ(displayKey(WindModelType::MULTI_LEVEL), "simedtdlg.radio.MultiLevelWind");
    EXPECT_EQ(displayName(WindModelType::AVERAGE), "Average");
    EXPECT_EQ(displayName(WindModelType::MULTI_LEVEL), "Multi-level");
    EXPECT_EQ(descriptionKey(WindModelType::AVERAGE), "simedtdlg.radio.AverageWind.ttip");
    EXPECT_EQ(descriptionKey(WindModelType::MULTI_LEVEL), "simedtdlg.radio.MultiLevelWind.ttip");
    EXPECT_EQ(description(WindModelType::AVERAGE),
              "Model the wind as pink noise from the average wind speed and standard deviation.");
    EXPECT_EQ(description(WindModelType::MULTI_LEVEL),
              "Model the wind using speed and direction entries at various altitude levels.");
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
