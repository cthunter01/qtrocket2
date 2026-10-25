#include "QtRocket/gui/dialogs/ThrustCurveMotorPlotDialog.h"

#include <memory>
#include <span>

#include <QDialog>
#include <QDialogButtonBox>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/plot/ThrustCurvePlot.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

ThrustCurveMotorPlotDialog::ThrustCurveMotorPlotDialog(
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected,
    const GuiServices& services, QWidget* parent)
  : QDialog(parent), m_chart(makeChild<ThrustCurvePlot>(this, services))
{
    setObjectName(QStringLiteral("plotDialog"));
    //: Not translated in OpenRocket (ThrustCurveMotorPlotDialog.java): the window's title
    setWindowTitle(tr("Motor thrust curves"));
    // Dialog.ModalityType.APPLICATION_MODAL.
    setWindowModality(Qt::ApplicationModal);

    auto* layout = makeChild<QVBoxLayout>(this);
    // "wrap para": the paragraph gap is the only gap of the column.
    layout->setSpacing(kParagraphGap);
    // "width 600:600:, height 400:400:, grow": at least 600 x 400, that large when the dialog is
    // made its contents' size (pack()), and growing with the dialog. The policy leaves the plot's
    // own size hint out of the layout's, so that the minimum is the size the dialog opens with.
    m_chart->setMinimumSize(kChartWidth, kChartHeight);
    m_chart->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_chart->setCurves(motors, selected);
    layout->addWidget(m_chart, 1);

    // "right, tag close": QDialogButtonBox puts a lone rejecting button at the right.
    auto* buttons = makeChild<QDialogButtonBox>(this);
    //: dlg.but.close
    m_close = buttons->addButton(tr("Close"), QDialogButtonBox::RejectRole);
    m_close->setObjectName(QStringLiteral("close"));
    // GUIUtil.setDisposableDialogOptions(this, null): no default button.
    m_close->setAutoDefault(false);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // pack().
    adjustSize();
}

ThrustCurveMotorPlotDialog& ThrustCurveMotorPlotDialog::showPlot(
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors, int selected,
    const GuiServices& services, QWidget& parent)
{
    auto* dialog = makeChild<ThrustCurveMotorPlotDialog>(&parent, motors, selected, services);
    // DISPOSE_ON_CLOSE: Close, Esc and the window's close button all finish the dialog.
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->show();
    return *dialog;
}

}  // namespace QtRocket::Gui
