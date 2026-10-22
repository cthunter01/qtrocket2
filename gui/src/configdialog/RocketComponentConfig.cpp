#include "QtRocket/gui/configdialog/RocketComponentConfig.h"

#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QLineEdit>
#include <QList>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QTextOption>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/binding/StringBinding.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/FieldEnabling.h"
#include "QtRocket/gui/widgets/IconToggleButton.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/TextCommitter.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// A check box's property of every component (RocketComponent's is<Name> and set<Name>).
[[nodiscard]] BoolSource componentFlag(bool (RocketComponent::*is)() const,
                                       void (RocketComponent::*set)(bool))
{
    return BoolSource::of<RocketComponent>({
        .get = [is](const RocketComponent& component) { return (component.*is)(); },
        .set = [set](RocketComponent& component, const bool& value) { (component.*set)(value); },
    });
}

/// A number property of every component (RocketComponent's get<Name> and set<Name>).
[[nodiscard]] DoubleSource componentNumber(double (RocketComponent::*get)() const,
                                           void (RocketComponent::*set)(double))
{
    return DoubleSource::of<RocketComponent>({
        .get = [get](const RocketComponent& component) { return (component.*get)(); },
        .set = [set](RocketComponent& component, const double& value) { (component.*set)(value); },
    });
}

/// What one block of the Override tab shows (RocketComponentConfig.overrideTab()).
struct OverrideBlock
{
    /// The check box: its text, tooltip, source and property.
    QString    checkText;
    QString    checkTip;
    BoolSource checkSource;
    QString    checkProperty;
    /// "Override for all subcomponents": its tooltip, source and property.
    QString    subTip;
    BoolSource subSource;
    QString    subProperty;
    /// The ancestor that overrides the value, or null; the text of the bold label and its tooltip
    /// for it, and whether that text is rich text (the CD's).
    const RocketComponent* overriddenBy = nullptr;
    QString                overriddenByText;
    QString                overriddenByTip;
    bool                   richText = false;
    /// The number field.
    std::shared_ptr<DoubleBinding> value;
    QString                        valueProperty;
    ConfigFields::NumberRowOptions options;
};

/// @p text as rich text of an HTML label: the name of a component inserted escaped.
[[nodiscard]] QString withName(const QString& text, const RocketComponent& component, bool richText)
{
    const QString name = QtStrings::fromUtf8(component.getName());
    return text.arg(richText ? name.toHtmlEscaped() : name);
}

/// The calculated CDs of the selected components before a switch of their CD override (the
/// source of "Override coefficient of drag"): a CD override of one component changes the
/// calculated CD of others (the force analysis is the whole rocket's), and apply() switches the
/// others first, so every CD is calculated when apply() reaches the first component, before any
/// of them changes. The pointers live only during that apply(): they are forgotten after the
/// main component, the last one apply() reaches.
class CalculatedCDs
{
public:
    explicit CalculatedCDs(const std::shared_ptr<ComponentSelection>& selection)
      : m_selection(selection)
    {
    }

    /// The calculated CD of @p component, which apply() reached; @p switchingOn: the override is
    /// being switched on (nothing is calculated otherwise).
    [[nodiscard]] std::optional<double> of(const RocketComponent& component, double mach,
                                           bool switchingOn)
    {
        const std::shared_ptr<ComponentSelection> selection = m_selection.lock();
        if (selection == nullptr || !selection->isValid())
        {
            m_values.clear();
            return std::nullopt;
        }
        const std::span<RocketComponent* const> others = selection->getOthers();
        const RocketComponent&                  main   = selection->getMain();
        const RocketComponent*                  first  = others.empty() ? &main : others.front();
        if (&component == first)
        {
            m_values.clear();
            if (switchingOn)
            {
                for (const RocketComponent* other : others)
                {
                    m_values.emplace_back(other, ComponentDrag::getOverrideCD(*other, mach));
                }
                m_values.emplace_back(&main, ComponentDrag::getOverrideCD(main, mach));
            }
        }
        std::optional<double> cd;
        for (const auto& [target, value] : m_values)
        {
            if (target == &component && value.has_value())
            {
                cd = *value;
            }
        }
        if (&component == &main)
        {
            m_values.clear();
        }
        return cd;
    }

private:
    std::weak_ptr<ComponentSelection>                              m_selection;
    std::vector<std::pair<const RocketComponent*, Result<double>>> m_values;
};

}  // namespace

