#include "QtRocket/gui/util/CustomFinImporter.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QRect>
#include <QRgb>
#include <QRgba64>
#include <QString>
#include <QStringList>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include <gtest/gtest.h>

#include "FinImportReport.h"
#include "GuiTestSupport.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

using CustomFinImporterTest = Test::GuiTest;
using Test::finImage;
using Test::finImageTestName;
using Test::FinImportReport;
using Test::outlineText;
using Test::parseOutline;

/// CustomFinImport.badFinImage and CustomFinImport.error.badimage.
constexpr std::string_view kBadFinImage =
    "Invalid fin image. Make sure the fin is a solid black "
    "or dark color and touching the bottom of the image.";
constexpr std::string_view kNoFinShape = "Could not deduce fin shape from image.";

/// What getPoints() or tracePoints() gives, in the report's notation: "N: POINTS", or "error:
/// MESSAGE".
[[nodiscard]] std::string resultText(const Result<std::vector<Coordinate>>& result)
{
    return result.has_value() ? outlineText(*result) : "error: " + result.error().message;
}

/// Java's line @p key ("raw", "points") of the block of @p image in the report, in
/// resultText()'s notation: "invalid" and "error CLASS: MESSAGE" become "error: <message>",
/// "never ends" stays.
[[nodiscard]] std::string javaLine(std::string_view image, std::string_view key)
{
    const std::optional<std::string> line =
        FinImportReport::get().line("image " + std::string(image), key);
    if (!line.has_value())
    {
        return "(no line)";
    }
    if (*line == "invalid")
    {
        return "error: " + std::string(kBadFinImage);
    }
    if (line->starts_with("error "))
    {
        return "error: " + line->substr(line->find(": ") + 2);
    }
    return *line;
}

/// javaLine() with QtRocket's differences by design (CustomFinImporter's class comment):
/// - an image whose palette has no pure black, or a transparent colour: Java's write-back into
///   the palette leaves no fin pixel (the single point (0, 0)); here the fin is the rectangle of
///   rectangle.png, which has the same fin without a palette;
/// - greyscale with alpha: Java converts the levels to sRGB (149 becomes 201, white); here they
///   stay (149 is black), which leaves the notches of threshold.png, whose other two notches are
///   at the same places;
/// - a TIFF with a transparent white background: Qt's premultiplied image has lost the white,
///   and the whole image is fin, as in full-black.png, of the same size;
/// - a walk that never ends fails.
[[nodiscard]] std::string expectedLine(std::string_view image, std::string_view key)
{
    if (image == "palette-without-black.png" || image == "palette-transparent.png")
    {
        return javaLine("rectangle.png", key);
    }
    if (image == "transparent-white.tiff")
    {
        return javaLine("full-black.png", key);
    }
    if (image == "greyscale-alpha.png")
    {
        return javaLine("threshold.png", key);
    }
    if (javaLine(image, key) == "never ends")
    {
        return "error: " + std::string(kNoFinShape);
    }
    return javaLine(image, key);
}

/// The images of the report that Qt reads, in its order.
constexpr std::array<std::string_view, 25> kImages{
    "trapezoid.png",
    "rectangle.png",
    "floating.png",
    "white.png",
    "threshold.png",
    "colours.png",
    "greyscale.png",
    "greyscale16.png",
    "greyscale4.png",
    "greyscale-alpha.png",
    "transparent.png",
    "transparent-background.png",
    "palette-without-black.png",
    "palette-with-black.png",
    "palette-transparent.png",
    "one-pixel-tall.png",
    "single-pixel.png",
    "one-pixel-wide.png",
    "full-black.png",
    "one-by-one.png",
    "edges.png",
    "staircase.png",
    "overhang.png",
    "arch.png",
    "swept.png",
};

/// The TIFF images of the report, after the others: Qt reads them only with the TIFF plugin of
/// its imageformats module, which CI's Qt (qtbase and qtsvg) lacks.
constexpr std::array<std::string_view, 2> kTiffImages{"transparent-white.tiff",
                                                      "semi-transparent.tiff"};

