#include "QtRocket/gui/figure/ScaleSelector.h"

#include <cmath>
#include <cstddef>
#include <optional>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QObject>
#include <QScopedValueRollback>
#include <QString>
#include <QStyle>
#include <QToolButton>
#include <QWidget>

#include "QtRocket/gui/figure/ZoomModel.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/DecimalFormat.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

/// The combo box's least width ("wmin 120lp").
constexpr int kComboMinimumWidth = 120;
/// The gap between the zoom out button and the combo box where the style names none.
constexpr int kDefaultGap = 6;

}  // namespace

ScaleSelector::ScaleSelector(ZoomModel* model, QWidget* parent)
  : QWidget(parent),
    m_model(model),
    m_zoomOut(makeChild<QToolButton>(this)),
    m_combo(makeChild<QComboBox>(this)),
    m_zoomIn(makeChild<QToolButton>(this)),
    m_zoomFit(makeChild<QToolButton>(this))
{
    QTROCKET_ASSERT(model != nullptr);

    // Zoom out button
    m_zoomOut->setObjectName(QStringLiteral("zoomOutButton"));
    m_zoomOut->setIcon(Icons::icon(Icons::Id::ZOOM_OUT));
    //: ScaleSelector.btn.ZoomOut.ttip
    m_zoomOut->setToolTip(tr("Zoom out"));
    connect(m_zoomOut, &QToolButton::clicked, this, [this] { zoomOut(); });

    // Zoom level selector. Its own line edit, so that the action runs before QComboBox's own
    // reaction to the edit (which may activate the matching level, see applyText()).
    m_combo->setObjectName(QStringLiteral("scaleSelectorCombo"));
    auto* edit = makeChild<QLineEdit>(m_combo);
    connect(edit, &QLineEdit::editingFinished, this, [this, edit] {
        // QLineEdit reports one Return twice (Qt 6.11), and a loss of focus too: an entry
        // is applied once, when the user changed the text since.
        if (!edit->isModified())
        {
            return;
        }
        edit->setModified(false);
        m_inEditingFinished = true;
        applyText(edit->text());
    });
    m_combo->setLineEdit(edit);
    connect(edit, &QLineEdit::editingFinished, this, [this] { m_inEditingFinished = false; });
    m_combo->setInsertPolicy(QComboBox::NoInsert);
    m_combo->setCompleter(nullptr);
    for (const double level : kScaleLevels)
    {
        m_combo->addItem(formatScaleText(level, level == 1.0));
    }
    m_combo->setMinimumWidth(kComboMinimumWidth);
    m_combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_combo, &QComboBox::activated, this, [this](int index) {
        // A text entered was applied already (editingFinished above).
        if (!m_inEditingFinished)
        {
            applyText(m_combo->itemText(index));
        }
    });
    setZoomText();
    connect(model, &ZoomModel::changed, this, [this] { updateZoomText(); });

    // Zoom in button
    m_zoomIn->setObjectName(QStringLiteral("zoomInButton"));
    m_zoomIn->setIcon(Icons::icon(Icons::Id::ZOOM_IN));
    //: ScaleSelector.btn.ZoomIn.ttip
    m_zoomIn->setToolTip(tr("Zoom in"));
    connect(m_zoomIn, &QToolButton::clicked, this, [this] { zoomIn(); });

    // Zoom fit button
    m_zoomFit->setObjectName(QStringLiteral("zoomFitButton"));
    m_zoomFit->setIcon(Icons::icon(Icons::Id::ZOOM_RESET));
    //: ScaleSelector.btn.ZoomFit.ttip
    m_zoomFit->setToolTip(tr("Zoom to fit"));
    connect(m_zoomFit, &QToolButton::clicked, this, [this] { zoomFit(); });

    // getAsPanel(): "insets 0", "[][]0[]0[]".
    auto* layout = makeChild<QHBoxLayout>(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_zoomOut);
    const int gap = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, this);
    layout->addSpacing(gap > 0 ? gap : kDefaultGap);
    layout->addWidget(m_combo, 1);
    layout->addWidget(m_zoomIn);
    layout->addWidget(m_zoomFit);
}

void ScaleSelector::updateZoomText()
{
    setZoomText();
}

QString ScaleSelector::formatScaleText(double scale, bool fit)
{
    // DecimalFormat "0.#%": the value times 100 with "0.#" and a '%' suffix; NaN without it.
    static const DecimalFormat kPercentFormat("0.#");
    QString text = std::isnan(scale)
                       ? QStringLiteral("NaN")
                       : QtStrings::fromUtf8(kPercentFormat.format(scale * 100.0)) + u'%';
    if (fit)
    {
        text += fitSuffix();
    }
    return text;
}