RocketComponentConfig::RocketComponentConfig(OpenRocketDocument&                 document,
                                             const GuiServices&                  services,
                                             std::shared_ptr<ComponentSelection> selection,
                                             QWidget*                            parent)
  : QWidget(parent),
    m_document(&document),
    m_services(services),
    m_scope(*this, std::move(selection))
{
    const std::shared_ptr<ComponentSelection>& valid = m_scope.selection();
    if (valid == nullptr || !valid->isValid())
    {
        bug("A component configuration needs a valid selection");
    }
    const RocketComponent& main = valid->getMain();
    m_mainKind                  = main.kind();

    // Whether the other selected components are of the same class and massive.
    m_allSameType = valid->isAllOfOneKind();
    m_allMassive  = main.isMassive();
    if (m_allMassive)
    {
        for (const RocketComponent* other : valid->getOthers())
        {
            if (!other->isMassive())
            {
                m_allMassive = false;
                break;
            }
        }
    }

    m_layout = makeChild<QVBoxLayout>(this);
    addHeader();
    m_tabs = makeChild<QTabWidget>(this);
    m_tabs->setObjectName(QStringLiteral("tabbedPane"));
    m_layout->addWidget(m_tabs, 1);
    addOverrideTab();
    addCommentTab();
    addButtonRow();
    updateFields();
}

RocketComponentConfig::~RocketComponentConfig() = default;

// ------------------------------------------------------------------------------------ header

void RocketComponentConfig::addHeader()
{
    auto* header       = makeChild<QWidget>(this);
    auto* headerLayout = makeChild<QHBoxLayout>(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    //: RocketCompCfg.lbl.Componentname
    auto* label = makeChild<QLabel>(header, tr("Component name:"));
    //: RocketCompCfg.lbl.Componentname.ttip
    const QString tip = tr("The component name.");
    label->setToolTip(tip);
    headerLayout->addWidget(label);

    m_nameField = makeChild<QLineEdit>(header);
    m_nameField->setObjectName(QStringLiteral("Name"));
    m_nameField->setToolTip(tip);
    // Java: JTextField(15), fifteen columns wide at least.
    m_nameField->setMinimumWidth(m_nameField->fontMetrics().averageCharWidth() * 15);
    headerLayout->addWidget(m_nameField, 1);
    label->setBuddy(m_nameField);

    m_textCommitter = makeChild<TextCommitter>(this);
    m_textCommitter->addField(*m_nameField,
                              m_scope.make<StringBinding>(selection(), componentNameSource()));
    m_layout->addWidget(header);
}

// ------------------------------------------------------------------------------------- tabs

void RocketComponentConfig::insertTab(int index, QWidget& page, const QString& title,
                                      const QString& toolTip)
{
    const int inserted = m_tabs->insertTab(index, &page, title);
    m_tabs->setTabToolTip(inserted, toolTip);
}

QString RocketComponentConfig::selectedTabName() const
{
    const int index = m_tabs->currentIndex();
    return index >= 0 ? m_tabs->tabText(index) : QString();
}

int RocketComponentConfig::selectedTabIndex() const
{
    return m_tabs->currentIndex();
}

void RocketComponentConfig::setSelectedTab(const QString& title)
{
    for (int i = 0; i < m_tabs->count(); ++i)
    {
        if (m_tabs->tabText(i) == title)
        {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }
    m_tabs->setCurrentIndex(0);
}

void RocketComponentConfig::setSelectedTabIndex(int index)
{
    m_tabs->setCurrentIndex(index);
}

// ---------------------------------------------------------------------------- override tab

DoubleSource RocketComponentConfig::overrideCDSource(const Preferences& preferences)
{
    return DoubleSource::of<RocketComponent>({
        .getChecked =
            [&preferences](const RocketComponent& component) {
                return ComponentDrag::getOverrideCD(component, preferences.getDefaultMach());
            },
        .set = [](RocketComponent& component, const double& cd) { component.setOverrideCD(cd); },
    });
}

BoolSource RocketComponentConfig::cdOverriddenSource(
    const Preferences& preferences, const std::shared_ptr<ComponentSelection>& selection)
{
    const auto snapshot = std::make_shared<CalculatedCDs>(selection);
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return component.isCDOverridden(); },
        .set =
            [&preferences, snapshot](RocketComponent& component, const bool& overridden) {
                // The override starts from the calculated CD (Java's getOverrideCD() stores it
                // whenever it is read; the core here does not): each component its own.
                const std::optional<double> cd =
                    snapshot->of(component, preferences.getDefaultMach(), overridden);
                if (overridden && !component.isCDOverridden() && cd.has_value())
                {
                    component.setOverrideCD(*cd);
                }
                component.setCDOverridden(overridden);
            },
    });
}