/// Whether Qt reads TIFF files here.
[[nodiscard]] bool readsTiff()
{
    return QImageReader::supportedImageFormats().contains(QByteArrayLiteral("tiff"));
}

/// The files of the report that are no image Qt reads, after the images.
constexpr std::array<std::string_view, 3> kNoImages{"not-an-image.png", "truncated.png",
                                                    "missing.png"};

/// The point lists of the report's "optimize" blocks.
constexpr std::array<std::string_view, 11> kOptimizeCases{"square", "tolerance", "beyond", "ends",
                                                          "behind", "past",      "stairs", "zigzag",
                                                          "two",    "one",       "loop"};

/// The report's headers of the blocks that start with @p kind (followed by a space).
[[nodiscard]] std::vector<std::string> headersOf(std::string_view kind)
{
    std::vector<std::string> result;
    for (const std::string& header : FinImportReport::get().headers())
    {
        if (header.starts_with(std::string(kind) + ' '))
        {
            result.push_back(header.substr(kind.size() + 1));
        }
    }
    return result;
}

/// @p names as strings.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> namesOf(const std::array<std::string_view, N>& names)
{
    return {names.begin(), names.end()};
}

TEST_F(CustomFinImporterTest, TheReportHasTheImagesAndCasesOfTheTests)
{
    std::vector<std::string> images = namesOf(kImages);
    images.insert(images.end(), kTiffImages.begin(), kTiffImages.end());
    images.insert(images.end(), kNoImages.begin(), kNoImages.end());
    EXPECT_EQ(headersOf("image"), images);
    EXPECT_EQ(headersOf("optimize"), namesOf(kOptimizeCases));
}

TEST_F(CustomFinImporterTest, TheScoutsThreeImagesGiveJavasPoints)
{
    // The M2 scouting's FinImageProbe on OpenRocket (config-dialogs, java-probe/out.txt): its
    // images are byte for byte those of tests/data/fin-images.
    EXPECT_EQ(resultText(CustomFinImporter::getPoints(finImage("trapezoid.png"))),
              "4: (0.0,0.0) (0.02,0.039) (0.058,0.039) (0.039,0.0)");
    EXPECT_EQ(resultText(CustomFinImporter::getPoints(finImage("rectangle.png"))),
              "4: (0.0,0.0) (0.0,0.019) (0.029,0.019) (0.029,0.0)");
    // "floating error: LocalizedIOException CustomFinImport.badFinImage"
    const Result<std::vector<Coordinate>> floating =
        CustomFinImporter::getPoints(finImage("floating.png"));
    ASSERT_FALSE(floating.has_value());
    EXPECT_EQ(floating.error().message, kBadFinImage);
    EXPECT_EQ(floating.error().code, ErrorCode::PARSE);
}

