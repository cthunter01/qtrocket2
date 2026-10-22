#include "BodyPanelTestSupport.h"

#include <initializer_list>
#include <string>

#include <QString>
#include <QWidget>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"

namespace QtRocket::Gui::Test
{

std::string missingNumberRows(const QWidget& root, std::initializer_list<const char*> properties)
{
    std::string missing;
    for (const char* property : properties)
    {
        const QString name = QString::fromLatin1(property);
        if (childNamed<UnitSpinBox>(root, name) == nullptr ||
            childNamed<UnitSelector>(root, name) == nullptr ||
            childNamed<ValueSlider>(root, name) == nullptr)
        {
            missing += std::string(property) + '\n';
        }
    }
    return missing;
}

}  // namespace QtRocket::Gui::Test