std::optional<double> RocketComponentConfig::overrideCGExtent(const RocketComponent& component)
{
    const auto lengthOf = [](const RocketComponent& c) {
        if (const auto* fins = dynamic_cast<const FinSet*>(&c))
        {
            return fins->getInstanceBoundingBox().span().x;
        }
        return c.getLength();
    };
    if (component.getChildCount() > 0)
    {
        // Java: minL from Double.MAX_VALUE, maxL from Double.MIN_VALUE (the smallest positive
        // double), over the subtree with the component itself.
        double minL = std::numeric_limits<double>::max();
        double maxL = std::numeric_limits<double>::denorm_min();
        for (const RocketComponent& c : component.subtree(true))
        {
            const double compPos = c.getAxialOffset(AxialMethod::ABSOLUTE);
            // Java's comparisons: a NaN keeps the bound (std::min() may become minsd on MSVC).
            // NOLINTNEXTLINE(readability-use-std-min-max): Java's test, NaN included
            if (compPos < minL)
            {
                minL = compPos;
            }
            const double compLen = lengthOf(c);
            // NOLINTNEXTLINE(readability-use-std-min-max): Java's test, NaN included
            if (compPos + compLen > maxL)
            {
                maxL = compPos + compLen;
            }
        }
        return maxL - minL;
    }
    if (dynamic_cast<const FinSet*>(&component) != nullptr)
    {
        return lengthOf(component);
    }
    return std::nullopt;
}

void RocketComponentConfig::addOverrideTab()
{
    auto* page = makeChild<QWidget>(this);
    auto* grid = makeChild<QGridLayout>(page);
    addMassOverride(*grid);
    addCGOverride(*grid);
    addCDOverride(*grid);

    // The mass and CG overrides do not include motors.
    grid->setRowStretch(3, 1);
    auto* note = makeChild<StyledLabel>(
        page,
        //: RocketCompCfg.lbl.longB1
        tr("<html>The overridden mass and center of gravity does not include motors. <br>") +
            //: RocketCompCfg.lbl.longB2
            tr("The center of gravity is measured from the front end of the") + QLatin1Char(' ') +
            enumText(m_mainKind).toLower() + QLatin1Char('.'),
        -1.0);
    note->setObjectName(QStringLiteral("overrideNote"));
    grid->addWidget(note, 4, 0, 1, 4, Qt::AlignBottom | Qt::AlignLeft);
    grid->setColumnStretch(3, 1);

    //: RocketCompCfg.tab.Override
    const int index = m_tabs->addTab(page, tr("Override"));
    //: RocketCompCfg.tab.Override.ttip
    m_tabs->setTabToolTip(index, tr("Override auto-calculated properties."));
}

namespace
{

/// Adds @p block in @p row of @p grid (the mass, CG or CD block of the Override tab).
void addOverrideBlock(BindingScope& scope, QGridLayout& grid, int row, const OverrideBlock& block)
{
    QWidget* page   = grid.parentWidget();
    auto*    checks = makeChild<QWidget>(page);
    // Column 0 indents what follows the check box (Java: gapleft 25lp).
    auto*         checksGrid = makeChild<QGridLayout>(checks);
    constexpr int kIndent    = 25;
    checksGrid->setContentsMargins(0, 0, 0, 0);
    checksGrid->setColumnMinimumWidth(0, kIndent);

    auto* check = makeChild<BoolCheckBox>(
        checks, scope.make<BoolBinding>(scope.selection(), block.checkSource), block.checkText);
    check->setObjectName(block.checkProperty);
    check->setToolTip(block.checkTip);
    checksGrid->addWidget(check, 0, 0, 1, 2);

    // Override for all subcomponents: one point smaller, enabled while overridden.
    auto* checkSub =
        makeChild<BoolCheckBox>(checks, scope.make<BoolBinding>(scope.selection(), block.subSource),
                                //: RocketCompCfg.checkbox.OverrideSubcomponents
                                RocketComponentConfig::tr("Override for all subcomponents"));
    checkSub->setObjectName(block.subProperty);
    QFont smallFont = checkSub->font();
    if (smallFont.pointSizeF() > 1.0)
    {
        smallFont.setPointSizeF(smallFont.pointSizeF() - 1.0);
        checkSub->setFont(smallFont);
    }
    checkSub->setToolTip(block.subTip);
    check->addEnableFollower(*checkSub);
    checksGrid->addWidget(checkSub, 1, 1);

    // Overridden by an ancestor: a bold label, the block disabled.
    if (block.overriddenBy != nullptr)
    {
        auto* label = makeChild<StyledLabel>(
            checks, withName(block.overriddenByText, *block.overriddenBy, block.richText), 0.0,
            StyledLabel::Style::BOLD);
        label->setTextFormat(block.richText ? Qt::RichText : Qt::PlainText);
        label->setFontColor(ThemeColors::color(ThemeColors::Key::DARK_ERROR));
        label->setToolTip(withName(block.overriddenByTip, *block.overriddenBy, block.richText));
        label->setObjectName(block.checkProperty + QStringLiteral("By"));
        checksGrid->addWidget(label, 2, 1);
    }
    grid.addWidget(checks, row, 0, Qt::AlignTop);

    const ConfigFields::NumberWidgets widgets = ConfigFields::addNumberRow(
        grid, row, QString(), block.value, block.valueProperty, block.options);
    QList<QWidget*> fields{widgets.spinBox};
    if (widgets.unitSelector != nullptr)
    {
        fields.append(widgets.unitSelector);
    }
    if (widgets.slider != nullptr)
    {
        fields.append(widgets.slider);
    }
    for (QWidget* field : fields)
    {
        check->addEnableFollower(*field);
    }

    if (block.overriddenBy != nullptr)
    {
        check->setFieldEnabled(false);
        check->removeEnableFollower(*checkSub);
        FieldEnabling::setFieldEnabled(*checkSub, false);
        for (QWidget* field : fields)
        {
            check->removeEnableFollower(*field);
            FieldEnabling::setFieldEnabled(*field, false);
        }
    }
}

}  // namespace

