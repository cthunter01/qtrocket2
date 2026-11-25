#include "QtRocket/gui/dialogs/plot/PlotDialog.h"

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <utility>

#include <QCheckBox>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputEvent>
#include <QObject>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/plot/DataPlot.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/plot/PlotPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// A button of the row showing @p icon alone (JButton(Icon)), named @p name; never the default
/// button.
[[nodiscard]] QPushButton* iconButton(QWidget* parent, Icons::Id icon, const QString& name)
{
    auto* button = makeChild<QPushButton>(parent);
    button->setObjectName(name);
    button->setIcon(Icons::icon(icon));
    button->setAutoDefault(false);
    return button;
}

}  // namespace

PlotDialog::PlotDialog(const GuiServices& services, QString geometryName, QWidget* parent)
  : QDialog(parent), m_services(&services), m_geometryName(std::move(geometryName))
{
    setObjectName(m_geometryName);
    // Dialog.ModalityType.DOCUMENT_MODAL: the window it is shown over waits.
    setWindowModality(Qt::WindowModal);
}

PlotDialog::~PlotDialog() = default;

void PlotDialog::build(DataPlot& plot, bool initialShowEvents, const Extras& extras)
{
    QTROCKET_ASSERT(m_chart == nullptr);
    QTROCKET_ASSERT(plot.parent() == this);
    m_chart = &plot;

    // The chart, then "wrap 20lp": the gap below it is kChartGap, whatever the layout's spacing.
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->setSpacing(0);
    layout->addWidget(m_chart, 1);
    layout->addSpacing(kChartGap);

    // The rows below the chart, the layout's own spacing between them.
    auto* controls = makeChild<QWidget>(this);
    controls->setObjectName(QStringLiteral("controls"));
    auto* rows = makeChild<QVBoxLayout>(controls);
    rows->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(controls);

    //// Description text
    m_help = makeChild<StyledLabel>(controls, helpText(), kSmallTextSize);
    m_help->setObjectName(QStringLiteral("chartHelp"));
    m_help->setTextFormat(Qt::PlainText);
    // QtRocket's text is longer than OpenRocket's one line: it wraps rather than widen the dialog.
    m_help->setWordWrap(true);
    rows->addWidget(m_help);

    // Add X axis warnings
    for (QWidget* const message : extras.messages)
    {
        if (message != nullptr)
        {
            rows->addWidget(message);
        }
    }

    // The row of controls ("split" cells, "gapleft rel" between them).
    auto* bar = makeChild<QWidget>(controls);
    bar->setObjectName(QStringLiteral("buttons"));
    auto* row = makeChild<QHBoxLayout>(bar);
    row->setContentsMargins(0, 0, 0, 0);
    rows->addWidget(bar);

    //// Show data points
    //: PlotDialog.CheckBox.Showdatapoints
    m_checkData = makeChild<QCheckBox>(bar, tr("Show data points"));
    m_checkData->setObjectName(QStringLiteral("checkData"));
    m_checkData->setChecked(m_chart->showsPoints());
    connect(m_checkData, &QCheckBox::clicked, this,
            guarded(
                [this](bool show) {
                    PlotPreferences::setShowPoints(m_services->preferences(), show);
                    m_chart->setShowPoints(show);
                },
                QStringLiteral("Show data points")));
    row->addWidget(m_checkData);

    //// Show events
    //: PlotDialog.CheckBox.ShowEvents
    m_checkEvents = makeChild<QCheckBox>(bar, tr("Show events"));
    m_checkEvents->setObjectName(QStringLiteral("checkEvents"));
    m_checkEvents->setChecked(initialShowEvents);
    // Initialize the show state of events.
    m_chart->setShowEvents(initialShowEvents);
    connect(m_checkEvents, &QCheckBox::clicked, this,
            guarded(
                [this](bool show) {
                    PlotPreferences::setShowEvents(m_services->preferences(), show);
                    m_chart->setShowEvents(show);
                },
                QStringLiteral("Show events")));
    row->addWidget(m_checkEvents);

    // Show errors checkbox, add series selection box
    for (QWidget* const control : extras.controls)
    {
        if (control != nullptr)
        {
            row->addWidget(control);
        }
    }

    //// Zoom in button ("gapleft rel", as the reset and zoom out buttons)
    m_zoomIn = iconButton(bar, Icons::Id::ZOOM_IN, QStringLiteral("zoomIn"));
    connect(m_zoomIn, &QPushButton::clicked, this,
            guarded([this] { m_chart->zoomIn(takeDomainOnly()); }, QStringLiteral("Zoom in")));
    row->addWidget(m_zoomIn);

    //// Reset Zoom button.
    m_resetZoom = iconButton(bar, Icons::Id::ZOOM_RESET, QStringLiteral("resetZoom"));
    connect(m_resetZoom, &QPushButton::clicked, this,
            guarded(
                [this] {
                    // ZOOM_RESET_BOTH whatever the modifiers.
                    static_cast<void>(takeDomainOnly());
                    m_chart->resetZoom();
                },
                QStringLiteral("Reset zoom")));
    row->addWidget(m_resetZoom);

    //// Zoom out button
    m_zoomOut = iconButton(bar, Icons::Id::ZOOM_OUT, QStringLiteral("zoomOut"));
    connect(m_zoomOut, &QPushButton::clicked, this,
            guarded([this] { m_chart->zoomOut(takeDomainOnly()); }, QStringLiteral("Zoom out")));
    row->addWidget(m_zoomOut);
    for (QPushButton* const zoom : {m_zoomIn, m_resetZoom, m_zoomOut})
    {
        zoom->installEventFilter(this);
    }

    //// Print chart button
    //: PlotDialog.btn.exportImage
    m_export = makeChild<QPushButton>(bar, tr("Export Image"));
    m_export->setObjectName(QStringLiteral("exportImage"));
    m_export->setAutoDefault(false);
    connect(m_export, &QPushButton::clicked, this,
            guarded([this] { static_cast<void>(exportImage()); }, QStringLiteral("Export Image")));
    row->addWidget(m_export);

    //// Close button ("gapbefore push, right"), the default button.
    row->addStretch(1);
    //: dlg.but.close
    m_close = makeChild<QPushButton>(bar, tr("Close"));
    m_close->setObjectName(QStringLiteral("close"));
    m_close->setDefault(true);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(m_close);

    // GUIUtil.rememberWindowSize() and rememberWindowPosition(); without a remembered geometry,
    // pack(): the contents' size, the chart's the preferred one.
    m_geometry = makeChild<WindowGeometry>(this, m_services->preferences(), m_geometryName);
    if (!m_geometry->restored())
    {
        // The help text as tall as it wraps at that width: the chart gets its preferred height.
        const QSize hint = sizeHint();
        resize(hint.width(), hasHeightForWidth() ? heightForWidth(hint.width()) : hint.height());
    }
}

