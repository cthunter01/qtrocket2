#include "QtRocket/gui/util/EnumText.h"

#include <string>

#include <QChar>
#include <QCoreApplication>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/TransitionShape.h"

namespace QtRocket::Gui
{

namespace
{

using EnumTextTest = Test::GuiTest;

/// A translator that knows every enum text: "<key>|<English text>", and records what it was
/// asked.
class KeyTranslator : public QTranslator
{
public:
    [[nodiscard]] QString translate(const char* context, const char* sourceText,
                                    const char* disambiguation, int /*n*/) const override
    {
        if (context == nullptr || std::string(context) != kEnumTextContext ||
            disambiguation == nullptr)
        {
            return {};
        }
        return QString::fromUtf8(disambiguation) + QStringLiteral("|") +
               QString::fromUtf8(sourceText);
    }

    [[nodiscard]] bool isEmpty() const override { return false; }
};

/// Installs a translator for as long as it lives.
class InstalledTranslator
{
public:
    InstalledTranslator() { QCoreApplication::installTranslator(&m_translator); }
    ~InstalledTranslator() { QCoreApplication::removeTranslator(&m_translator); }
    InstalledTranslator(const InstalledTranslator&)            = delete;
    InstalledTranslator& operator=(const InstalledTranslator&) = delete;
    InstalledTranslator(InstalledTranslator&&)                 = delete;
    InstalledTranslator& operator=(InstalledTranslator&&)      = delete;

private:
    KeyTranslator m_translator;
};

TEST_F(EnumTextTest, IsTheEnglishTextWithoutATranslator)
{
    EXPECT_EQ(enumText(ComponentKind::BODY_TUBE), QStringLiteral("Body Tube"));
    EXPECT_EQ(enumText(MassComponent::MassComponentType::ALTIMETER), QStringLiteral("Altimeter"));
    EXPECT_EQ(enumText(TransitionShape::OGIVE),
              QString::fromUtf8(displayName(TransitionShape::OGIVE)));
}

TEST_F(EnumTextTest, AsksTheTranslatorsWithTheKeyAsTheDisambiguation)
{
    const InstalledTranslator translator;
    EXPECT_EQ(enumText(ComponentKind::BODY_TUBE), QStringLiteral("BodyTube.BodyTube|Body Tube"));
    EXPECT_EQ(enumText(MassComponent::MassComponentType::ALTIMETER),
              QStringLiteral("MassComponent.Altimeter|Altimeter"));
    EXPECT_EQ(translatedEnumText("Some.key", "Some text"), QStringLiteral("Some.key|Some text"));
}

TEST_F(EnumTextTest, ReadsTheTextsAsUtf8)
{
    EXPECT_EQ(translatedEnumText("Unit.degree", "\xC2\xB0"), QString(QChar(0x00B0)));
}

}  // namespace

}  // namespace QtRocket::Gui
