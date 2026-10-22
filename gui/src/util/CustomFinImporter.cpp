#include "QtRocket/gui/util/CustomFinImporter.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iterator>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QChar>
#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QList>
#include <QRgb>
#include <QRgba64>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// The largest level of an 8-bit and of a 16-bit greyscale image.
constexpr double kMaxLevel8  = 255.0;
constexpr double kMaxLevel16 = 65535.0;

/// The sRGB transfer function of IEC 61966-2-1: the linear value @p linear (0 to 1) as an sRGB
/// value of 8 bits, rounded half up (Java's colour model converts the levels of a linear greyscale
/// image so; the result is the same for every level of 8 and 16 bits, see the tests).
[[nodiscard]] int srgb8(double linear)
{
    constexpr double kLinearLimit = 0.0031308;
    constexpr double kLinearSlope = 12.92;
    constexpr double kScale       = 1.055;
    constexpr double kOffset      = 0.055;
    constexpr double kExponent    = 1.0 / 2.4;
    const double value = linear <= kLinearLimit ? linear * kLinearSlope
                                                : (kScale * std::pow(linear, kExponent)) - kOffset;
    return static_cast<int>(std::floor((value * kMaxLevel8) + 0.5));
}

/// srgb8() of the 256 levels of an 8-bit greyscale image.
[[nodiscard]] const std::array<std::uint8_t, 256>& srgbOfGrey8()
{
    static const std::array<std::uint8_t, 256> kTable = [] {
        std::array<std::uint8_t, 256> table{};
        for (std::size_t level = 0; level < table.size(); ++level)
        {
            table.at(level) =
                static_cast<std::uint8_t>(srgb8(static_cast<double>(level) / kMaxLevel8));
        }
        return table;
    }();
    return kTable;
}

/// Which way the walk faces (Java's FacingDirections).
enum class Facing : std::uint8_t
{
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

/// The black-and-white image validateImage() makes: whether each pixel is fin.
class FinMask
{
public:
    explicit FinMask(const QImage& image)
      : m_width(image.width()),
        m_height(image.height()),
        m_fin(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height))
    {
        for (int y = 0; y < m_height; ++y)
        {
            for (int x = 0; x < m_width; ++x)
            {
                m_fin[index(x, y)] =
                    CustomFinImporter::isBlack(CustomFinImporter::javaRgb(image, x, y));
            }
        }
    }

    [[nodiscard]] int height() const noexcept { return m_height; }

    /// Whether the pixel (@p x, @p y) is fin: false outside the image (pixelIsFin()).
    [[nodiscard]] bool isFin(int x, int y) const
    {
        return x >= 0 && x < m_width && y >= 0 && y < m_height && m_fin[index(x, y)];
    }

    /// The first x of a fin pixel on the bottom row, from the left (validateImage()'s startX);
    /// nullopt when there is none.
    [[nodiscard]] std::optional<int> startX() const
    {
        for (int x = 0; x < m_width; ++x)
        {
            if (isFin(x, m_height - 1))
            {
                return x;
            }
        }
        return std::nullopt;
    }

    /// Whether the walk may move one pixel to the right of @p x (moveForward()).
    [[nodiscard]] bool hasRightOf(int x) const noexcept { return x < m_width - 1; }

private:
    [[nodiscard]] std::size_t index(int x, int y) const noexcept
    {
        return (static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width)) +
               static_cast<std::size_t>(x);
    }

    int               m_width;
    int               m_height;
    std::vector<bool> m_fin;
};

/// Where the walk is and which way it faces, and whether it has left the bottom row.
struct WalkState
{
    int    x         = 0;
    int    y         = 0;
    Facing facing    = Facing::UP;
    bool   offBottom = false;

    friend bool operator==(const WalkState&, const WalkState&) = default;
};

/// The pixel to the left of the walk, ahead of it and to its right (checkLeftIsFin(),
/// checkForwardIsFin(), checkRightIsFin()).
[[nodiscard]] bool leftIsFin(const FinMask& mask, const WalkState& walk)
{
    switch (walk.facing)
    {
        case Facing::DOWN:
            return mask.isFin(walk.x + 1, walk.y);
        case Facing::UP:
            return mask.isFin(walk.x - 1, walk.y);
        case Facing::LEFT:
            return mask.isFin(walk.x, walk.y + 1);
        case Facing::RIGHT:
            return mask.isFin(walk.x, walk.y - 1);
    }
    return false;
}

[[nodiscard]] bool rightIsFin(const FinMask& mask, const WalkState& walk)
{
    switch (walk.facing)
    {
        case Facing::DOWN:
            return mask.isFin(walk.x - 1, walk.y);
        case Facing::UP:
            return mask.isFin(walk.x + 1, walk.y);
        case Facing::LEFT:
            return mask.isFin(walk.x, walk.y - 1);
        case Facing::RIGHT:
            return mask.isFin(walk.x, walk.y + 1);
    }
    return false;
}

