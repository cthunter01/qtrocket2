#pragma once

#include <filesystem>
#include <vector>

#include <QCoreApplication>
#include <QRgb>
#include <QString>

#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

class QImage;

namespace QtRocket::Gui
{

/// The outline of a fin read from an image (OpenRocket's CustomFinImporter, swing
/// gui/util/CustomFinImporter.java), for the "Import from image" of the freeform fin editor
/// (FreeformFinSetConfig::importImage()). The fin is the dark area of the image that touches its
/// bottom row, which is the fin's root; one pixel is one millimetre.
///
/// getPoints() does what Java's does:
/// 1. validateImage(): every pixel is black or white: black when
///    (int)(0.299 r + 0.587 g + 0.114 b) <= 200 (isBlack(), in doubles as Java computes it), the
///    colour being the one Java's BufferedImage.getRGB() gives (javaRgb()); its alpha is ignored,
///    so a transparent pixel counts by its colour (a transparent black background is fin). An
///    image without a black pixel on its bottom row fails with Java's text "Invalid fin image.
///    Make sure the fin is a solid black or dark color and touching the bottom of the image."
///    (CustomFinImport.badFinImage).
/// 2. loadFin(): a walk along the fin's outline that keeps the fin on its left hand: it starts on
///    the leftmost black pixel of the bottom row, facing up, and at every step turns left if the
///    pixel there is fin, else goes on if the pixel ahead is, else turns right if the pixel there
///    is, else turns round; it then moves one pixel ahead (never off the image) and adds the pixel
///    as a point when it is fin (repeated points included). The walk ends when it comes back to
///    the bottom row after having left it. The points are (0, 0) and then, for the pixel (x, y),
///    ((x - start x) * 0.001, (height - y - 1) * 0.001) m.
/// 3. optimizePoints(), repeated until a pass removes nothing: the points between a start point
///    and the furthest later point for which every point between lies within 0.8 mm of the
///    segment from the start to it are removed, and the next point becomes the start.
///
/// Deviations from OpenRocket:
/// - A walk that never comes back to the bottom row fails with "Could not deduce fin shape from
///   image." (CustomFinImport.error.badimage, a text Java has but does not use). Java's walk goes
///   round for ever, adding points until the memory runs out: a fin only one pixel tall (on the
///   bottom row only) or an image of one black pixel. The walk is deterministic, so it goes round
///   for ever exactly when it comes back to a state (pixel, direction, whether it has left the
///   bottom row) it was in before: Brent's cycle detection finds that without memory.
/// - The black-and-white decision is kept apart from the image. Java writes it back into the
///   image (setRGB(0) or setRGB(0xFFFFFF)) and reads it from there, which an image of indexed
///   colours (a palette PNG, a GIF, an 8-bit BMP) turns into the nearest colour of its palette:
///   with no pure black in the palette, or with a transparent colour in it (which the black's
///   alpha 0 picks), no fin pixel is left, and Java's walk gives the single point (0, 0)
///   (tests/data/fin-images: palette-without-black.png, palette-transparent.png). Here such an
///   image gives the outline an image of the same colours without a palette gives.
/// - Java reads the image with ImageIO.read(), which picks its reader by the file's content;
///   here QImageReader does, also by the content and without turning the image by its
///   orientation metadata (as ImageIO), with the formats Qt reads (imageNameFilter()). A file Qt
///   cannot read as an image fails with "Error loading file:" and Qt's reason
///   (CustomFinImport.errorLoadingFile, unused in Java); Java shows the text of ImageIO's
///   IOException ("Can't read input file!", "Error reading PNG image data") or, for a file that
///   is no image at all, ends in a NullPointerException. An image beyond Qt's allocation limit
///   (QImageReader::allocationLimit(), 256 MB by default) fails the same way; Java reads every
///   image its heap holds.
/// - The colour Java's getRGB() gives is reproduced for 8- and 16-bit greyscale images
///   (javaRgb()), whose levels Java converts from linear grey to sRGB; a greyscale image with
///   alpha, which Qt reads as ARGB32, keeps its levels (Java converts them too:
///   greyscale-alpha.png), as do images with a colour profile of their own, which Java converts
///   through it.
/// - The point list is a std::vector and the points are compared by their index; Java compares
///   them by identity in its list iterators, which is the same, every point being an object of
///   its own.
///
/// Texts through tr() with their messages.properties keys.
class CustomFinImporter
{
    Q_DECLARE_TR_FUNCTIONS(CustomFinImporter)

public:
    CustomFinImporter() = delete;

    /// The size of a pixel, in m.
    static constexpr double kPixelSize = 0.001;
    /// How far, in m, a point may lie from a segment and be dropped (optimizePoints()).
    static constexpr double kTolerance = 0.0008;
    /// The grey (int)(0.299 r + 0.587 g + 0.114 b) above which a pixel is white.
    static constexpr int kWhiteAbove = 200;

    /// The outline of the fin in the image file @p file (getPoints(File)): steps 1 to 3 of the
    /// class comment.
    /// @return the points, at least (0, 0); a failure with the text the editor shows for a file
    ///         that is no image Qt reads, an image without a fin on its bottom row, or a walk
    ///         that never ends
    [[nodiscard]] static Result<std::vector<Coordinate>> getPoints(
        const std::filesystem::path& file);

    /// The image in @p file as getPoints() reads it (ImageIO.read() in Java): with the reader its
    /// content asks for, not turned by its orientation metadata.
    /// @return the image; a failure with "Error loading file:" and Qt's reason
    [[nodiscard]] static Result<QImage> readImage(const std::filesystem::path& file);

    /// The same for an image already read (a null image is one without a fin).
    [[nodiscard]] static Result<std::vector<Coordinate>> getPoints(const QImage& image);

    /// Steps 1 and 2 of the class comment: the points of the walk before optimizePoints(), the
    /// first one (0, 0); the failures of getPoints().
    [[nodiscard]] static Result<std::vector<Coordinate>> tracePoints(const QImage& image);

    /// One pass of step 3 of the class comment over @p points (optimizePoints()).
    static void optimizePoints(std::vector<Coordinate>& points);

    /// The colour of the pixel (@p x, @p y) of @p image as Java's BufferedImage.getRGB() gives it
    /// for the image ImageIO reads from the same file: QImage::pixel(), except for an 8- or
    /// 16-bit greyscale image (QImage::Format_Grayscale8, Format_Grayscale16: a greyscale PNG or
    /// JPEG), whose level is taken as linear grey and converted to sRGB as Java's colour model
    /// converts it (the sRGB transfer function of IEC 61966-2-1, rounded to 8 bits: 100 gives
    /// 168, 149 gives 201). Greyscale images of 1, 2 or 4 bits are palettes in both.
    [[nodiscard]] static QRgb javaRgb(const QImage& image, int x, int y);

    /// Whether the colour @p rgb is black for the fin (step 1 of the class comment); the alpha is
    /// ignored.
    [[nodiscard]] static bool isBlack(QRgb rgb) noexcept;

    /// The name filter of the file dialog (FileHelper.getImageFileFilter()): "Image files"
    /// (FileHelper.IMAGES) and the suffixes of the formats QImageReader reads, "*.<format>" in
    /// lower case, sorted, separated by spaces as Qt's name filters are (Java lists the suffixes
    /// of ImageIO's readers, separated by "; ").
    [[nodiscard]] static QString imageNameFilter();
};

}  // namespace QtRocket::Gui
