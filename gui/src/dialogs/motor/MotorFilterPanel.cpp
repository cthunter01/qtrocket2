#include "QtRocket/gui/dialogs/motor/MotorFilterPanel.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMargins>
#include <QObject>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/dialogs/motor/CheckList.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/RangeSlider.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/motor/chooser/MotorFilterSliders.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// MigLayout's paragraph gap ("gapleft para"), before the check boxes.
constexpr int kParagraphGap = 12;
/// The points the small texts are smaller (GUIUtil.changeFontSize(component, -1)).
constexpr double kSmallerFont = -1.0;
/// The manufacturer list's preferred size (its scroll pane's setPreferredSize(60, 60)).
constexpr QSize kManufacturerListSize{60, 60};
/// The panel's border (BorderFactory.createEmptyBorder(1, 1, 1, 1)).
constexpr int kBorder = 1;
/// The length slider's values per metre: it runs in whole millimetres.
constexpr double kLengthSliderScale = 1000.0;

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motor filter panel");
}

/// A font that differs from the one it is set on in its weight only: bold or not.
[[nodiscard]] QFont weightOnly(bool bold)
{
    QFont font;
    font.setBold(bold);
    return font;
}

/// Changes @p widget's font by @p points points (GUIUtil.changeFontSize()).
void changeFontSize(QWidget& widget, double points)
{
    QFont font = widget.font();
    if (font.pointSizeF() > 0.0)
    {
        font.setPointSizeF(std::max(1.0, font.pointSizeF() + points));
    }
    else
    {
        // A font sized in pixels: as many pixels as points.
        font.setPixelSize(std::max(1, font.pixelSize() + static_cast<int>(points)));
    }
    widget.setFont(font);
}

/// A titled box whose title is bold (GUIUtil.changeFontStyle(border, Font.BOLD)) and whose
/// content, the widget returned, is not.
[[nodiscard]] QWidget& makeTitledBox(QWidget& parent, const QString& title, QGroupBox*& box)
{
    box = makeChild<QGroupBox>(&parent, title);
    box->setFont(weightOnly(true));
    auto* boxLayout = makeChild<QVBoxLayout>(box);
    auto* content   = makeChild<QWidget>(box);
    content->setFont(weightOnly(false));
    boxLayout->addWidget(content);
    return *content;
}

/// A row of @p parent without margins, for @p parent's vertical layout @p layout: its
/// horizontal layout.
[[nodiscard]] QHBoxLayout& addRow(QVBoxLayout& layout, QWidget& parent)
{
    auto* row       = makeChild<QWidget>(&parent);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    layout.addWidget(row);
    return *rowLayout;
}

/// @p widget, a child of @p parent, in a row of @p parent's vertical layout @p layout after the
/// paragraph gap ("gapleft para").
void addIndented(QVBoxLayout& layout, QWidget& parent, QWidget& widget)
{
    QHBoxLayout& row = addRow(layout, parent);
    row.addSpacing(kParagraphGap);
    row.addWidget(&widget, 1);
}

/// The labels of the "Total Impulse" slider: the impulse classes' names, A to O (impulseLabels).
[[nodiscard]] std::map<int, QString> impulseLabels()
{
    std::map<int, QString> labels;
    int                    value = 0;
    for (const ImpulseClass impulseClass : kAllImpulseClasses)
    {
        labels.emplace(value++, QtStrings::fromUtf8(toString(impulseClass)));
    }
    return labels;
}

/// The labels of the "Nominal Diameter" slider in the default motor dimension unit
/// (scaleDiameterLabels()).
[[nodiscard]] std::map<int, QString> diameterLabels()
{
    std::map<int, QString> labels;
    int                    value = 0;
    for (const std::string& label : MotorFilterSliders::diameterLabels())
    {
        labels.emplace(value++, QtStrings::fromUtf8(label));
    }
    return labels;
}

/// The highest value of the "Total Impulse" slider, the last impulse class's.
constexpr int kLastImpulseClass = static_cast<int>(kAllImpulseClasses.size()) - 1;
/// The highest value of the "Nominal Diameter" slider, the "+" stop's.
constexpr int kLastDiameterStop = static_cast<int>(MotorFilterSliders::kMotorDiameters.size()) - 1;

}  // namespace

