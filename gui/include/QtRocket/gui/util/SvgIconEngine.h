#pragma once

#include <utility>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QIcon>
#include <QIconEngine>
#include <QList>
#include <QPixmap>
#include <QRgb>
#include <QSize>
#include <QString>
#include <QSvgRenderer>

#include "QtRocket/gui/util/ThemeColors.h"

class QPainter;
class QRect;

namespace QtRocket::Gui
{

/// The icon engine of OpenRocket's SVG icons (Icons.loadSvgIcon(): a FlatSVGIcon with a colour
/// filter): the SVG is drawn with its placeholder colours replaced by the colours of
/// ThemeColors keys, read from the application's palette each time the icon is painted, so that
/// an icon follows a change between the light and the dark palette without being made again.
/// It renders with QSvgRenderer at the size asked for times the device pixel ratio, so it is
/// sharp at any size and on any screen.
///
/// The placeholders are RGB values (OpenRocket's Map<Integer, String>, alpha ignored): a colour
/// of the SVG with that RGB gets the RGB of its key's colour and keeps its own opacity. The SVG
/// may write a colour as #rgb, #rrggbb, rgb(r,g,b) or a name (black, white, ...), in a
/// fill, stroke, stop-color, flood-color, lighting-color or color attribute or in a style
/// declaration; currentColor is black (the initial color property). A shape without a fill is
/// filled black (SVG's initial fill value), which a black placeholder recolours as well.
///
/// The other icon modes (Disabled, Active, Selected) are the Normal pixmap as the application's
/// style generates them (QStyle::generatedIconPixmap()), as Qt's own SVG engine does.
class SvgIconEngine final : public QIconEngine
{
public:
    /// A placeholder RGB (0xRRGGBB) and the key of the colour it stands for.
    using Placeholder = std::pair<QRgb, ThemeColors::Key>;

    /// The engine of the SVG resource @p resourcePath (":/icons/actions/undo.svg"). A missing or
    /// broken resource gives an engine that isNull() and draws nothing.
    SvgIconEngine(const QString& resourcePath, std::vector<Placeholder> placeholders);
    ~SvgIconEngine() override = default;

    SvgIconEngine(const SvgIconEngine& other);
    SvgIconEngine& operator=(const SvgIconEngine&) = delete;
    SvgIconEngine(SvgIconEngine&&)                 = delete;
    SvgIconEngine& operator=(SvgIconEngine&&)      = delete;

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override;
    QSize   actualSize(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
    [[nodiscard]] QIconEngine* clone() const override;
    [[nodiscard]] QString      key() const override;
    bool                       isNull() override;

    /// The SVG resource.
    [[nodiscard]] const QString& resourcePath() const noexcept { return m_resourcePath; }

    /// @p svg with every colour whose RGB is a key of @p colors replaced by that colour's RGB
    /// (see the class comment), as the engine draws it; the input unchanged when it is not
    /// well-formed XML.
    [[nodiscard]] static QByteArray recolored(const QByteArray&                           svg,
                                              const std::vector<std::pair<QRgb, QColor>>& colors);

private:
    /// Resolves the placeholders' colours for the application's palette; when they changed,
    /// loads the recoloured SVG into the renderer and forgets the pixmaps drawn so far.
    void update();
    /// The Normal pixmap of @p pixelSize device pixels.
    [[nodiscard]] QPixmap render(const QSize& pixelSize);

    QString                              m_resourcePath;
    QByteArray                           m_source;
    std::vector<Placeholder>             m_placeholders;
    std::vector<std::pair<QRgb, QColor>> m_colors;
    bool                                 m_loaded = false;
    QSvgRenderer                         m_renderer;
    /// The Normal pixmaps drawn for the current colours, by device pixel size.
    std::vector<std::pair<QSize, QPixmap>> m_pixmaps;
};

}  // namespace QtRocket::Gui
