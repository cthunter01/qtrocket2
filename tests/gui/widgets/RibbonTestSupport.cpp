#include "RibbonTestSupport.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QModelIndex>
#include <QTest>
#include <Qt>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"

namespace QtRocket::Gui::Test
{

ChangeEventTypes::ChangeEventTypes(Rocket& rocket)
  : m_rocket(&rocket),
    m_connection(rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
        m_types.push_back(event.getType());
        m_allFromRocket = m_allFromRocket && event.getSource() == m_rocket;
    }))
{
}

void ChangeEventTypes::clear() noexcept
{
    m_types.clear();
    m_allFromRocket = true;
}

void pickComboItem(QComboBox const& combo, int index)
{
    QAbstractItemView* view = combo.view();
    ASSERT_NE(view, nullptr);
    const QModelIndex item =
        view->model()->index(index, combo.modelColumn(), combo.rootModelIndex());
    ASSERT_TRUE(item.isValid()) << index;
    view->setCurrentIndex(item);
    QTest::keyClick(view, Qt::Key_Return);
}

}  // namespace QtRocket::Gui::Test