[[nodiscard]] bool forwardIsFin(const FinMask& mask, const WalkState& walk)
{
    switch (walk.facing)
    {
        case Facing::DOWN:
            return mask.isFin(walk.x, walk.y + 1);
        case Facing::UP:
            return mask.isFin(walk.x, walk.y - 1);
        case Facing::LEFT:
            return mask.isFin(walk.x - 1, walk.y);
        case Facing::RIGHT:
            return mask.isFin(walk.x + 1, walk.y);
    }
    return false;
}

[[nodiscard]] Facing rotatedLeft(Facing facing)
{
    switch (facing)
    {
        case Facing::UP:
            return Facing::LEFT;
        case Facing::RIGHT:
            return Facing::UP;
        case Facing::DOWN:
            return Facing::RIGHT;
        case Facing::LEFT:
            return Facing::DOWN;
    }
    return facing;
}

[[nodiscard]] Facing rotatedRight(Facing facing)
{
    switch (facing)
    {
        case Facing::UP:
            return Facing::RIGHT;
        case Facing::RIGHT:
            return Facing::DOWN;
        case Facing::DOWN:
            return Facing::LEFT;
        case Facing::LEFT:
            return Facing::UP;
    }
    return facing;
}

[[nodiscard]] Facing turnedAround(Facing facing)
{
    switch (facing)
    {
        case Facing::UP:
            return Facing::DOWN;
        case Facing::DOWN:
            return Facing::UP;
        case Facing::RIGHT:
            return Facing::LEFT;
        case Facing::LEFT:
            return Facing::RIGHT;
    }
    return facing;
}

/// One pixel ahead, never off the image (moveForward()).
void moveForward(const FinMask& mask, WalkState& walk)
{
    switch (walk.facing)
    {
        case Facing::UP:
            if (walk.y > 0)
            {
                --walk.y;
            }
            break;
        case Facing::RIGHT:
            if (mask.hasRightOf(walk.x))
            {
                ++walk.x;
            }
            break;
        case Facing::DOWN:
            if (walk.y < mask.height() - 1)
            {
                ++walk.y;
            }
            break;
        case Facing::LEFT:
            if (walk.x > 0)
            {
                --walk.x;
            }
            break;
    }
}

/// One step of loadFin()'s walk: the turn, the move, and whether it has left the bottom row.
void step(const FinMask& mask, WalkState& walk)
{
    if (leftIsFin(mask, walk))
    {
        walk.facing = rotatedLeft(walk.facing);
    }
    else if (forwardIsFin(mask, walk))
    {
        // Do nothing
    }
    else if (rightIsFin(mask, walk))
    {
        walk.facing = rotatedRight(walk.facing);
    }
    else
    {
        walk.facing = turnedAround(walk.facing);
    }
    moveForward(mask, walk);
    if (walk.y < mask.height() - 1)
    {
        walk.offBottom = true;
    }
}

/// The closest point to @p p on the segment from @p a to @p b (closestPointOnSegment()).
[[nodiscard]] Coordinate closestPointOnSegment(const Coordinate& a, const Coordinate& b,
                                               const Coordinate& p)
{
    const Coordinate d     = b.sub(a);
    const double     numer = p.sub(a).dot(d);
    if (numer <= 0.0)
    {
        return a;
    }
    const double denom = d.dot(d);
    if (numer >= denom)
    {
        return b;
    }
    return a.add(d.multiply(numer / denom));
}

/// The distance of @p testPoint from the segment from @p startPoint to @p endPoint
/// (pointDistanceFromLine()).
[[nodiscard]] double pointDistanceFromLine(const Coordinate& startPoint, const Coordinate& endPoint,
                                           const Coordinate& testPoint)
{
    const Coordinate pt = closestPointOnSegment(startPoint, endPoint, testPoint);
    return testPoint.sub(pt).length();
}

/// Whether every point after @p start and before @p end lies within the tolerance of the segment
/// between them (optimizePoints()' inner loop).
[[nodiscard]] bool isStraight(const std::vector<Coordinate>& points, std::size_t start,
                              std::size_t end)
{
    for (std::size_t test = start + 1; test != end; ++test)
    {
        if (pointDistanceFromLine(points.at(start), points.at(end), points.at(test)) >
            CustomFinImporter::kTolerance)
        {
            return false;
        }
    }
    return true;
}

/// The error code of a file QImageReader could not read.
[[nodiscard]] ErrorCode readErrorCode(QImageReader::ImageReaderError error)
{
    switch (error)
    {
        case QImageReader::FileNotFoundError:
        case QImageReader::DeviceError:
            return ErrorCode::IO;
        case QImageReader::UnsupportedFormatError:
            return ErrorCode::UNSUPPORTED_FORMAT;
        case QImageReader::InvalidDataError:
        case QImageReader::UnknownError:
            break;
    }
    return ErrorCode::PARSE;
}

}  // namespace

Result<std::vector<Coordinate>> CustomFinImporter::getPoints(const std::filesystem::path& file)
{
    const Result<QImage> image = readImage(file);
    if (!image.has_value())
    {
        return std::unexpected(image.error());
    }
    return getPoints(*image);
}