void RocketComponentConfig::addMassOverride(QGridLayout& grid)
{
    const RocketComponent& main = selection()->getMain();
    addOverrideBlock(
        m_scope, grid, 0,
        {
            //: RocketCompCfg.checkbox.Overridemass
            .checkText = tr("Override mass:"),
            //: RocketCompCfg.checkbox.Overridemass.ttip
            .checkTip      = tr("Overrides the mass of the component calculated by the component's "
                                "volume and density."),
            .checkSource   = componentFlag(&RocketComponent::isMassOverridden,
                                           &RocketComponent::setMassOverridden),
            .checkProperty = QStringLiteral("MassOverridden"),
            //: RocketCompCfg.checkbox.OverrideSubcomponents.Mass.ttip
            .subTip       = tr("<html>Overrides the aggregate mass of this component <br>and its "
                               "subcomponents with the mass of this component.</html>"),
            .subSource    = componentFlag(&RocketComponent::isSubcomponentsOverriddenMass,
                                          &RocketComponent::setSubcomponentsOverriddenMass),
            .subProperty  = QStringLiteral("SubcomponentsOverriddenMass"),
            .overriddenBy = main.getMassOverriddenBy(),
            //: RocketCompCfg.lbl.MassOverriddenBy (%s: the name of the overriding component)
            .overriddenByText = tr("Mass overridden by %1"),
            //: RocketCompCfg.lbl.MassOverriddenBy.ttip
            .overriddenByTip = tr("The mass of this component is determined by the mass override "
                                  "value of %1"),
            .richText        = false,
            .value = m_scope.make<DoubleBinding>(selection(),
                                                 componentNumber(&RocketComponent::getOverrideMass,
                                                                 &RocketComponent::setOverrideMass),
                                                 unitGroup(UnitGroupId::MASS),
                                                 DoubleBinding::Options{.min = 0.0}),
            .valueProperty = QStringLiteral("OverrideMass"),
            .options       = {.slider = SliderMapping::fromRange(0.0, 0.03, 1.0)},
        });
}

void RocketComponentConfig::addCGOverride(QGridLayout& grid)
{
    const RocketComponent& main = selection()->getMain();
    // The slider ends at the extent of the component and its children (constant) or at its length
    // (Java: DoubleModel(component, "Length", UNITS_LENGTH, 0)).
    ConfigFields::NumberRowOptions options;
    if (const std::optional<double> extent = overrideCGExtent(main); extent.has_value())
    {
        options.slider = SliderMapping::linear(0.0, *extent);
    }
    else
    {
        const auto length = m_scope.make<DoubleBinding>(
            selection(),
            DoubleSource::of<RocketComponent>(
                {.get = [](const RocketComponent& component) { return component.getLength(); }}),
            unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{.min = 0.0});
        options.slider = SliderMapping::linear(0.0, length->value());
        options.bounds = {.min = nullptr, .max = length};
    }
    addOverrideBlock(
        m_scope, grid, 1,
        {
            //: RocketCompCfg.checkbox.Overridecenterofgrav
            .checkText = tr("Override center of gravity CG:"),
            //: RocketCompCfg.checkbox.Overridecenterofgrav.ttip
            .checkTip = tr("Overrides the component's center of gravity (CG) location."),
            .checkSource =
                componentFlag(&RocketComponent::isCGOverridden, &RocketComponent::setCGOverridden),
            .checkProperty = QStringLiteral("CGOverridden"),
            //: RocketCompCfg.checkbox.OverrideSubcomponents.CG.ttip
            .subTip = tr("<html>Overrides the center of gravity (CG) of this component <br>and its "
                         "subcomponents with the CG of this component.</html>"),
            .subSource    = componentFlag(&RocketComponent::isSubcomponentsOverriddenCG,
                                          &RocketComponent::setSubcomponentsOverriddenCG),
            .subProperty  = QStringLiteral("SubcomponentsOverriddenCG"),
            .overriddenBy = main.getCGOverriddenBy(),
            //: RocketCompCfg.lbl.CGOverriddenBy (%s: the name of the overriding component)
            .overriddenByText = tr("CG overridden by %1"),
            //: RocketCompCfg.lbl.CGOverriddenBy.ttip
            .overriddenByTip =
                tr("The CG of this component is determined by the CG override value of %1"),
            .richText = false,
            .value    = m_scope.make<DoubleBinding>(
                selection(),
                componentNumber(&RocketComponent::getOverrideCGX, &RocketComponent::setOverrideCGX),
                unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{.min = 0.0}),
            .valueProperty = QStringLiteral("OverrideCGX"),
            .options       = options,
        });
}