QString PlotDialog::helpText()
{
    //: Not in OpenRocket: QtRocket's own text in place of PlotDialog.lbl.Chart, which describes
    //: JFreeChart's gestures; these are rocketplot's (the user's decision U-M4-6)
    return tr(
        "left click drag to zoom area. shift- or ctrl-left click drag to pan. mouse wheel "
        "to zoom, ctrl-mouse wheel x axis only, shift-mouse wheel y axis only. double click "
        "to reset. right click for the menu.");
}

UserPrompts::SaveFileRequest PlotDialog::exportImageRequest(const Preferences& preferences)
{
    UserPrompts::SaveFileRequest request;
    //: FileHelper.PNG_FILTER
    request.nameFilters = QStringList{tr("PNG image (*.png)")};
    if (const std::optional<std::filesystem::path> directory = preferences.getDefaultDirectory())
    {
        request.directory = *directory;
    }
    // A SaveFileChooser.
    request.refuseIllegalNames = true;
    return request;
}

bool PlotDialog::exportImage()
{
    return m_chart->exportImage(exportImageRequest(m_services->preferences()));
}

bool PlotDialog::eventFilter(QObject* watched, QEvent* event)
{
    const bool zoomButton   = watched == m_zoomIn || watched == m_resetZoom || watched == m_zoomOut;
    const QEvent::Type type = event->type();
    // The input that clicks a button: its mouse release, or its key's (Space).
    if (zoomButton && (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease ||
                       type == QEvent::KeyPress || type == QEvent::KeyRelease))
    {
        if (const auto* const input = dynamic_cast<const QInputEvent*>(event); input != nullptr)
        {
            m_clickModifiers = input->modifiers();
        }
    }
    return QDialog::eventFilter(watched, event);
}

bool PlotDialog::takeDomainOnly()
{
    const bool domainOnly = m_clickModifiers.testFlag(Qt::AltModifier);
    m_clickModifiers      = Qt::NoModifier;
    return domainOnly;
}

}  // namespace QtRocket::Gui