TEST_F(CustomFinImporterTest, OpenRocketsLinesOfTheDifferencesAreAsDocumented)
{
    // Java's palette write-back loses the fin: the single point (0, 0).
    EXPECT_EQ(javaLine("palette-without-black.png", "points"), "1: (0.0,0.0)");
    EXPECT_EQ(javaLine("palette-transparent.png", "points"), "1: (0.0,0.0)");
    EXPECT_NE(expectedLine("palette-without-black.png", "points"), "1: (0.0,0.0)");
    // Java converts the greyscale with alpha to sRGB: all four notches.
    EXPECT_EQ(javaLine("greyscale-alpha.png", "points"),
              "17: (0.0,0.0) (0.0,0.019) (0.002,0.019) (0.002,0.015) (0.006,0.015) (0.006,0.019) "
              "(0.007,0.015) (0.011,0.015) (0.011,0.019) (0.012,0.015) (0.016,0.015) "
              "(0.016,0.019) (0.017,0.015) (0.021,0.015) (0.021,0.019) (0.029,0.019) "
              "(0.029,0.0)");
    EXPECT_NE(expectedLine("greyscale-alpha.png", "points"),
              javaLine("greyscale-alpha.png", "points"));
    // Java's TIFF reader keeps the colour under the alpha: the black block of
    // transparent-white.tiff only, and the notch of semi-transparent.tiff's alpha 100 white.
    EXPECT_EQ(javaLine("transparent-white.tiff", "points"),
              "4: (0.0,0.0) (0.0,0.004) (0.005,0.004) (0.005,0.0)");
    EXPECT_NE(expectedLine("transparent-white.tiff", "points"),
              javaLine("transparent-white.tiff", "points"));
    EXPECT_EQ(javaLine("semi-transparent.tiff", "points"),
              "8: (0.0,0.0) (0.0,0.011) (0.003,0.011) (0.003,0.007) (0.007,0.007) (0.007,0.011) "
              "(0.014,0.011) (0.014,0.0)");
    EXPECT_EQ(expectedLine("semi-transparent.tiff", "points"),
              javaLine("semi-transparent.tiff", "points"));
    // Java's walk goes round until the heap is full.
    EXPECT_EQ(javaLine("one-pixel-tall.png", "points"), "never ends");
    EXPECT_EQ(javaLine("one-by-one.png", "raw"), "never ends");
    // ImageIO's texts, and the NullPointerException of an image it has no reader for.
    EXPECT_EQ(javaLine("missing.png", "points"), "error: Can't read input file!");
    EXPECT_EQ(javaLine("truncated.png", "points"), "error: Error reading PNG image data");
    EXPECT_EQ(javaLine("not-an-image.png", "points"),
              "error: Cannot invoke \"java.awt.image.BufferedImage.getHeight()\" because \"pic\" "
              "is null");
}

class FinImportGolden : public Test::GuiTest, public ::testing::WithParamInterface<std::string_view>
{ };

TEST_P(FinImportGolden, TracesAndSimplifiesAsOpenRocket)
{
    const std::string_view image = GetParam();
    if (image.ends_with(".tiff") && !readsTiff())
    {
        GTEST_SKIP() << "Qt reads no TIFF here (the TIFF plugin of Qt's imageformats module)";
    }
    const Result<QImage> read = CustomFinImporter::readImage(finImage(image));
    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(resultText(CustomFinImporter::tracePoints(*read)), expectedLine(image, "raw"));
    EXPECT_EQ(resultText(CustomFinImporter::getPoints(finImage(image))),
              expectedLine(image, "points"));
}

INSTANTIATE_TEST_SUITE_P(Images, FinImportGolden, ::testing::ValuesIn(kImages),
                         [](const ::testing::TestParamInfo<std::string_view>& param) {
                             return finImageTestName(param.param);
                         });
INSTANTIATE_TEST_SUITE_P(TiffImages, FinImportGolden, ::testing::ValuesIn(kTiffImages),
                         [](const ::testing::TestParamInfo<std::string_view>& param) {
                             return finImageTestName(param.param);
                         });

class FinOptimizeGolden : public Test::GuiTest,
                          public ::testing::WithParamInterface<std::string_view>
{ };

/// getPoints()' loop of optimizePoints() over @p points, until a pass removes nothing:
/// "passes=P N: POINTS".
[[nodiscard]] std::string loopText(std::vector<Coordinate> points)
{
    int         passes = 0;
    std::size_t count  = 0;
    while (count != points.size())
    {
        count = points.size();
        CustomFinImporter::optimizePoints(points);
        ++passes;
    }
    return std::format("passes={} {}", passes, outlineText(points));
}

TEST_P(FinOptimizeGolden, SimplifiesAsOpenRocket)
{
    const std::string      header = "optimize " + std::string(GetParam());
    const FinImportReport& report = FinImportReport::get();
    const std::string      input  = report.line(header, "input").value_or("");
    ASSERT_FALSE(input.empty());
    std::vector<Coordinate> once = parseOutline(input);
    CustomFinImporter::optimizePoints(once);
    EXPECT_EQ(outlineText(once), report.line(header, "once"));
    EXPECT_EQ(loopText(parseOutline(input)), report.line(header, "loop"));
}

