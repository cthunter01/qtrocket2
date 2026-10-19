#include "WidgetTestSupport.h"

#include <initializer_list>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui::Test
{

QLineEdit* editorOf(QAbstractSpinBox& spinBox)
{
    return spinBox.findChild<QLineEdit*>(QString(), Qt::FindDirectChildrenOnly);
}

bool showActive(QWidget& window)
{
    window.show();
    window.activateWindow();
    return QTest::qWaitForWindowActive(&window);
}

bool showActiveWith(QWidget& window, std::initializer_list<QWidget*> widgets)
{
    auto* const layout = makeChild<QVBoxLayout>(&window);
    for (QWidget* const widget : widgets)
    {
        layout->addWidget(widget);
    }
    if (auto* const dialog = qobject_cast<QDialog*>(&window))
    {
        auto* const buttons =
            makeChild<QDialogButtonBox>(dialog, QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
        layout->addWidget(buttons);
    }
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks): its Qt parent, window, owns layout
    return showActive(window);
}

void typeInto(QAbstractSpinBox& spinBox, const QString& text)
{
    spinBox.setFocus();
    QCoreApplication::processEvents();  // the select-all a spin box defers when it gets the focus
    spinBox.selectAll();
    QTest::keyClicks(&spinBox, text);
}

bool turnWheel(QWidget& widget, int delta)
{
    const QPointF position(widget.width() / 2.0, widget.height() / 2.0);
    QWheelEvent   event(position, widget.mapToGlobal(position), QPoint(), QPoint(0, delta),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
    return event.isAccepted();
}

}  // namespace QtRocket::Gui::Test
