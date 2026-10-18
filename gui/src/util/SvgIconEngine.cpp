#include "QtRocket/gui/util/SvgIconEngine.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QIODevice>
#include <QIcon>
#include <QIconEngine>
#include <QImage>
#include <QLatin1Char>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRgb>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QStyle>
#include <QStyleOption>
#include <QXmlStreamAttribute>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

namespace
{

/// The SVG properties whose value is a colour.
constexpr std::array<QStringView, 6> kColorProperties{
    QStringView(u"fill"),        QStringView(u"stroke"),         QStringView(u"stop-color"),
    QStringView(u"flood-color"), QStringView(u"lighting-color"), QStringView(u"color"),
};

constexpr QRgb kRgbMask = 0x00ffffff;

/// The more pixmaps an engine keeps, the more sizes it is painted at; a few are enough.
constexpr std::size_t kMaxPixmaps = 8;

[[nodiscard]] bool isColorProperty(QStringView name)
{
    return std::ranges::any_of(kColorProperties,
                               [name](QStringView property) { return name == property; });
}

/// The RGB of the channel list of "rgb(r,g,b)" (integers 0-255), or nullopt.
[[nodiscard]] std::optional<QRgb> parseRgbFunction(QStringView value)
{
    const QStringView inner = value.sliced(4, value.size() - 5);
    const auto        parts = inner.split(QLatin1Char(','));
    if (parts.size() != 3)
    {
        return std::nullopt;
    }
    std::array<int, 3> channels{};
    for (std::size_t index = 0; index < channels.size(); ++index)
    {
        bool      ok      = false;
        const int channel = parts.at(static_cast<qsizetype>(index)).trimmed().toInt(&ok);
        if (!ok || channel < 0 || channel > 255)
        {
            return std::nullopt;
        }
        channels.at(index) = channel;
    }
    return qRgb(channels[0], channels[1], channels[2]) & kRgbMask;
}

/// The RGB of the SVG colour @p value, or nullopt for none, a paint server (url(...)), inherit
/// and the like.
[[nodiscard]] std::optional<QRgb> parseSvgColor(QStringView value)
{
    const QStringView text = value.trimmed();
    if (text.compare(QStringView(u"currentColor"), Qt::CaseInsensitive) == 0)
    {
        // The initial value of the color property: black.
        return QRgb{0};
    }
    if (text.startsWith(QStringView(u"rgb("), Qt::CaseInsensitive) &&
        text.endsWith(QLatin1Char(')')))
    {
        return parseRgbFunction(text);
    }
    if (text.compare(QStringView(u"none"), Qt::CaseInsensitive) == 0 ||
        text.compare(QStringView(u"transparent"), Qt::CaseInsensitive) == 0)
    {
        return std::nullopt;
    }
    const QColor color = QColor::fromString(text);
    if (!color.isValid())
    {
        return std::nullopt;
    }
    return color.rgb() & kRgbMask;
}

/// The replacement of the colour @p value, or nullopt to keep it.
[[nodiscard]] std::optional<QString> replacement(QStringView                                 value,
                                                 const std::vector<std::pair<QRgb, QColor>>& colors)
{
    const std::optional<QRgb> rgb = parseSvgColor(value);
    if (!rgb)
    {
        return std::nullopt;
    }
    const auto found = std::ranges::find_if(colors, [&rgb](const std::pair<QRgb, QColor>& color) {
        return (color.first & kRgbMask) == *rgb;
    });
    if (found == colors.end())
    {
        return std::nullopt;
    }
    return found->second.name(QColor::HexRgb);
}

/// @p style ("fill:none;stroke:#000;stroke-width:1.5px;") with its colours replaced.
[[nodiscard]] QString recolorStyle(const QString&                              style,
                                   const std::vector<std::pair<QRgb, QColor>>& colors)
{
    QStringList declarations = style.split(QLatin1Char(';'));
    for (QString& declaration : declarations)
    {
        const qsizetype colon = declaration.indexOf(QLatin1Char(':'));
        if (colon < 0 || !isColorProperty(QStringView(declaration).first(colon).trimmed()))
        {
            continue;
        }
        if (const std::optional<QString> color =
                replacement(QStringView(declaration).sliced(colon + 1), colors))
        {
            declaration = declaration.first(colon + 1) + *color;
        }
    }
    return declarations.join(QLatin1Char(';'));
}

/// Whether the declarations of @p style set the fill.
[[nodiscard]] bool styleSetsFill(const QString& style)
{
    const QStringList declarations = style.split(QLatin1Char(';'));
    return std::ranges::any_of(declarations, [](const QString& declaration) {
        const qsizetype colon = declaration.indexOf(QLatin1Char(':'));
        return colon >= 0 &&
               QStringView(declaration).first(colon).trimmed() == QStringView(u"fill");
    });
}

/// Writes the start element the reader is at, its attributes' colours replaced; on the root
/// element a black fill when the SVG leaves the fill at its initial black and black is replaced.
void writeStartElement(const QXmlStreamReader& reader, QXmlStreamWriter& writer, bool root,
                       const std::vector<std::pair<QRgb, QColor>>& colors)
{
    writer.writeStartElement(reader.qualifiedName().toString());
    bool fillSet = false;
    for (const QXmlStreamAttribute& attribute : reader.attributes())
    {
        const QString name  = attribute.qualifiedName().toString();
        QString       value = attribute.value().toString();
        if (name == QStringView(u"style"))
        {
            fillSet = fillSet || styleSetsFill(value);
            value   = recolorStyle(value, colors);
        }
        else if (isColorProperty(name))
        {
            fillSet = fillSet || name == QStringView(u"fill");
            if (const std::optional<QString> color = replacement(value, colors))
            {
                value = *color;
            }
        }
        writer.writeAttribute(name, value);
    }
    if (root && !fillSet)
    {
        if (const std::optional<QString> color = replacement(QStringView(u"black"), colors))
        {
            writer.writeAttribute(QStringLiteral("fill"), *color);
        }
    }
}

/// The application's style's @p mode version of @p pixmap (QStyle::generatedIconPixmap()), or
/// @p pixmap itself in a program without QApplication.
[[nodiscard]] QPixmap styledPixmap(const QPixmap& pixmap, QIcon::Mode mode)
{
    if (mode == QIcon::Normal || pixmap.isNull() ||
        qobject_cast<QApplication*>(QCoreApplication::instance()) == nullptr)
    {
        return pixmap;
    }
    QStyleOption option;
    option.palette       = QGuiApplication::palette();
    const QPixmap styled = QApplication::style()->generatedIconPixmap(mode, pixmap, &option);
    return styled.isNull() ? pixmap : styled;
}

[[nodiscard]] QByteArray readResource(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return {};
    }
    return file.readAll();
}

}  // namespace