void RocketComponentConfig::addCDOverride(QGridLayout& grid)
{
    const RocketComponent& main        = selection()->getMain();
    const Preferences&     preferences = m_services.preferences();
    //: RocketCompCfg.lbl.CDOverriddenBy (%s: the name of the overriding component)
    const QString overriddenBy = tr("<html>C<sub>D</sub> overridden by %1");
    addOverrideBlock(
        m_scope, grid, 2,
        {
            //: RocketCompCfg.checkbox.SetDragCoeff
            .checkText = tr("<html>Override coefficient of drag C<sub>D</sub>:</html>"),
            //: RocketCompCfg.checkbox.SetDragCoeff.ttip
            .checkTip      = tr("<html>Overrides the component's coefficient of drag "
                                "C<sub>D</sub>.</html>"),
            .checkSource   = cdOverriddenSource(preferences, selection()),
            .checkProperty = QStringLiteral("CDOverridden"),
            //: RocketCompCfg.checkbox.OverrideSubcomponents.CD.ttip
            .subTip       = tr("<html>Overrides the coefficient of drag (C<sub>D</sub>) of this "
                               "component <br>and its subcomponents with the C<sub>D</sub> of this "
                               "component.</html>"),
            .subSource    = componentFlag(&RocketComponent::isSubcomponentsOverriddenCD,
                                          &RocketComponent::setSubcomponentsOverriddenCD),
            .subProperty  = QStringLiteral("SubcomponentsOverriddenCD"),
            .overriddenBy = main.getCDOverriddenBy(),
            .overriddenByText = overriddenBy,
            // Java gives this label its own text as the tooltip too.
            .overriddenByTip = overriddenBy,
            .richText        = true,
            .value         = m_scope.make<DoubleBinding>(selection(), overrideCDSource(preferences),
                                                         unitGroup(UnitGroupId::COEFFICIENT),
                                                         DoubleBinding::Options{}),
            .valueProperty = QStringLiteral("OverrideCD"),
            .options       = {.slider = SliderMapping::linear(-1.0, 1.0), .unitSelector = false},
        });
}

// ----------------------------------------------------------------------------- comment tab

void RocketComponentConfig::addCommentTab()
{
    auto* page   = makeChild<QWidget>(this);
    auto* layout = makeChild<QVBoxLayout>(page);
    layout->addWidget(makeChild<StyledLabel>(
        page,
        //: RocketCompCfg.lbl.Commentsonthe
        tr("Comments on the") + QLatin1Char(' ') + enumText(m_mainKind) + QLatin1Char(':'),
        StyledLabel::Style::BOLD));

    m_commentArea = makeChild<QPlainTextEdit>(page);
    m_commentArea->setObjectName(QStringLiteral("Comment"));
    m_commentArea->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_commentArea->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout->addWidget(m_commentArea, 1);
    m_textCommitter->addField(*m_commentArea,
                              m_scope.make<StringBinding>(selection(), componentCommentSource()));

    //: RocketCompCfg.tab.Comment
    const int index = m_tabs->addTab(page, tr("Comment"));
    //: RocketCompCfg.tab.Comment.ttip
    m_tabs->setTabToolTip(index, tr("Specify a comment for the component"));
}

// ------------------------------------------------------------------------------ button row

void RocketComponentConfig::addButtonRow()
{
    auto* row   = makeChild<QWidget>(this);
    m_buttonRow = makeChild<QHBoxLayout>(row);
    m_buttonRow->setContentsMargins(0, 0, 0, 0);

    addComponentInfo(*m_buttonRow);

    // The multi-component edit label and the mass label.
    m_multiCompEditLabel = makeChild<StyledLabel>(row, QString(), -1.0, StyledLabel::Style::BOLD);
    m_multiCompEditLabel->setObjectName(QStringLiteral("multiCompEditLabel"));
    m_multiCompEditLabel->setFontColor(ThemeColors::color(ThemeColors::Key::MULTI_COMP_EDIT));
    m_buttonRow->addWidget(m_multiCompEditLabel);
    m_infoLabel = makeChild<StyledLabel>(row, QString(), -1.0);
    m_infoLabel->setObjectName(QStringLiteral("infoLabel"));
    m_infoLabel->setTextFormat(Qt::PlainText);
    m_buttonRow->addWidget(m_infoLabel, 1);

    //: dlg.but.cancel
    m_cancelButton = makeChild<QPushButton>(row, tr("Cancel"));
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    //: RocketCompCfg.btn.Cancel.ttip
    m_cancelButton->setToolTip(tr("Discard changes and close the dialog"));
    m_cancelButton->setAutoDefault(false);
    m_buttonRow->addWidget(m_cancelButton);

    //: dlg.but.ok
    m_okButton = makeChild<QPushButton>(row, tr("OK"));
    m_okButton->setObjectName(QStringLiteral("okButton"));
    //: RocketCompCfg.btn.OK.ttip
    m_okButton->setToolTip(tr("Keep changes and close the dialog"));
    m_okButton->setAutoDefault(false);
    m_buttonRow->addWidget(m_okButton);

    m_layout->addWidget(row);
}