INSTANTIATE_TEST_SUITE_P(Points, FinOptimizeGolden, ::testing::ValuesIn(kOptimizeCases),
                         [](const ::testing::TestParamInfo<std::string_view>& param) {
                             return std::string(param.param);
                         });

/// The sRGB values javaRgb() gives for the levels 0 to 255 of an 8-bit greyscale image, as the
/// report's "srgb" line writes them.
[[nodiscard]] std::string grey8Text()
{
    QImage      image(256, 1, QImage::Format_Grayscale8);
    std::string text;
    for (int level = 0; level < image.width(); ++level)
    {
        image.setPixelColor(level, 0, QColor(level, level, level));
        text += std::format("{}{}", text.empty() ? "" : " ",
                            qBlue(CustomFinImporter::javaRgb(image, level, 0)));
    }
    return text;
}

/// The sRGB values javaRgb() gives for the levels 0 to 65535 of a 16-bit greyscale image, as the
/// report's "firsts" line writes them: the first level of each value.
[[nodiscard]] std::string grey16Text()
{
    QImage      image(256, 256, QImage::Format_Grayscale16);
    std::string text;
    int         last = -1;
    for (int level = 0; level < 65536; ++level)
    {
        const int  x     = level % 256;
        const int  y     = level / 256;
        const auto value = static_cast<quint16>(level);
        // In the line's native byte order, as QImage keeps it (setPixelColor() rounds).
        const std::span<uchar> line(image.scanLine(y),
                                    static_cast<std::size_t>(image.bytesPerLine()));
        std::memcpy(line.subspan(static_cast<std::size_t>(x) * sizeof value, sizeof value).data(),
                    &value, sizeof value);
        if (image.pixelColor(x, y).rgba64().red() != value)
        {
            return std::format("the level {} is not kept", level);
        }
        if (const int srgb = qBlue(CustomFinImporter::javaRgb(image, x, y)); srgb != last)
        {
            text += std::format("{}{}@{}", text.empty() ? "" : " ", srgb, level);
            last = srgb;
        }
    }
    return text;
}

TEST_F(CustomFinImporterTest, GreyscaleLevelsBecomeJavasSrgbValues)
{
    // Java's colour model converts a linear greyscale image's levels to sRGB in getRGB(): every
    // level of 8 and of 16 bits.
    const FinImportReport& report = FinImportReport::get();
    EXPECT_EQ(grey8Text(), report.line("grey8", "srgb"));
    EXPECT_EQ(grey16Text(), report.line("grey16", "firsts"));
    // Greyscale images of fewer bits are palettes of the levels in both.
    EXPECT_EQ(report.line("grey4", "srgb"),
              "0 17 34 51 68 85 102 119 136 153 170 187 204 221 238 255");
    EXPECT_EQ(report.line("grey2", "srgb"), "0 85 170 255");
    EXPECT_EQ(report.line("grey1", "srgb"), "0 255");
    // Java converts a greyscale image with alpha as well (Qt reads it as ARGB32: a difference).
    EXPECT_EQ(report.line("grey-alpha8", "srgb"), report.line("grey8", "srgb"));
}