QString ScaleSelector::fitSuffix()
{
    // The file has " (fit)": Java's properties parser drops the spaces before a value.
    //: ScaleSelector.lbl.ZoomFitSuffix
    return tr("(fit)");
}

std::optional<ScaleSelector::ScaleRequest> ScaleSelector::parseScaleText(const QString& text)
{
    const QString suffix       = fitSuffix();
    const bool    fitSelection = text.endsWith(suffix);
    QString       number       = text;
    number.remove(suffix).remove(u'%');

    const std::optional<double> parsed = Strings::javaParseDouble(QtStrings::toUtf8(number));
    if (!parsed)
    {
        return std::nullopt;
    }
    // Java: n <= 0.005 becomes 0.005; NaN stays NaN.
    const double scale = MathUtil::javaMax(*parsed / 100, kMinimumScale);
    return ScaleRequest{.scale = scale, .fit = fitSelection && std::abs(scale - 1.0) < 0.0001};
}

double ScaleSelector::nextLargerScale(double currentScale) noexcept
{
    std::size_t index = 0;
    for (; index < kScaleLevels.size() - 1; ++index)
    {
        if (currentScale > kScaleLevels.at(index) + 0.05 &&
            currentScale < kScaleLevels.at(index + 1) + 0.05)
        {
            return kScaleLevels.at(index);
        }
    }
    if (currentScale > kScaleLevels.at(kScaleLevels.size() / 2))
    {
        // The scale is large: drop to the next lower full 100 %.
        const double nextScale = std::ceil(currentScale - 1.05);
        return MathUtil::javaMax(nextScale, kScaleLevels.at(index));
    }
    // The scale is small.
    return currentScale / 1.5;
}

double ScaleSelector::nextSmallerScale(double currentScale) noexcept
{
    for (std::size_t index = 0; index < kScaleLevels.size() - 1; ++index)
    {
        if (currentScale > kScaleLevels.at(index) - 0.05 &&
            currentScale < kScaleLevels.at(index + 1) - 0.05)
        {
            return kScaleLevels.at(index + 1);
        }
    }
    if (currentScale > kScaleLevels.at(kScaleLevels.size() / 2))
    {
        // The scale is large: go to the next full 100 %.
        return std::floor(currentScale + 1.05);
    }
    return currentScale * 1.5;
}

void ScaleSelector::applyText(const QString& text)
{
    ZoomModel* const model = m_model.data();
    if (m_updatingText || model == nullptr)
    {
        return;
    }
    const std::optional<ScaleRequest> request = parseScaleText(text);
    if (!request)
    {
        setZoomText();
        return;
    }
    if (request->fit)
    {
        model->setFit();
    }
    else
    {
        model->setScale(request->scale);
    }
    // A model may apply the request later: show what was asked for until it reports its scale
    // (ScaleSelectorTest).
    setZoomText(request->scale, request->fit);
}

void ScaleSelector::setZoomText()
{
    ZoomModel* const model = m_model.data();
    if (model != nullptr)
    {
        const double scale = model->scale();
        setZoomText(scale, model->isFit());
    }
}

void ScaleSelector::setZoomText(double scale, bool fit)
{
    const QString text = formatScaleText(scale, fit);
    if (text == m_combo->currentText())
    {
        return;
    }
    const QScopedValueRollback<bool> updating(m_updatingText, true);
    const int                        index = m_combo->findText(text);
    if (index >= 0)
    {
        m_combo->setCurrentIndex(index);
    }
    else
    {
        m_combo->setEditText(text);
    }
}

void ScaleSelector::zoomOut()
{
    ZoomModel* const model = m_model.data();
    if (model == nullptr)
    {
        return;
    }
    const double newScale = nextLargerScale(model->scale());
    model->setScale(newScale);
    setZoomText(newScale, false);
}

void ScaleSelector::zoomIn()
{
    ZoomModel* const model = m_model.data();
    if (model == nullptr)
    {
        return;
    }
    const double newScale = nextSmallerScale(model->scale());
    model->setScale(newScale);
    setZoomText(newScale, false);
}

void ScaleSelector::zoomFit()
{
    ZoomModel* const model = m_model.data();
    if (model == nullptr)
    {
        return;
    }
    model->setFit();
    setZoomText(1.0, true);
}

}  // namespace QtRocket::Gui