SvgIconEngine::SvgIconEngine(const QString& resourcePath, std::vector<Placeholder> placeholders)
  : m_resourcePath(resourcePath),
    m_source(readResource(resourcePath)),
    m_placeholders(std::move(placeholders))
{
}

SvgIconEngine::SvgIconEngine(const SvgIconEngine& other)
  : QIconEngine(other),
    m_resourcePath(other.m_resourcePath),
    m_source(other.m_source),
    m_placeholders(other.m_placeholders)
{
}

void SvgIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode,
                          QIcon::State state)
{
    const qreal   scale  = painter->device()->devicePixelRatioF();
    const QPixmap pixmap = scaledPixmap(rect.size(), mode, state, scale);
    painter->drawPixmap(rect, pixmap);
}

QPixmap SvgIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    return scaledPixmap(size, mode, state, 1.0);
}

QPixmap SvgIconEngine::scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State /*state*/,
                                    qreal scale)
{
    update();
    const QSize pixelSize = (QSizeF(actualSize(size, mode, QIcon::Off)) * scale).toSize();
    if (pixelSize.isEmpty())
    {
        return {};
    }
    QPixmap result = styledPixmap(render(pixelSize), mode);
    result.setDevicePixelRatio(scale);
    return result;
}

QSize SvgIconEngine::actualSize(const QSize& size, QIcon::Mode /*mode*/, QIcon::State /*state*/)
{
    update();
    const QSize natural = m_renderer.defaultSize();
    if (natural.isEmpty() || size.isEmpty())
    {
        return size;
    }
    return natural.scaled(size, Qt::KeepAspectRatio);
}

QIconEngine* SvgIconEngine::clone() const
{
    return makeQtOwned<SvgIconEngine>(*this);
}

QString SvgIconEngine::key() const
{
    return QStringLiteral("QtRocket::Gui::SvgIconEngine");
}

bool SvgIconEngine::isNull()
{
    update();
    return !m_renderer.isValid();
}

void SvgIconEngine::update()
{
    const QPalette                       palette = QGuiApplication::palette();
    std::vector<std::pair<QRgb, QColor>> colors;
    colors.reserve(m_placeholders.size());
    for (const auto& [rgb, colorKey] : m_placeholders)
    {
        colors.emplace_back(rgb, ThemeColors::color(colorKey, palette));
    }
    if (m_loaded && colors == m_colors)
    {
        return;
    }
    m_colors = std::move(colors);
    m_pixmaps.clear();
    m_loaded = true;
    if (!m_source.isEmpty())
    {
        static_cast<void>(m_renderer.load(recolored(m_source, m_colors)));
    }
}

QPixmap SvgIconEngine::render(const QSize& pixelSize)
{
    const auto cached = std::ranges::find_if(
        m_pixmaps,
        [&pixelSize](const std::pair<QSize, QPixmap>& entry) { return entry.first == pixelSize; });
    if (cached != m_pixmaps.end())
    {
        return cached->second;
    }
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (m_renderer.isValid())
    {
        QPainter painter(&image);
        m_renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(pixelSize)));
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    if (m_pixmaps.size() >= kMaxPixmaps)
    {
        m_pixmaps.clear();
    }
    m_pixmaps.emplace_back(pixelSize, pixmap);
    return pixmap;
}

QByteArray SvgIconEngine::recolored(const QByteArray&                           svg,
                                    const std::vector<std::pair<QRgb, QColor>>& colors)
{
    QXmlStreamReader reader(svg);
    reader.setNamespaceProcessing(false);
    QByteArray       result;
    QXmlStreamWriter writer(&result);
    bool             root = true;
    while (!reader.atEnd())
    {
        switch (reader.readNext())
        {
            case QXmlStreamReader::StartElement:
                writeStartElement(reader, writer, root, colors);
                root = false;
                break;
            case QXmlStreamReader::EndElement:
                writer.writeEndElement();
                break;
            case QXmlStreamReader::Characters:
                if (reader.isCDATA())
                {
                    writer.writeCDATA(reader.text().toString());
                }
                else
                {
                    writer.writeCharacters(reader.text().toString());
                }
                break;
            default:
                // The declaration, the DOCTYPE, comments and processing instructions are left
                // out: the renderer needs none of them.
                break;
        }
    }
    if (reader.hasError())
    {
        return svg;
    }
    return result;
}

}  // namespace QtRocket::Gui