TEST_F(CustomFinImporterTest, ReadsTheTestImagesInTheFormatsTheConversionExpects)
{
    // Java's BufferedImage types: TYPE_BYTE_GRAY (10), TYPE_USHORT_GRAY (11), TYPE_BYTE_BINARY
    // (12), TYPE_CUSTOM (0), TYPE_4BYTE_ABGR (6), TYPE_BYTE_INDEXED (13).
    const FinImportReport& report = FinImportReport::get();
    EXPECT_EQ(report.line("image greyscale.png", "read"), "type=10 size=40x30");
    EXPECT_EQ(report.line("image greyscale16.png", "read"), "type=11 size=40x30");
    EXPECT_EQ(report.line("image greyscale4.png", "read"), "type=12 size=40x30");
    EXPECT_EQ(report.line("image greyscale-alpha.png", "read"), "type=0 size=40x30");
    EXPECT_EQ(report.line("image transparent.png", "read"), "type=6 size=40x30");
    EXPECT_EQ(report.line("image palette-transparent.png", "read"), "type=13 size=40x30");
    EXPECT_EQ(report.line("image semi-transparent.tiff", "read"), "type=0 size=30x20");
    EXPECT_EQ(CustomFinImporter::readImage(finImage("greyscale.png"))->format(),
              QImage::Format_Grayscale8);
    EXPECT_EQ(CustomFinImporter::readImage(finImage("greyscale16.png"))->format(),
              QImage::Format_Grayscale16);
    EXPECT_EQ(CustomFinImporter::readImage(finImage("greyscale4.png"))->format(),
              QImage::Format_Indexed8);
    EXPECT_EQ(CustomFinImporter::readImage(finImage("greyscale-alpha.png"))->format(),
              QImage::Format_ARGB32);
    EXPECT_EQ(CustomFinImporter::readImage(finImage("transparent.png"))->format(),
              QImage::Format_ARGB32);
    EXPECT_EQ(CustomFinImporter::readImage(finImage("palette-transparent.png"))->format(),
              QImage::Format_Indexed8);
}

TEST_F(CustomFinImporterTest, KeepsTheLevelsOfAGreyscaleImageWithAlpha)
{
    // The notch of level 149 (x 8 to 10): sRGB 201 in Java (white), 149 here (black).
    const Result<QImage> image = CustomFinImporter::readImage(finImage("greyscale-alpha.png"));
    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(CustomFinImporter::javaRgb(*image, 9, 11), qRgb(149, 149, 149));
    EXPECT_TRUE(CustomFinImporter::isBlack(CustomFinImporter::javaRgb(*image, 9, 11)));
    // In the plain greyscale image, the same level is Java's 201.
    const Result<QImage> grey = CustomFinImporter::readImage(finImage("greyscale.png"));
    ASSERT_TRUE(grey.has_value());
    EXPECT_EQ(CustomFinImporter::javaRgb(*grey, 14, 11), qRgb(201, 201, 201));
    EXPECT_FALSE(CustomFinImporter::isBlack(CustomFinImporter::javaRgb(*grey, 14, 11)));
}

TEST_F(CustomFinImporterTest, TheThresholdIsJavasInDoublesAndIgnoresAlpha)
{
    // (int)(0.299 r + 0.587 g + 0.114 b) > 200 is white.
    EXPECT_TRUE(CustomFinImporter::isBlack(qRgb(200, 200, 200)));   // 200.0
    EXPECT_FALSE(CustomFinImporter::isBlack(qRgb(201, 201, 201)));  // 201.0
    EXPECT_TRUE(CustomFinImporter::isBlack(qRgb(149, 217, 255)));   // 200.99999999999997
    EXPECT_FALSE(CustomFinImporter::isBlack(qRgb(202, 202, 202)));  // 201.99999999999997
    EXPECT_TRUE(CustomFinImporter::isBlack(qRgb(255, 0, 0)));       // 76.245
    EXPECT_FALSE(CustomFinImporter::isBlack(qRgb(255, 255, 0)));    // 225.93
    EXPECT_TRUE(CustomFinImporter::isBlack(qRgba(0, 0, 0, 0)));     // transparent black
    EXPECT_FALSE(CustomFinImporter::isBlack(qRgba(255, 255, 255, 0)));
    // The transparent image: its transparent black is fin, its transparent white background not.
    const Result<QImage> image = CustomFinImporter::readImage(finImage("transparent.png"));
    ASSERT_TRUE(image.has_value());
    EXPECT_EQ(CustomFinImporter::javaRgb(*image, 6, 20), qRgba(0, 0, 0, 0));
    EXPECT_EQ(CustomFinImporter::javaRgb(*image, 0, 0), qRgba(255, 255, 255, 0));
}