Result<QImage> CustomFinImporter::readImage(const std::filesystem::path& file)
{
    QImageReader reader(QtStrings::fromPath(file));
    // ImageIO.read() picks its reader by the content and ignores the orientation metadata.
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(false);
    QImage image = reader.read();
    if (image.isNull())
    {
        //: CustomFinImport.errorLoadingFile
        return fail(
            readErrorCode(reader.error()),
            QtStrings::toUtf8(tr("Error loading file:") + QChar(u' ') + reader.errorString()));
    }
    return image;
}

Result<std::vector<Coordinate>> CustomFinImporter::getPoints(const QImage& image)
{
    Result<std::vector<Coordinate>> points = tracePoints(image);
    if (!points.has_value())
    {
        return points;
    }

    // Optimize the loaded fin (Java: a do-while loop; points is never empty, so a first pass
    // always runs)
    std::size_t count = 0;
    while (count != points->size())
    {
        count = points->size();
        optimizePoints(*points);
    }

    return points;
}

Result<std::vector<Coordinate>> CustomFinImporter::tracePoints(const QImage& image)
{
    const FinMask            mask(image);
    const std::optional<int> startX = mask.startX();
    if (!startX.has_value())
    {
        //: CustomFinImport.badFinImage
        return fail(ErrorCode::PARSE,
                    QtStrings::toUtf8(tr("Invalid fin image. Make sure the fin is a solid black or "
                                         "dark color and touching the bottom of the image.")));
    }

    // Load the fin
    std::vector<Coordinate> points{Coordinate::kNul};
    const int               height = mask.height();
    WalkState               walk{.x = *startX, .y = height - 1};
    // Brent's cycle detection: the walk goes round for ever when it comes back to a state it was
    // in; the saved state moves on at every power of two of the steps since it was saved.
    WalkState   saved       = walk;
    std::size_t power       = 1;
    std::size_t sinceSaving = 0;
    // Java's do-while loop: a step, then whether the walk goes on.
    const auto goesOn = [&walk, height] {
        return !walk.offBottom || (walk.y < height - 1 && walk.y >= 0);
    };
    for (bool more = true; more; more = goesOn())
    {
        step(mask, walk);
        if (mask.isFin(walk.x, walk.y))
        {
            const double x = static_cast<double>(walk.x - *startX) * kPixelSize;
            const double y = static_cast<double>(height - walk.y - 1) * kPixelSize;
            points.emplace_back(x, y);
        }
        if (walk == saved)
        {
            //: CustomFinImport.error.badimage
            return fail(ErrorCode::PARSE,
                        QtStrings::toUtf8(tr("Could not deduce fin shape from image.")));
        }
        if (++sinceSaving == power)
        {
            saved = walk;
            power *= 2;
            sinceSaving = 0;
        }
    }

    return points;
}

void CustomFinImporter::optimizePoints(std::vector<Coordinate>& points)
{
    // Java walks the list with list iterators and compares the points by identity: the index of
    // a point here (every point of the walk is an object of its own).
    std::size_t start = 0;
    while (start + 1 < points.size())
    {
        bool        removedSection = false;
        std::size_t end            = points.size() - 1;
        for (; end != start; --end)
        {
            if (isStraight(points, start, end))
            {
                // Entire segment was within distance, it's a straight line.
                // Remove all but the first and last point
                points.erase(std::next(points.begin(), static_cast<std::ptrdiff_t>(start + 1)),
                             std::next(points.begin(), static_cast<std::ptrdiff_t>(end)));
                start          = start + 1;
                removedSection = true;
                break;
            }
        }
        if (!removedSection && end == start)
        {
            start = start + 1;
        }
    }
}

QRgb CustomFinImporter::javaRgb(const QImage& image, int x, int y)
{
    switch (image.format())
    {
        case QImage::Format_Grayscale8:
        {
            const int value = srgbOfGrey8().at(static_cast<std::size_t>(qRed(image.pixel(x, y))));
            return qRgb(value, value, value);
        }
        case QImage::Format_Grayscale16:
        {
            const int value =
                srgb8(static_cast<double>(image.pixelColor(x, y).rgba64().red()) / kMaxLevel16);
            return qRgb(value, value, value);
        }
        default:
            return image.pixel(x, y);
    }
}

bool CustomFinImporter::isBlack(QRgb rgb) noexcept
{
    // Clear alpha, we don't care about it; convert to black & white
    const int red   = qRed(rgb);
    const int green = qGreen(rgb);
    const int blue  = qBlue(rgb);
    const int pixel = static_cast<int>((0.299 * red) + (0.587 * green) + (0.114 * blue));
    return pixel <= kWhiteAbove;
}

QString CustomFinImporter::imageNameFilter()
{
    QStringList patterns;
    for (const QByteArray& format : QImageReader::supportedImageFormats())
    {
        patterns.append(QStringLiteral("*.") + QString::fromLatin1(format).toLower());
    }
    patterns.sort();
    patterns.removeDuplicates();
    //: FileHelper.IMAGES
    return tr("Image files") + QStringLiteral(" (") + patterns.join(QChar(u' ')) + QChar(u')');
}

}  // namespace QtRocket::Gui