void RocketComponentConfig::addComponentInfo(QHBoxLayout& buttons)
{
    // Not for a multi-component edit.
    if (selection()->isMultiEdit())
    {
        return;
    }
    m_componentInfo = makeChild<DescriptionArea>(
        this, QStringLiteral("<html>") + componentInfoText(m_mainKind) + QStringLiteral("</html>"),
        5);
    m_componentInfo->setObjectName(QStringLiteral("componentInfo"));
    m_componentInfo->setVisible(false);
    m_layout->addWidget(m_componentInfo);

    // The description shows while the button is NOT selected, so that the button looks enabled
    // to draw attention to it (Java).
    m_infoButton = makeChild<IconToggleButton>(buttons.parentWidget());
    m_infoButton->setObjectName(QStringLiteral("componentInfoButton"));
    //: RocketCompCfg.btn.ComponentInfo.ttip
    m_infoButton->setToolTip(tr("Show/hide informative text about this component."));
    m_infoButton->setSelectedIcon(Icons::icon(Icons::Id::HELP_ABOUT));
    m_infoButton->setIconScale(1.5);
    m_infoButton->setChecked(true);
    buttons.addWidget(m_infoButton);
    connect(m_infoButton, &IconToggleButton::toggled, this, [this](bool selected) {
        m_componentInfo->setVisible(!selected);
        // Fit the dialog to its new contents (Java: parent.pack()).
        window()->adjustSize();
    });
}

void RocketComponentConfig::addButton(QPushButton& button)
{
    button.setAutoDefault(false);
    m_buttonRow->insertWidget(m_buttonRow->indexOf(m_cancelButton), &button);
}

// ---------------------------------------------------------------------------------- fields

void RocketComponentConfig::updateFields()
{
    const std::shared_ptr<ComponentSelection>& valid = selection();
    if (valid == nullptr || !valid->isValid())
    {
        return;
    }
    const RocketComponent& main = valid->getMain();

    // The info label: the preset, then the mass (only for one component, all massive).
    QString text;
    if (m_allSameType && main.getPresetComponent() != nullptr)
    {
        const ComponentPreset& preset = *main.getPresetComponent();
        text += QtStrings::fromUtf8(preset.getManufacturer().toString()) + QLatin1Char(' ') +
                QtStrings::fromUtf8(preset.getPartNo()) + QStringLiteral("      ");
    }
    if (m_allMassive && !valid->isMultiEdit())
    {
        //: RocketCompCfg.lbl.Componentmass
        text += tr("Component mass:") + QLatin1Char(' ') +
                QtStrings::defaultUnitText(UnitGroupId::MASS, main.getComponentMass());
        QString overrideText;
        if (main.isMassOverridden())
        {
            //: RocketCompCfg.lbl.overriddento
            overrideText = tr("(overridden to") + QLatin1Char(' ') +
                           QtStrings::defaultUnitText(UnitGroupId::MASS, main.getOverrideMass()) +
                           QLatin1Char(')');
        }
        if (main.getMassOverriddenBy() != nullptr)
        {
            //: RocketCompCfg.lbl.overriddenby
            overrideText = tr("(overridden by") + QLatin1Char(' ') +
                           QtStrings::fromUtf8(main.getMassOverriddenBy()->getName()) +
                           QLatin1Char(')');
        }
        if (!overrideText.isEmpty())
        {
            text += QLatin1Char(' ') + overrideText;
        }
        m_infoLabel->setText(text);
    }
    else
    {
        m_infoLabel->setText(QString());
    }

    // The multi-component edit label, whose tooltip names the components.
    if (valid->isMultiEdit())
    {
        //: ComponentCfgDlg.MultiComponentEdit
        m_multiCompEditLabel->setText(tr("Multi-component edit"));
        QList<QString> names{QtStrings::fromUtf8(main.getName()).toHtmlEscaped()};
        for (const RocketComponent* other : valid->getOthers())
        {
            names.append(QtStrings::fromUtf8(other->getName()).toHtmlEscaped());
        }
        //: ComponentCfgDlg.MultiComponentEdit.ttip
        m_multiCompEditLabel->setToolTip(
            tr("<html>You are editing the following components: <br>") +
            names.join(QStringLiteral(", ")));
    }
    else
    {
        // Java hides the description again on every change.
        if (m_infoButton != nullptr)
        {
            m_componentInfo->setVisible(false);
            m_infoButton->setChecked(true);
            m_infoButton->setVisible(true);
        }
        m_multiCompEditLabel->setText(QString());
    }
}

