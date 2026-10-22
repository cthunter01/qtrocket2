#include "tree/ProbeDesign.h"

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/EngineBlock.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// @p component named @p name, added as the last child of @p parent (the probes name each
/// component first).
template <class T>
RocketComponent& add(RocketComponent& parent, std::string_view name)
{
    auto component = std::make_unique<T>();
    component->setName(name);
    return parent.addChild(std::move(component));
}

/// "[a, b]", Java's List.toString() of @p texts.
std::string javaList(const std::vector<std::string>& texts)
{
    std::string text = "[";
    for (std::size_t i = 0; i < texts.size(); ++i)
    {
        text += (i > 0 ? ", " : "") + texts[i];
    }
    return text + "]";
}

}  // namespace

ProbeDesign::ProbeDesign(Booster booster) : m_document(makeDocument(booster))
{
    for (const RocketComponent& component : rocket().subtree())
    {
        m_originalIds.insert(component.getId());
    }
}

ProbeDesign::~ProbeDesign() = default;

std::unique_ptr<OpenRocketDocument> ProbeDesign::makeDocument(Booster booster)
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    Rocket&                             rocket   = document->getRocket();
    RocketComponent&                    stage    = rocket.getChild(0);
    add<NoseCone>(stage, "NC");
    RocketComponent& bt1 = add<BodyTube>(stage, "BT1");
    RocketComponent& it  = add<InnerTube>(bt1, "IT");
    add<EngineBlock>(it, "EB");
    add<CenteringRing>(bt1, "CR1");
    add<TrapezoidFinSet>(bt1, "Fins");
    RocketComponent& bt2 = add<BodyTube>(stage, "BT2");
    add<Parachute>(bt2, "Chute");
    if (booster != Booster::NONE)
    {
        RocketComponent& second = add<AxialStage>(rocket, "Booster");
        if (booster == Booster::WITH_SIDE_BOOSTERS)
        {
            RocketComponent& bbt  = add<BodyTube>(second, "BBT");
            RocketComponent& side = add<ParallelStage>(bbt, "Side boosters");
            add<BodyTube>(side, "PBT");
        }
    }
    return document;
}

Rocket& ProbeDesign::rocket() const
{
    return m_document->getRocket();
}

RocketComponent& ProbeDesign::find(std::string_view name) const
{
    return find(rocket(), name);
}

RocketComponent& ProbeDesign::find(Rocket& rocket, std::string_view name)
{
    for (RocketComponent& component : rocket.subtree())
    {
        if (component.getName() == name)
        {
            return component;
        }
    }
    ADD_FAILURE() << "no component named " << name;
    return rocket;
}

std::vector<RocketComponent*> ProbeDesign::find(std::initializer_list<std::string_view> names) const
{
    std::vector<RocketComponent*> found;
    found.reserve(names.size());
    for (const std::string_view name : names)
    {
        found.push_back(&find(name));
    }
    return found;
}

std::string ProbeDesign::tree(const RocketComponent& component) const
{
    std::string text = component.getName();
    if (!isOriginal(component))
    {
        text += '*';
    }
    if (component.getChildCount() > 0)
    {
        std::vector<std::string> children;
        children.reserve(component.getChildCount());
        for (std::size_t i = 0; i < component.getChildCount(); ++i)
        {
            children.push_back(tree(component.getChild(i)));
        }
        text += javaList(children);
    }
    return text;
}

std::string ProbeDesign::tree() const
{
    return tree(rocket());
}

bool ProbeDesign::isOriginal(const RocketComponent& component) const
{
    return m_originalIds.contains(component.getId());
}

std::string ProbeDesign::plainTree(const RocketComponent& component)
{
    std::string text = component.getName();
    if (component.getChildCount() > 0)
    {
        std::vector<std::string> children;
        children.reserve(component.getChildCount());
        for (std::size_t i = 0; i < component.getChildCount(); ++i)
        {
            children.push_back(plainTree(component.getChild(i)));
        }
        text += javaList(children);
    }
    return text;
}

std::string ProbeDesign::names(const std::vector<const RocketComponent*>& components) const
{
    std::vector<std::string> texts;
    texts.reserve(components.size());
    for (const RocketComponent* component : components)
    {
        texts.push_back(component->getName() + (isOriginal(*component) ? "" : "*"));
    }
    return javaList(texts);
}

std::string ProbeDesign::names(const std::vector<RocketComponent*>& components) const
{
    return names(std::vector<const RocketComponent*>(components.begin(), components.end()));
}

std::string ProbeDesign::clipboard(const ComponentCopy& copy) const
{
    const std::vector<const RocketComponent*> components = copy.getComponents();
    std::vector<std::string>                  trees;
    trees.reserve(components.size());
    for (const RocketComponent* component : components)
    {
        trees.push_back(tree(*component));
    }
    return javaList(trees);
}

}  // namespace QtRocket::Gui::Test