TEST_F(CustomFinImporterTest, QtReadsATiffWithAlphaPremultiplied)
{
    if (!readsTiff())
    {
        GTEST_SKIP() << "Qt reads no TIFF here (the TIFF plugin of Qt's imageformats module)";
    }
    const Result<QImage> image = CustomFinImporter::readImage(finImage("semi-transparent.tiff"));
    ASSERT_TRUE(image.has_value()) << image.error().message;
    EXPECT_EQ(image->format(), QImage::Format_ARGB32_Premultiplied);
    // The alpha 100 white of the bottom row: premultiplied (100, 100, 100) in the image, white as
    // Java's getRGB() gives it.
    EXPECT_EQ(image->pixel(4, 19), qRgba(100, 100, 100, 100));
    EXPECT_EQ(CustomFinImporter::javaRgb(*image, 4, 19), qRgba(255, 255, 255, 100));
    // The transparent white of transparent-white.tiff is gone: transparent black, fin.
    const Result<QImage> white = CustomFinImporter::readImage(finImage("transparent-white.tiff"));
    ASSERT_TRUE(white.has_value()) << white.error().message;
    EXPECT_EQ(CustomFinImporter::javaRgb(*white, 0, 0), qRgba(0, 0, 0, 0));
}

/// semi-transparent.tiff as FinImportDumper sets its pixels, in @p format.
[[nodiscard]] QImage semiTransparentImage(QImage::Format format)
{
    QImage image(30, 20, QImage::Format_ARGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(QRect(5, 8, 15, 12), Qt::black);
        const QColor semiWhite(255, 255, 255, 100);
        painter.fillRect(QRect(4, 19, 1, 1), semiWhite);
        painter.fillRect(QRect(9, 8, 3, 4), semiWhite);
    }
    return image.convertToFormat(format);
}

TEST_F(CustomFinImporterTest, APremultipliedImageGivesJavasColours)
{
    // Any premultiplied image, as Qt's TIFF reader gives one (also where Qt reads no TIFF): the
    // colour without the alpha's factor, so semi-transparent.tiff's points are Java's.
    const QImage premultiplied = semiTransparentImage(QImage::Format_ARGB32_Premultiplied);
    EXPECT_EQ(premultiplied.pixel(4, 19), qRgba(100, 100, 100, 100));
    EXPECT_EQ(CustomFinImporter::javaRgb(premultiplied, 4, 19), qRgba(255, 255, 255, 100));
    EXPECT_FALSE(CustomFinImporter::isBlack(CustomFinImporter::javaRgb(premultiplied, 4, 19)));
    EXPECT_EQ(resultText(CustomFinImporter::getPoints(premultiplied)),
              javaLine("semi-transparent.tiff", "points"));
    EXPECT_EQ(resultText(CustomFinImporter::tracePoints(premultiplied)),
              javaLine("semi-transparent.tiff", "raw"));
    // The same image not premultiplied gives the same points.
    EXPECT_EQ(resultText(CustomFinImporter::getPoints(semiTransparentImage(QImage::Format_ARGB32))),
              javaLine("semi-transparent.tiff", "points"));
    // A 16-bit premultiplied image too.
    const QImage wide = semiTransparentImage(QImage::Format_RGBA64_Premultiplied);
    EXPECT_EQ(CustomFinImporter::javaRgb(wide, 4, 19), qRgba(255, 255, 255, 100));
}