MotorFilterPanel::MotorFilterPanel(ThrustCurveMotorSelection& selection, Preferences& preferences,
                                   QWidget* parent)
  : QWidget(parent),
    m_selection(&selection),
    m_preferences(&preferences),
    m_manufacturers(selection.getManufacturers()),
    // The limit boxes as stored.
    m_limitDiameter(MotorPreferences::isLimitDiameter(preferences)),
    m_limitByLength(MotorPreferences::isLimitLength(preferences))
{
    setObjectName(QStringLiteral("motorFilterPanel"));

    // The excluded manufacturers as stored.
    const std::vector<const Manufacturer*> unselectedManusFromPreferences =
        MotorPreferences::getExcludedMotorManufacturers(preferences);
    m_selection->getRowFilter().setExcludedManufacturers(unselectedManusFromPreferences);

    // The main scroll pane, without a border, its bars as needed.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setContentsMargins(kBorder, kBorder, kBorder, kBorder);
    auto* scroll = makeChild<QScrollArea>(this);
    scroll->setObjectName(QStringLiteral("mainScrollPane"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(scroll);

    // "fill, ins n 0 0 0", the rows "[][grow][][][]".
    auto* content = makeChild<QWidget>(scroll);
    content->setObjectName(QStringLiteral("contentPanel"));
    auto*          contentLayout = makeChild<QVBoxLayout>(content);
    const QMargins margins       = contentLayout->contentsMargins();
    contentLayout->setContentsMargins(0, margins.top(), 0, 0);
    makeHideUsedBox(*content, *contentLayout);
    contentLayout->addWidget(&makeManufacturerBox(*content), 1);
    contentLayout->addWidget(&makeImpulseBox(*content));
    contentLayout->addWidget(&makeDimensionsBox(*content));
    scroll->setWidget(content);

    updateEnabled();
}

MotorFilterPanel::~MotorFilterPanel()
{
    // The children are deleted after this object's members: none of their signals may reach the
    // panel by then (a slider whose press ends as it goes).
    const QList<QObject*> children = findChildren<QObject*>();
    for (const QObject* child : children)
    {
        QObject::disconnect(child, nullptr, this, nullptr);
    }
}

// ------------------------------------------------------------------------------ the rows

void MotorFilterPanel::makeHideUsedBox(QWidget& content, QVBoxLayout& contentLayout)
{
    //: TCMotorSelPan.checkbox.hideUsed
    m_hideUsedBox = makeChild<QCheckBox>(&content, tr("Hide motors already used in the mount"));
    m_hideUsedBox->setObjectName(QStringLiteral("hideUsedBox"));
    changeFontSize(*m_hideUsedBox, kSmallerFont);
    connect(m_hideUsedBox, &QCheckBox::clicked, this,
            guarded([this] { hideUsedClicked(); }, context()));
    addIndented(contentLayout, content, *m_hideUsedBox);
}

QGroupBox& MotorFilterPanel::makeManufacturerBox(QWidget& content)
{
    QGroupBox* box = nullptr;
    //: TCurveMotorCol.MANUFACTURER
    QWidget& sub = makeTitledBox(content, tr("Manufacturer"), box);
    box->setObjectName(QStringLiteral("manufacturerPanel"));
    auto* subLayout = makeChild<QVBoxLayout>(&sub);
    subLayout->setContentsMargins(0, 0, 0, 0);

    // The manufacturers sorted by simple name (getManufacturers()), shown by display name, the
    // stored ones unchecked.
    m_manufacturerCheckList = makeChild<CheckList>(&sub);
    m_manufacturerCheckList->setObjectName(QStringLiteral("manufacturerCheckList"));
    QStringList texts;
    for (const Manufacturer* manufacturer : m_manufacturers)
    {
        texts.append(QtStrings::fromUtf8(manufacturer->getDisplayName()));
    }
    m_manufacturerCheckList->setItems(texts);
    std::vector<int>                        unchecked;
    const std::vector<const Manufacturer*>& excluded =
        m_selection->getRowFilter().getExcludedManufacturers();
    for (std::size_t index = 0; index < m_manufacturers.size(); ++index)
    {
        if (std::ranges::find(excluded, m_manufacturers[index]) != excluded.end())
        {
            unchecked.push_back(static_cast<int>(index));
        }
    }
    m_manufacturerCheckList->setUncheckedIndices(unchecked);
    m_manufacturerCheckList->setSizeHints(kManufacturerListSize, QSize(0, 0));
    connect(m_manufacturerCheckList, &CheckList::checksChanged, this,
            guarded([this] { manufacturersChanged(); }, context()));
    subLayout->addWidget(m_manufacturerCheckList, 1);

    //: TCMotorSelPan.btn.checkNone
    auto* clearMotors = makeChild<QPushButton>(&sub, tr("Clear All"));
    clearMotors->setObjectName(QStringLiteral("clearMotors"));
    connect(clearMotors, &QPushButton::clicked, this,
            guarded([this] { m_manufacturerCheckList->clearAll(); }, context()));
    //: TCMotorSelPan.btn.checkAll
    auto* selectMotors = makeChild<QPushButton>(&sub, tr("Select All"));
    selectMotors->setObjectName(QStringLiteral("selectMotors"));
    connect(selectMotors, &QPushButton::clicked, this,
            guarded([this] { m_manufacturerCheckList->checkAll(); }, context()));
    QHBoxLayout& buttons = addRow(*subLayout, sub);
    for (QPushButton* button : {clearMotors, selectMotors})
    {
        // Never a dialog's default button (a QPushButton in a QDialog is autoDefault, and the
        // dialog makes the first one its default when it is shown): Return in the chooser's search
        // field would clear the list. Java's buttons are plain JButtons.
        button->setAutoDefault(false);
        buttons.addWidget(button);
    }
    buttons.addStretch(1);
    return *box;
}

QGroupBox& MotorFilterPanel::makeImpulseBox(QWidget& content)
{
    QGroupBox* box = nullptr;
    //: TCurveMotorCol.TOTAL_IMPULSE
    QWidget& sub = makeTitledBox(content, tr("Total Impulse"), box);
    box->setObjectName(QStringLiteral("impulsePanel"));
    auto* subLayout = makeChild<QVBoxLayout>(&sub);
    subLayout->setContentsMargins(0, 0, 0, 0);
    m_impulseSlider = makeChild<RangeSlider>(&sub, 0, kLastImpulseClass, 0, kLastImpulseClass);
    m_impulseSlider->setObjectName(QStringLiteral("impulseSlider"));
    m_impulseSlider->setTickInterval(1);
    m_impulseSlider->setLabels(impulseLabels());
    connect(m_impulseSlider, &RangeSlider::stateChanged, this,
            guarded([this] { impulseChanged(); }, context()));
    subLayout->addWidget(m_impulseSlider);
    return *box;
}

QGroupBox& MotorFilterPanel::makeDimensionsBox(QWidget& content)
{
    QGroupBox* box = nullptr;
    //: TCMotorSelPan.MotorSize
    QWidget& sub = makeTitledBox(content, tr("Motor Dimensions"), box);
    box->setObjectName(QStringLiteral("dimensionsPanel"));
    auto* subLayout = makeChild<QVBoxLayout>(&sub);
    subLayout->setContentsMargins(0, 0, 0, 0);

    m_motorMountDimension = makeChild<QLabel>(&sub);
    m_motorMountDimension->setObjectName(QStringLiteral("motorMountDimension"));
    changeFontSize(*m_motorMountDimension, kSmallerFont);
    subLayout->addWidget(m_motorMountDimension);

    makeDiameterRows(sub, *subLayout);
    makeLengthRows(sub, *subLayout);
    return *box;
}

void MotorFilterPanel::makeDiameterRows(QWidget& box, QVBoxLayout& boxLayout)
{
    //: TCMotorSelPan.Diameter
    boxLayout.addWidget(makeChild<QLabel>(&box, tr("Nominal Diameter")));

    m_limitDiameterCheckBox =
        //: TCMotorSelPan.checkbox.limitdiameter
        makeChild<QCheckBox>(&box, tr("Limit motor diameter to mount diameter"));
    m_limitDiameterCheckBox->setObjectName(QStringLiteral("limitDiameterCheckBox"));
    m_limitDiameterCheckBox->setChecked(m_limitDiameter);
    changeFontSize(*m_limitDiameterCheckBox, kSmallerFont);
    connect(m_limitDiameterCheckBox, &QCheckBox::clicked, this,
            guarded([this] { limitDiameterClicked(); }, context()));
    addIndented(boxLayout, box, *m_limitDiameterCheckBox);

    m_diameterSlider = makeChild<RangeSlider>(&box, 0, kLastDiameterStop, 0, kLastDiameterStop);
    m_diameterSlider->setObjectName(QStringLiteral("diameterSlider"));
    m_diameterSlider->setTickInterval(1);
    m_diameterSlider->setLabels(diameterLabels());
    connect(m_diameterSlider, &RangeSlider::stateChanged, this,
            guarded([this] { diameterChanged(); }, context()));
    boxLayout.addWidget(m_diameterSlider);
}

void MotorFilterPanel::makeLengthRows(QWidget& box, QVBoxLayout& boxLayout)
{
    //: TCMotorSelPan.Length
    boxLayout.addWidget(makeChild<QLabel>(&box, tr("Length")));

    m_limitByLengthCheckBox =
        //: TCMotorSelPan.checkbox.limitlength
        makeChild<QCheckBox>(&box, tr("Limit motor length to mount length"));
    m_limitByLengthCheckBox->setObjectName(QStringLiteral("limitByLengthCheckBox"));
    m_limitByLengthCheckBox->setChecked(m_limitByLength);
    changeFontSize(*m_limitByLengthCheckBox, kSmallerFont);
    connect(m_limitByLengthCheckBox, &QCheckBox::clicked, this,
            guarded([this] { limitLengthClicked(); }, context()));
    addIndented(boxLayout, box, *m_limitByLengthCheckBox);

    // DoubleModel(filter, "MinimumLength" / "MaximumLength", UNITS_MOTOR_DIMENSIONS, 0).
    const MotorRowFilter& filter     = m_selection->getRowFilter();
    const UnitGroup&      units      = unitGroup(UnitGroupId::MOTOR_DIMENSIONS);
    constexpr double      kNoMaximum = std::numeric_limits<double>::infinity();
    m_minLengthModel =
        std::make_shared<DoubleBinding>(filter.getMinimumLength(), units, 0.0, kNoMaximum);
    m_maxLengthModel =
        std::make_shared<DoubleBinding>(filter.getMaximumLength(), units, 0.0, kNoMaximum);

    m_minLengthSpinner = makeChild<UnitSpinBox>(&box, m_minLengthModel);
    m_minLengthSpinner->setObjectName(QStringLiteral("MinimumLength"));
    m_minLengthUnitSelect = makeChild<UnitSelector>(&box, m_minLengthModel);
    m_minLengthUnitSelect->setObjectName(QStringLiteral("MinimumLength"));
    m_maxLengthSpinner = makeChild<UnitSpinBox>(&box, m_maxLengthModel);
    m_maxLengthSpinner->setObjectName(QStringLiteral("MaximumLength"));
    m_maxLengthUnitSelect = makeChild<UnitSelector>(&box, m_maxLengthModel);
    m_maxLengthUnitSelect->setObjectName(QStringLiteral("MaximumLength"));
    QHBoxLayout& fields = addRow(boxLayout, box);
    fields.addWidget(m_minLengthSpinner, 1);
    fields.addWidget(m_minLengthUnitSelect);
    fields.addWidget(m_maxLengthSpinner, 1);
    fields.addWidget(m_maxLengthUnitSelect);
    // After the spin boxes' own listeners, as Java adds them after the spinners'.
    m_minLengthConnection = m_minLengthModel->addChangeListener(
        [this] { Guard::run([this] { minimumLengthChanged(); }, context()); });
    m_maxLengthConnection = m_maxLengthModel->addChangeListener(
        [this] { Guard::run([this] { maximumLengthChanged(); }, context()); });

    m_lengthSlider = makeChild<RangeSlider>(&box, 0, MotorFilterSliders::kLengthSliderMaximum, 0,
                                            MotorFilterSliders::kLengthSliderMaximum);
    m_lengthSlider->setObjectName(QStringLiteral("lengthSlider"));
    connect(m_lengthSlider, &RangeSlider::stateChanged, this,
            guarded([this] { lengthSliderChanged(); }, context()));
    boxLayout.addWidget(m_lengthSlider);
}

// ------------------------------------------------------------------------------ the mount

void MotorFilterPanel::setMotorMount(const MotorMount* mount)
{
    // filter.setMotorMount(mount) is ThrustCurveMotorSelection::setMotorMountAndConfig()'s.
    Q_EMIT filterChanged();
    if (mount == nullptr)
    {
        m_mountLength.reset();
        m_mountDiameter.reset();
        m_motorMountDimension->clear();
    }
    else
    {
        m_mountLength   = mount->getLength();
        m_mountDiameter = mount->getMotorMountDiameter();
        //: TCMotorSelPan.MotorMountDimensions
        m_motorMountDimension->setText(
            tr("Motor mount dimensions:") + QStringLiteral(" ") +
            QtStrings::defaultUnitText(UnitGroupId::MOTOR_DIMENSIONS, *m_mountDiameter) +
            QStringLiteral(" x ") +
            QtStrings::defaultUnitText(UnitGroupId::MOTOR_DIMENSIONS, *m_mountLength));
    }
    setLimitLength();
    setLimitDiameter();
}

void MotorFilterPanel::setLimitLength()
{
    const Unit& unit = unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getDefaultUnit();
    m_minLengthModel->setCurrentUnit(unit);
    m_maxLengthModel->setCurrentUnit(unit);

    MotorPreferences::setLimitLength(*m_preferences, m_limitByLength);
    if (m_mountLength.has_value() && m_limitByLength)
    {
        m_lengthSlider->setValueAt(1, MotorFilterSliders::limitLengthStop(*m_mountLength));
        m_maxLengthModel->setValue(*m_mountLength);
        // Java's DoubleModel runs maxLengthModel's listener also when the value does not change
        // (the chooser opened for the same mount again), and the listener puts the thumb at the
        // truncated value: 69 for a mount of 69.85 mm, which the line above rounds to 70. Here the
        // listener runs only for a change (maximumLengthChanged()), so its thumb is put here.
        m_lengthSlider->setValueAt(
            1, MathUtil::javaIntCast(kLengthSliderScale * m_maxLengthModel->value()));
    }
}

void MotorFilterPanel::setLimitDiameter()
{
    MotorPreferences::setLimitDiameter(*m_preferences, m_limitDiameter);
    m_diameterSlider->setLabels(diameterLabels());
    if (m_limitDiameter && m_mountDiameter.has_value())
    {
        // The largest nominal diameter not above the mount's (a stop of -1 clamps to 0).
        m_diameterSlider->setValueAt(1, MotorFilterSliders::limitDiameterStop(*m_mountDiameter));
    }
}

void MotorFilterPanel::setHideUnavailable(bool hideUnavailable)
{
    m_selection->setHideUnavailable(hideUnavailable);
    Q_EMIT filterChanged();
}

void MotorFilterPanel::updateEnabled()
{
    // BooleanModel.addEnableComponent(component, false): enabled while the box is off.
    m_diameterSlider->setEnabled(!m_limitDiameter);
    m_minLengthSpinner->setFieldEnabled(!m_limitByLength);
    m_maxLengthSpinner->setFieldEnabled(!m_limitByLength);
    m_lengthSlider->setEnabled(!m_limitByLength);
}

// ------------------------------------------------------------------------------ the listeners

void MotorFilterPanel::hideUsedClicked()
{
    m_selection->getRowFilter().setHideUsedMotors(m_hideUsedBox->isChecked());
    Q_EMIT filterChanged();
}

void MotorFilterPanel::manufacturersChanged()
{
    const std::vector<const Manufacturer*> uncheckedManufacturers =
        manufacturersAt(m_manufacturerCheckList->uncheckedIndices());
    m_selection->getRowFilter().setExcludedManufacturers(uncheckedManufacturers);
    MotorPreferences::setExcludedMotorManufacturers(*m_preferences, uncheckedManufacturers);
    Q_EMIT filterChanged();
}

void MotorFilterPanel::impulseChanged()
{
    MotorRowFilter& filter = m_selection->getRowFilter();
    filter.setMinimumImpulse(
        kAllImpulseClasses.at(static_cast<std::size_t>(m_impulseSlider->valueAt(0))));
    filter.setMaximumImpulse(
        kAllImpulseClasses.at(static_cast<std::size_t>(m_impulseSlider->valueAt(1))));
    Q_EMIT filterChanged();
}

void MotorFilterPanel::limitDiameterClicked()
{
    m_limitDiameter = m_limitDiameterCheckBox->isChecked();
    setLimitDiameter();
    Q_EMIT filterChanged();
    updateEnabled();
}

void MotorFilterPanel::diameterChanged()
{
    MotorRowFilter& filter = m_selection->getRowFilter();
    filter.setMinimumDiameter(MotorFilterSliders::minimumDiameter(
        static_cast<std::size_t>(m_diameterSlider->valueAt(0))));
    filter.setMaximumDiameter(MotorFilterSliders::maximumDiameter(
        static_cast<std::size_t>(m_diameterSlider->valueAt(1))));
    Q_EMIT filterChanged();
}

void MotorFilterPanel::limitLengthClicked()
{
    m_limitByLength = m_limitByLengthCheckBox->isChecked();
    setLimitLength();
    Q_EMIT filterChanged();
    updateEnabled();
}

void MotorFilterPanel::minimumLengthChanged()
{
    // The DoubleModel's setter, then minLengthModel's ChangeListener.
    m_selection->getRowFilter().setMinimumLength(m_minLengthModel->value());
    const double minValue = m_minLengthModel->value();
    if (minValue > m_maxLengthModel->value())
    {
        m_maxLengthModel->setValue(minValue);
    }
    m_lengthSlider->setValueAt(0, MathUtil::javaIntCast(kLengthSliderScale * minValue));
    // "If our value is greater than 1000 mm, moving the slider sets it down to 1000. Patch it if
    // so."
    if (m_minLengthModel->value() != minValue)
    {
        m_minLengthModel->setValue(minValue);
    }
    Q_EMIT filterChanged();
}

void MotorFilterPanel::maximumLengthChanged()
{
    // The DoubleModel's setter, then maxLengthModel's ChangeListener.
    m_selection->getRowFilter().setMaximumLength(m_maxLengthModel->value());
    const double maxValue = m_maxLengthModel->value();
    if (maxValue < m_minLengthModel->value())
    {
        m_minLengthModel->setValue(maxValue);
    }
    m_lengthSlider->setValueAt(1, MathUtil::javaIntCast(kLengthSliderScale * maxValue));
    // "If our value is greater than 1000 mm, moving the slider sets it to infinite. Patch it if
    // so."
    if (m_maxLengthModel->value() != maxValue)
    {
        m_maxLengthModel->setValue(maxValue);
    }
    Q_EMIT filterChanged();
}

void MotorFilterPanel::lengthSliderChanged()
{
    const int minLength = m_lengthSlider->valueAt(0);
    m_minLengthModel->setValue(minLength / kLengthSliderScale);

    double maxLengthValue = 0;
    // Java unboxes mountLength here, which is null before the chooser is opened for a mount: a
    // NullPointerException; the slider's own value is taken then.
    if (m_limitByLength && m_mountLength.has_value())
    {
        maxLengthValue = *m_mountLength;
    }
    else
    {
        const int maxLength = m_lengthSlider->valueAt(1);
        maxLengthValue      = maxLength < MotorFilterSliders::kLengthSliderMaximum
                                  ? maxLength / kLengthSliderScale
                                  : std::numeric_limits<double>::infinity();
    }
    m_maxLengthModel->setValue(maxLengthValue);
    Q_EMIT filterChanged();
}

std::vector<const Manufacturer*> MotorFilterPanel::manufacturersAt(
    const std::vector<int>& indices) const
{
    std::vector<const Manufacturer*> manufacturers;
    manufacturers.reserve(indices.size());
    for (const int index : indices)
    {
        manufacturers.push_back(m_manufacturers.at(static_cast<std::size_t>(index)));
    }
    return manufacturers;
}

}  // namespace QtRocket::Gui
