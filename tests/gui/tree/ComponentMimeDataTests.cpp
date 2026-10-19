#include "QtRocket/gui/tree/ComponentMimeData.h"

#include <memory>
#include <vector>

#include <QByteArray>
#include <QMimeData>
#include <QString>
#include <QStringList>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using ::testing::ElementsAre;
using ComponentMimeDataTest = Gui::Test::GuiTest;

TEST_F(ComponentMimeDataTest, CarriesTheIdsAndTheDocumentTheyComeFrom)
{
    const std::unique_ptr<OpenRocketDocument> source = OpenRocketDocumentFactory::createNewRocket();
    const std::unique_ptr<OpenRocketDocument> other  = OpenRocketDocumentFactory::createNewRocket();
    const Uuid                                rocket = source->getRocket().getId();
    const Uuid                                stage  = source->getRocket().getChild(0).getId();

    const ComponentMimeData data(*source, std::vector<Uuid>{rocket, stage});

    EXPECT_THAT(data.componentIds(), ElementsAre(rocket, stage));
    EXPECT_EQ(&data.sourceDocument(), source.get());
    EXPECT_TRUE(data.isFrom(*source));
    EXPECT_FALSE(data.isFrom(*other));
    // The format, with the ids one per line for whoever looks.
    EXPECT_EQ(data.formats(), QStringList{ComponentMimeData::mimeType()});
    EXPECT_EQ(data.data(ComponentMimeData::mimeType()),
              QByteArray::fromStdString(rocket.toString() + "\n" + stage.toString() + "\n"));
    EXPECT_EQ(ComponentMimeData::mimeType(), QStringLiteral("application/x-qtrocket-components"));
    EXPECT_EQ(ComponentMimeData::fromMimeData(&data), &data);
}

TEST_F(ComponentMimeDataTest, KnowsOnlyItsOwnDrags)
{
    // Data of the format from elsewhere (another program's drag) is not a drag of the tree: as
    // Java's DataFlavor of the class, the drags work within this process only.
    QMimeData other;
    other.setData(ComponentMimeData::mimeType(), QByteArray("not ours"));

    EXPECT_EQ(ComponentMimeData::fromMimeData(&other), nullptr);
    EXPECT_EQ(ComponentMimeData::fromMimeData(nullptr), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
