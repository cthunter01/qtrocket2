// The dialog's part of a field widget's enabled state (FieldEnabling): what a check box's enable
// follower or a dialog's rule decides, kept apart from what the field's binding says.

#include "QtRocket/gui/widgets/FieldEnabling.h"

#include <array>
#include <memory>

#include <QLabel>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "binding/BindingTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using FieldEnablingTest = Test::GuiTest;
using Test::bodyTubeLength;
using Test::IntBeanProperty;

/// A new rocket's document with a body tube in its stage: the tube.
[[nodiscard]] BodyTube& addTube(OpenRocketDocument& document)
{
    return document.getRocket().getChild(0).addChild(std::make_unique<BodyTube>());
}

/// The source of a component's mass override.
[[nodiscard]] BoolSource massOverridden()
{
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.isMassOverridden(); },
        .set = [](RocketComponent& component, bool on) { component.setMassOverridden(on); },
    });
}

/// The source of an external component's finish.
[[nodiscard]] EnumSource<Finish> finish()
{
    return EnumSource<Finish>::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getFinish(); },
        .set = [](ExternalComponent& component,
                  const Finish&      value) { component.setFinish(value); },
    });
}

/// The source of an external component's material.
[[nodiscard]] MaterialSource material()
{
    return MaterialSource::of<ExternalComponent>({
        .get = [](const ExternalComponent& component) { return component.getMaterial(); },
        .set = [](ExternalComponent& component,
                  const Material&    value) { component.setMaterial(value); },
    });
}

/// A field widget of every kind on one body tube, and a label.
struct EveryField
{
    // First, so that they go last: the widgets' bindings refer to them.
    MaterialStorage                     materials;
    std::unique_ptr<OpenRocketDocument> document{OpenRocketDocumentFactory::createNewRocket()};
    IntBeanProperty                     countBean{.value = 3};
    std::shared_ptr<ComponentSelection> selection{
        std::make_shared<ComponentSelection>(addTube(*document))};
    std::shared_ptr<DoubleBinding> length{std::make_shared<DoubleBinding>(
        selection, bodyTubeLength(), unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{})};

    QWidget           window;
    UnitSpinBox*      spin{makeChild<UnitSpinBox>(&window, length)};
    UnitSelector*     selector{makeChild<UnitSelector>(&window, length)};
    ValueSlider*      slider{makeChild<ValueSlider>(&window, length, SliderMapping::linear(0, 1))};
    IntSpinBox*       count{makeChild<IntSpinBox>(
        &window, std::make_shared<IntBinding>(selection, countBean.source()))};
    BoolCheckBox*     check{makeChild<BoolCheckBox>(
        &window, std::make_shared<BoolBinding>(selection, massOverridden()), QString())};
    EnumComboBox*     combo{makeChild<EnumComboBox>(
        &window, std::make_shared<EnumBinding<Finish>>(selection, finish(), kAllFinishes))};
    MaterialComboBox* materialCombo{makeChild<MaterialComboBox>(
        &window, std::make_shared<MaterialBinding>(selection, material(), Material::Type::BULK,
                                                   materials, *document))};
    QLabel*           label{makeChild<QLabel>(&window)};

    /// Every widget.
    [[nodiscard]] std::array<QWidget*, 8> all() const
    {
        return {spin, selector, slider, count, check, combo, materialCombo, label};
    }
};

/// How many widgets of @p fields are enabled.
[[nodiscard]] int enabledCount(const EveryField& fields)
{
    int count = 0;
    for (const QWidget* widget : fields.all())
    {
        count += widget->isEnabled() ? 1 : 0;
    }
    return count;
}

/// How many widgets of @p fields the dialog's part enables.
[[nodiscard]] int fieldEnabledCount(const EveryField& fields)
{
    int count = 0;
    for (const QWidget* widget : fields.all())
    {
        count += FieldEnabling::isFieldEnabled(*widget) ? 1 : 0;
    }
    return count;
}

/// Sets the dialog's part of every widget of @p fields to @p enabled.
void setEveryFieldEnabled(const EveryField& fields, bool enabled)
{
    for (QWidget* widget : fields.all())
    {
        FieldEnabling::setFieldEnabled(*widget, enabled);
    }
}

TEST_F(FieldEnablingTest, SetsTheDialogsPartOfEveryFieldWidget)
{
    const EveryField fields;
    EXPECT_EQ(enabledCount(fields), 8);
    EXPECT_EQ(fieldEnabledCount(fields), 8);
    setEveryFieldEnabled(fields, false);
    EXPECT_FALSE(fields.spin->isFieldEnabled());
    EXPECT_FALSE(fields.selector->isFieldEnabled());
    EXPECT_FALSE(fields.slider->isFieldEnabled());
    EXPECT_FALSE(fields.count->isFieldEnabled());
    EXPECT_FALSE(fields.check->isFieldEnabled());
    EXPECT_FALSE(fields.combo->isFieldEnabled());
    EXPECT_FALSE(fields.materialCombo->isFieldEnabled());
    EXPECT_EQ(enabledCount(fields), 0);
    EXPECT_EQ(fieldEnabledCount(fields), 0);
    setEveryFieldEnabled(fields, true);
    EXPECT_EQ(enabledCount(fields), 8);
}

TEST_F(FieldEnablingTest, AnyOtherWidgetsStateIsItsOwnWhateverItsParent)
{
    QWidget parent;
    auto*   label = makeChild<QLabel>(&parent);
    parent.setEnabled(false);
    EXPECT_FALSE(label->isEnabled());
    EXPECT_TRUE(FieldEnabling::isFieldEnabled(*label));
    FieldEnabling::setFieldEnabled(*label, false);
    parent.setEnabled(true);
    EXPECT_FALSE(label->isEnabled());
    EXPECT_FALSE(FieldEnabling::isFieldEnabled(*label));
}

}  // namespace

}  // namespace QtRocket::Gui