bool RocketComponentConfig::commitAll()
{
    return m_scope.commitAll();
}

void RocketComponentConfig::release() noexcept
{
    m_scope.release();
}

void RocketComponentConfig::orderFocus()
{
    // The focusable widgets of the panel in the order of the focus chain, without the buttons.
    QWidget* last = nullptr;
    for (QWidget* widget = nextInFocusChain(); widget != this; widget = widget->nextInFocusChain())
    {
        if (widget != m_cancelButton && widget != m_okButton && isAncestorOf(widget) &&
            widget->focusPolicy() != Qt::NoFocus)
        {
            last = widget;
        }
    }
    if (last != nullptr)
    {
        QWidget::setTabOrder(last, m_cancelButton);
    }
    QWidget::setTabOrder(m_cancelButton, m_okButton);
}

QString RocketComponentConfig::componentInfoText(ComponentKind kind)
{
    switch (kind)
    {
        case ComponentKind::ROCKET:
            //: ComponentInfo.Rocket
            return tr(
                "This is your rocket. Nothing more to say about it. Have fun designing and "
                "building it! :)");
        case ComponentKind::AXIAL_STAGE:
            //: ComponentInfo.AxialStage
            return tr(
                "A <b>stage</b> is a <b>section</b> of the model rocket that contains "
                "<b>motors</b> which ignite successively and <b>separate</b> after motor "
                "burnout. <br>Each stage must safely descend after separation. The main "
                "stage, often called the <b>'Sustainer'</b>, usually descends with a "
                "<b>recovery device</b>.");
        case ComponentKind::PARALLEL_STAGE:
            //: ComponentInfo.ParallelStage
            return tr(
                "A <b>booster</b> is a fictional component that acts as an <b>attachment</b> "
                "point to a <b>body tube</b> and can <b>separate</b> from that body tube at "
                "a specified time during flight. <br>The most common use of a booster is "
                "the attachment of a <b>strap-on booster</b>.");
        case ComponentKind::POD_SET:
            //: ComponentInfo.PodSet
            return tr(
                "A <b>pod</b> is a fictional component that acts as an <b>attachment</b> "
                "point to a <b>body tube</b>. <br>Once defined, other components can be "
                "attached to the pod to create complex rocket geometries.");
        case ComponentKind::BODY_TUBE:
            //: ComponentInfo.BodyTube
            return tr(
                "A <b>body tube</b> is primarily used for the <b>structure</b> of the "
                "exterior body of the model rocket.");
        case ComponentKind::TRANSITION:
            //: ComponentInfo.Transition
            return tr(
                "A <b>transition</b> is the part of the airframe that <b>connects</b> two "
                "<b>body tubes</b> having <b>different</b> outside <b>diameters</b>. <br>Both "
                "ends of a transition are cut down to form an internal shoulder that slides "
                "inside of each body tube to hold the transition in place at both ends.");
        case ComponentKind::NOSE_CONE:
            //: ComponentInfo.NoseCone
            return tr(
                "A <b>nose cone</b> is the <b>front end</b> of the rocket airframe. Nose "
                "cones vary widely in shape. <br>The back end can be cut down to form an "
                "<b>internal shoulder</b> that slides inside of a body tube to hold the nose "
                "cone in place.");
        case ComponentKind::TRAPEZOID_FIN_SET:
            //: ComponentInfo.TrapezoidFinSet
            return tr(
                "Fins help <b>stabilize</b> the rocket. The most common fin shape is "
                "<b>trapezoidal</b>, four sides with parallel root and tip chords (the edge "
                "that glues to the body tube and the outer edge).");
        case ComponentKind::ELLIPTICAL_FIN_SET:
            //: ComponentInfo.EllipticalFinSet
            return tr(
                "Fins help <b>stabilize</b> the rocket. The most efficient fin shape is "
                "<b>elliptical</b>. This shape induces the <b>least amount of drag</b> of "
                "any fin shape. It is commonly used for competition flying events.");
        case ComponentKind::FREEFORM_FIN_SET:
            //: ComponentInfo.FreeformFinSet
            return tr(
                "Fins help <b>stabilize</b> the rocket. The most versatile fin component is "
                "the <b>freeform fin</b>. This fin component allows the creation of "
                "virtually any solid fin shape, with the ability to manually enter data "
                "points or import a shape from an image file.");
        case ComponentKind::TUBE_FIN_SET:
            //: ComponentInfo.TubeFinSet (OpenRocket's text names OpenRocket)
            return tr(
                "<b>Tube fins</b> are used to keep the model rocket going <b>straight after "
                "launch</b>. Tube fins vary in length and diameter, and may have either "
                "straight or curved ends. <br>Currently, QtRocket only supports "
                "straight-perpendicular cut ends.");
        case ComponentKind::LAUNCH_LUG:
            //: ComponentInfo.LaunchLug
            return tr(
                "A <b>launch lug</b> is the most common guide used to keep smaller model "
                "rockets from <b>changing orientation</b> until the rocket leaves the end of "
                "the launch rod.");
        case ComponentKind::RAIL_BUTTON:
            //: ComponentInfo.RailButton
            return tr(
                "<b>Rail buttons</b> keep the model rocket from <b>changing orientation</b> "
                "when it launches. Rail buttons are most commonly used on larger model "
                "rockets, in pairs, <b>instead</b> of <b>launch lugs</b>. <br>They can be "
                "mounted to the outside of the body tube with a screw and internal nut. "
                "<br>A rail button looks like two washers with a spacer in between; one "
                "washer is against the body tube and the other slides inside the launch "
                "rail.");
        case ComponentKind::INNER_TUBE:
            //: ComponentInfo.InnerTube
            return tr(
                "<b>Inner tubes</b> are most commonly used for <b>motor tubes</b>, but may "
                "also be part of other internal systems, such as an ejection baffles and "
                "adjustable weight designs. <br>Inner tubes may be positioned radially "
                "inside of a body tube and, when used as a motor tube, may be clustered.");
        case ComponentKind::TUBE_COUPLER:
            //: ComponentInfo.TubeCoupler
            return tr(
                "A <b>coupler</b> is a short tube that <b>connects</b> two <b>tubes</b> with "
                "the <b>same diameters</b>. <br>In model rockets that use electronics, a "
                "coupler may be used for the <b>avionics bay</b>. <br>The tubes are often "
                "glued to the coupler to stop them from sliding.");
        case ComponentKind::ENGINE_BLOCK:
            //: ComponentInfo.EngineBlock
            return tr(
                "An <b>engine block stops</b> the <b>motor</b> from <b>moving forward</b> in "
                "the motor mount tube. <br>In order to add a motor, create a <b>body "
                "tube</b> or <b>inner tube</b> and mark it as a motor mount in the "
                "<em>Motor</em> tab.");
        case ComponentKind::CENTERING_RING:
            //: ComponentInfo.CenteringRing
            return tr(
                "<b>Centering rings</b> are used to <b>position</b> one or more <b>inner "
                "tubes</b> inside of a <b>body tube</b>. <br>Fin sets have an <b>automatic "
                "fin tab</b> feature for determining an ideal fin tab configuration based "
                "on an inner tube positioned below the root chord of the fins. This also "
                "requires two centering below the root chord.");
        case ComponentKind::BULKHEAD:
            //: ComponentInfo.Bulkhead
            return tr(
                "A <b>bulkhead</b> is a disk that may or may not have a small hole in the "
                "center. Bulkheads are used to <b>seal</b> one area of a model rocket from "
                "another or to mount recovery system eyes for <b>shock cord "
                "attachment</b>, such as nose cone caps and avionics bay lids.");
        case ComponentKind::MASS_COMPONENT:
            //: ComponentInfo.MassComponent (OpenRocket's text names OpenRocket)
            return tr(
                "A <b>mass component</b> may be used to simulate a <b>component</b> that is "
                "<b>not otherwise listed</b>. <br>In QtRocket, this component type may be "
                "designated for instance as an <b>altimeter</b>, a <b>flight computer</b>, "
                "a <b>battery</b>\u2026 Each designation has its own unique icon to ease "
                "identification.");
        case ComponentKind::SHOCK_CORD:
            //: ComponentInfo.ShockCord
            return tr(
                "A <b>shock cord</b> attaches the <b>nose cone</b> and <b>recovery "
                "device</b> to the <b>body tube</b>, keeping the model rocket together "
                "during <b>descent</b>. <br>Smaller rockets commonly have short elastic "
                "shock cords, while larger rockets have long shock cords made from "
                "materials such as Nylon or heat resistant Kevlar webbing.");
        case ComponentKind::PARACHUTE:
            //: ComponentInfo.Parachute
            return tr(
                "The <b>parachute</b> is the most common <b>recovery device</b>. Smal model "
                "rocket parachutes are made of thin, light-weight materials, such as "
                "polyethylene or Mylar, while larger parachutes are most commonly made of "
                "rip-stop nylon. Parachutes may be decorated in bright and contrasting "
                "colors/patterns to improve visibility. <br><b>Shroud lines</b> attach the "
                "parachute <b>canopy</b> to the <b>shock cord</b>. The shroud lines are "
                "sometimes brought together and attached using a swivel.");
        case ComponentKind::STREAMER:
            //: ComponentInfo.Streamer
            return tr(
                "A <b>streamer</b> is a <b>recovery device</b> that, when ejected, unrolls "
                "and <b>slows</b> the model <b>rocket</b> during descent. <br>A streamer is "
                "a long, narrow, <b>rectangular strip</b> of crepe paper or thin plastic "
                "film. The width and length of the streamer are adjusted to match the "
                "weight of the rocket, but a <b>10 to 1 length to width ratio</b> is "
                "considered the best for a streamer.");
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace QtRocket::Gui
