#include "QtRocket/gui/tree/ComponentToolTip.h"

#include <memory>
#include <string>
#include <vector>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

namespace
{

// The expected texts are OpenRocket's (commit 5f164fd0e), recorded by the component-tree scout's
// TreeProbe.java (handoff/m2-scout/evidence/m2-scout-component-tree/evidence/TreeProbe.out),
// which calls ComponentTreeRenderer's getToolTipSingleComponent() and
// getToolTipMultipleComponents() on new components in the default units (g, cm). Only where a
// name holds markup does QtRocket differ, by decision U4: it escapes the name.

/// The Mach number of OpenRocket's default preferences (no CD here is calculated).
constexpr double kDefaultMach = 0.3;

/// TreeProbe.tooltips()'s design: a new rocket whose stage holds a nose cone and a body tube,
/// the body tube an inner tube, and that a mass component.
class ComponentToolTipTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_document = OpenRocketDocumentFactory::createNewRocket();
    }

    void TearDown() override
    {
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    /// Adds the four components to the stage.
    void build()
    {
        RocketComponent& stage = this->stage();
        m_noseCone             = &stage.addChild(std::make_unique<NoseCone>());
        m_bodyTube             = &stage.addChild(std::make_unique<BodyTube>());
        m_innerTube            = &m_bodyTube->addChild(std::make_unique<InnerTube>());
        m_mass                 = &m_innerTube->addChild(std::make_unique<MassComponent>());
    }

    /// The body tube's mass override, with its subcomponents (100 g).
    void overrideBodyTubeMass()
    {
        m_bodyTube->setMassOverridden(true);
        m_bodyTube->setOverrideMass(0.1);
        m_bodyTube->setSubcomponentsOverriddenMass(true);
    }

    /// Then its CG (5 cm) and CD (0.42) overrides, with its subcomponents.
    void overrideBodyTubeCgAndCd()
    {
        m_bodyTube->setCGOverridden(true);
        m_bodyTube->setOverrideCGX(0.05);
        m_bodyTube->setSubcomponentsOverriddenCG(true);
        m_bodyTube->setCDOverridden(true);
        m_bodyTube->setOverrideCD(0.42);
        m_bodyTube->setSubcomponentsOverriddenCD(true);
    }

    [[nodiscard]] Rocket&          rocket() { return m_document->getRocket(); }
    [[nodiscard]] RocketComponent& stage() { return rocket().getChild(0); }

    [[nodiscard]] static std::string single(const RocketComponent& component)
    {
        return ComponentToolTip::singleComponent(component, kDefaultMach).toStdString();
    }

    [[nodiscard]] static std::string multiple(const std::vector<const RocketComponent*>& components)
    {
        return ComponentToolTip::multipleComponents(components).toStdString();
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    RocketComponent*                    m_noseCone  = nullptr;
    RocketComponent*                    m_bodyTube  = nullptr;
    RocketComponent*                    m_innerTube = nullptr;
    RocketComponent*                    m_mass      = nullptr;
};

TEST_F(ComponentToolTipTest, NamesTheEmptyRocketAndStageWithoutAMass)
{
    // TreeProbe: "rocket, empty stage", "empty stage".
    EXPECT_EQ(single(rocket()), "<html><b>Rocket</b>");
    EXPECT_EQ(single(stage()), "<html><b>Sustainer</b>");
}

TEST_F(ComponentToolTipTest, GivesTheMassAndTheTotalOfWhatAComponentHolds)
{
    build();
    // TreeProbe: "rocket", "stage", "nose cone", "body tube with children", "inner tube with
    // child", "mass component".
    EXPECT_EQ(single(rocket()), "<html><b>Rocket</b> (62.8 g total)");
    EXPECT_EQ(single(stage()), "<html><b>Sustainer</b> (62.8 g total)");
    EXPECT_EQ(single(*m_noseCone), "<html><b>Nose Cone</b> (20.4 g)");
    EXPECT_EQ(single(*m_bodyTube), "<html><b>Body Tube</b> (41 g of 42.4 g total)");
    EXPECT_EQ(single(*m_innerTube), "<html><b>Inner Tube</b> (1.38 g of 1.38 g total)");
    EXPECT_EQ(single(*m_mass), "<html><b>Mass Component</b> (0 g)");
}

TEST_F(ComponentToolTipTest, SumsTheMassesOfSeveralComponents)
{
    build();
    // TreeProbe: "multi nose cone + body tube", "multi stage + body tube", "multi body tube +
    // inner tube", "multi inner tube + mass + nose".
    EXPECT_EQ(multiple({m_noseCone, m_bodyTube}),
              "<html><b>Components</b> (61.4 g of 62.8 g total)<br>Nose Cone, Body Tube");
    EXPECT_EQ(multiple({&stage(), m_bodyTube}),
              "<html><b>Components</b> (62.8 g)<br>Sustainer, Body Tube");
    EXPECT_EQ(multiple({m_bodyTube, m_innerTube}),
              "<html><b>Components</b> (42.4 g of 42.4 g total)<br>Body Tube, Inner Tube");
    EXPECT_EQ(multiple({m_innerTube, m_mass, m_noseCone}),
              "<html><b>Components</b> (21.8 g of 21.8 g total)<br>Inner Tube, Mass Component, "
              "Nose Cone");
}

TEST_F(ComponentToolTipTest, TellsOfAMassOverrideAndOfTheComponentsItOverrides)
{
    build();
    overrideBodyTubeMass();
    // TreeProbe: "body tube mass overridden incl subcomponents", "inner tube overridden by body
    // tube", "multi inner tube + mass (both overridden by bt)", "multi inner tube + body tube".
    EXPECT_EQ(single(*m_bodyTube), "<html><b>Body Tube</b> (100 g of 100 g total), mass overriden");
    EXPECT_EQ(single(*m_innerTube),
              "<html><b>Inner Tube</b><br>Mass overridden by Body Tube (100 g)");
    EXPECT_EQ(multiple({m_innerTube, m_mass}),
              "<html><b>Components</b> (100 g)<br>Inner Tube, Mass Component");
    EXPECT_EQ(multiple({m_innerTube, m_bodyTube}),
              "<html><b>Components</b> (100 g of 100 g total)<br>Inner Tube, Body Tube");
}

TEST_F(ComponentToolTipTest, TellsOfCgAndCdOverrides)
{
    build();
    overrideBodyTubeMass();
    overrideBodyTubeCgAndCd();
    // TreeProbe: "body tube mass+cg+cd overridden incl subcomponents", "inner tube overridden
    // thrice".
    EXPECT_EQ(single(*m_bodyTube),
              "<html><b>Body Tube</b> (100 g of 100 g total), mass overriden, CG overriden, "
              "<html>C<sub>D</sub> overriden</html>");
    EXPECT_EQ(single(*m_innerTube),
              "<html><b>Inner Tube</b><br>Mass overridden by Body Tube (100 g)<br>CG overridden "
              "by Body Tube (5 cm)<br><html>C<sub>D</sub> overridden by Body Tube (0.42)");
}

TEST_F(ComponentToolTipTest, TellsOfOwnOverridesOnly)
{
    build();
    overrideBodyTubeMass();
    overrideBodyTubeCgAndCd();
    m_bodyTube->setSubcomponentsOverriddenMass(false);
    m_bodyTube->setSubcomponentsOverriddenCG(false);
    m_bodyTube->setSubcomponentsOverriddenCD(false);
    // TreeProbe: "body tube own overrides only", "inner tube not overridden".
    EXPECT_EQ(single(*m_bodyTube),
              "<html><b>Body Tube</b> (100 g of 101 g total), mass overriden, CG overriden, "
              "<html>C<sub>D</sub> overriden</html>");
    EXPECT_EQ(single(*m_innerTube), "<html><b>Inner Tube</b> (1.38 g of 1.38 g total)");
}

TEST_F(ComponentToolTipTest, EscapesTheCommentAndBreaksItsLines)
{
    build();
    m_noseCone->setComment("  Line one\n<b>two</b> & \"three\"  ");
    // TreeProbe: "nose cone with comment".
    EXPECT_EQ(single(*m_noseCone),
              "<html><b>Nose Cone</b> (20.4 g)<br>Line one<br>&lt;b&gt;two&lt;/b&gt; &amp; "
              "&quot;three&quot;");
}

TEST_F(ComponentToolTipTest, EscapesTheName)
{
    build();
    m_noseCone->setComment("  Line one\n<b>two</b> & \"three\"  ");
    m_noseCone->setName("My <nose>");
    // Decision U4: OpenRocket's TreeProbe "nose cone named with markup" gives
    // "<html><b>My <nose></b> (20.4 g)<br>...", which shows the name as "My ".
    EXPECT_EQ(single(*m_noseCone),
              "<html><b>My &lt;nose&gt;</b> (20.4 g)<br>Line one<br>&lt;b&gt;two&lt;/b&gt; "
              "&amp; &quot;three&quot;");
}

TEST_F(ComponentToolTipTest, EscapesTheNamesOfOverridersAndOfASelection)
{
    build();
    overrideBodyTubeMass();
    overrideBodyTubeCgAndCd();
    m_bodyTube->setName("Tube <A> & B");
    EXPECT_EQ(single(*m_innerTube),
              "<html><b>Inner Tube</b><br>Mass overridden by Tube &lt;A&gt; &amp; B (100 g)<br>CG "
              "overridden by Tube &lt;A&gt; &amp; B (5 cm)<br><html>C<sub>D</sub> overridden by "
              "Tube &lt;A&gt; &amp; B (0.42)");
    EXPECT_EQ(multiple({m_innerTube, m_bodyTube}),
              "<html><b>Components</b> (100 g of 100 g total)<br>Inner Tube, Tube &lt;A&gt; "
              "&amp; B");
}

TEST_F(ComponentToolTipTest, TellsOfAMassOverriddenToZero)
{
    build();
    m_mass->setMassOverridden(true);
    m_mass->setOverrideMass(0.0);
    // TreeProbe: "mass component overridden to zero".
    EXPECT_EQ(single(*m_mass), "<html><b>Mass Component</b> (0 g), mass overriden");
}

TEST_F(ComponentToolTipTest, GivesNoMultipleToolTipForNoComponent)
{
    // getToolTipMultipleComponents() returns null for an empty list.
    EXPECT_TRUE(ComponentToolTip::multipleComponents({}).isNull());
}

TEST_F(ComponentToolTipTest, ShowsTheSelectionWhenTheHoveredComponentIsOneOfSeveral)
{
    build();
    const std::vector<const RocketComponent*> selection{m_noseCone, m_bodyTube};
    const std::vector<const RocketComponent*> one{m_noseCone};
    // applyToolTipText(): the selection's tooltip for one of its components, the component's own
    // for another one or a selection of one.
    EXPECT_EQ(ComponentToolTip::forComponent(*m_bodyTube, selection, kDefaultMach).toStdString(),
              "<html><b>Components</b> (61.4 g of 62.8 g total)<br>Nose Cone, Body Tube");
    EXPECT_EQ(ComponentToolTip::forComponent(*m_innerTube, selection, kDefaultMach).toStdString(),
              "<html><b>Inner Tube</b> (1.38 g of 1.38 g total)");
    EXPECT_EQ(ComponentToolTip::forComponent(*m_noseCone, one, kDefaultMach).toStdString(),
              "<html><b>Nose Cone</b> (20.4 g)");
}

}  // namespace

}  // namespace QtRocket::Gui