TEST_F(CustomFinImporterTest, FilesThatAreNoImageFailWithQtsReason)
{
    const Result<std::vector<Coordinate>> missing =
        CustomFinImporter::getPoints(finImage("missing.png"));
    ASSERT_FALSE(missing.has_value());
    EXPECT_TRUE(missing.error().message.starts_with("Error loading file: "))
        << missing.error().message;
    EXPECT_EQ(missing.error().code, ErrorCode::IO);

    const Result<std::vector<Coordinate>> text =
        CustomFinImporter::getPoints(finImage("not-an-image.png"));
    ASSERT_FALSE(text.has_value());
    EXPECT_TRUE(text.error().message.starts_with("Error loading file: ")) << text.error().message;
    EXPECT_EQ(text.error().code, ErrorCode::UNSUPPORTED_FORMAT);

    // (libpng prints "libpng error: Read Error" to the standard error itself.)
    const Result<std::vector<Coordinate>> truncated =
        CustomFinImporter::getPoints(finImage("truncated.png"));
    ASSERT_FALSE(truncated.has_value());
    EXPECT_TRUE(truncated.error().message.starts_with("Error loading file: "))
        << truncated.error().message;
    EXPECT_EQ(truncated.error().code, ErrorCode::PARSE);
}

TEST_F(CustomFinImporterTest, ANullImageHasNoFin)
{
    const Result<std::vector<Coordinate>> points = CustomFinImporter::getPoints(QImage());
    ASSERT_FALSE(points.has_value());
    EXPECT_EQ(points.error().message, kBadFinImage);
}

/// A white image of @p width x @p height with the black rectangle @p fin.
[[nodiscard]] QImage imageWith(int width, int height, const QRect& fin)
{
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.fillRect(fin, Qt::black);
    return image;
}

TEST_F(CustomFinImporterTest, AWalkThatNeverEndsFailsSoon)
{
    // A fin one pixel tall along most of a wide image: Java's walk goes back and forth on the
    // bottom row for ever. Brent's cycle detection stops it after a few rounds.
    const Result<std::vector<Coordinate>> points =
        CustomFinImporter::tracePoints(imageWith(4000, 3, QRect(10, 2, 3980, 1)));
    ASSERT_FALSE(points.has_value());
    EXPECT_EQ(points.error().message, kNoFinShape);
    EXPECT_EQ(points.error().code, ErrorCode::PARSE);
}

TEST_F(CustomFinImporterTest, ALargeImageGivesItsOutline)
{
    // An elliptical fin 600 x 300 px in an image of 800 x 400: the outline from the leftmost
    // pixel of the root to the right end, simplified.
    QImage image(800, 400, QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawEllipse(QRect(100, 100, 600, 600));
    }
    const Result<std::vector<Coordinate>> points = CustomFinImporter::getPoints(image);
    ASSERT_TRUE(points.has_value()) << points.error().message;
    ASSERT_GE(points->size(), 3U);
    EXPECT_EQ(points->front().x, 0.0);
    EXPECT_EQ(points->front().y, 0.0);
    EXPECT_EQ(points->back().y, 0.0);
    EXPECT_GT(points->back().x, 0.5);
    EXPECT_LT(points->size(), 200U);
}

TEST_F(CustomFinImporterTest, TheNameFilterListsTheFormatsQtReads)
{
    const QString filter = CustomFinImporter::imageNameFilter();
    ASSERT_TRUE(filter.startsWith(QStringLiteral("Image files ("))) << filter.toStdString();
    ASSERT_TRUE(filter.endsWith(QChar(u')')));
    const QStringList patterns =
        filter.mid(13, filter.size() - 14).split(QChar(u' '), Qt::SkipEmptyParts);
    EXPECT_TRUE(patterns.contains(QStringLiteral("*.png")));
    QStringList sorted = patterns;
    sorted.sort();
    sorted.removeDuplicates();
    EXPECT_EQ(patterns, sorted);
    EXPECT_EQ(patterns.filter(QStringLiteral("*.")).size(), patterns.size());
    EXPECT_EQ(filter, filter.toLower().replace(0, 1, QChar(u'I')));
}

}  // namespace

}  // namespace QtRocket::Gui
