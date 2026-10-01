#include "QtRocket/file/XmlScanner.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

// The scanner follows the JDK 17 classes that do the work (XMLVersionDetector,
// XMLDocumentScannerImpl and its drivers, XMLDocumentFragmentScannerImpl, XMLNSDocumentScannerImpl,
// XMLScanner, XMLEntityScanner and the part of XMLDTDScannerImpl an internal subset without
// declarations reaches) closely enough that it fails where they fail, with their message. It reads
// the whole document as code points; Xerces reads UTF-16 code units, which only matters for the
// length of a name (counted in code units here too).

namespace QtRocket
{

namespace
{

/// A closed range of code points: the first and the last.
using CharRange = std::pair<char32_t, char32_t>;

// XMLChar's name tables, which the JDK applies to XML 1.0 documents (the letters, digits,
// combining characters and extenders of XML 1.0's Appendix B; the BMP only).
constexpr std::array<CharRange, 206> kNameStart10{
    {{0x003A, 0x003A}, {0x0041, 0x005A}, {0x005F, 0x005F}, {0x0061, 0x007A}, {0x00C0, 0x00D6},
     {0x00D8, 0x00F6}, {0x00F8, 0x0131}, {0x0134, 0x013E}, {0x0141, 0x0148}, {0x014A, 0x017E},
     {0x0180, 0x01C3}, {0x01CD, 0x01F0}, {0x01F4, 0x01F5}, {0x01FA, 0x0217}, {0x0250, 0x02A8},
     {0x02BB, 0x02C1}, {0x0386, 0x0386}, {0x0388, 0x038A}, {0x038C, 0x038C}, {0x038E, 0x03A1},
     {0x03A3, 0x03CE}, {0x03D0, 0x03D6}, {0x03DA, 0x03DA}, {0x03DC, 0x03DC}, {0x03DE, 0x03DE},
     {0x03E0, 0x03E0}, {0x03E2, 0x03F3}, {0x0401, 0x040C}, {0x040E, 0x044F}, {0x0451, 0x045C},
     {0x045E, 0x0481}, {0x0490, 0x04C4}, {0x04C7, 0x04C8}, {0x04CB, 0x04CC}, {0x04D0, 0x04EB},
     {0x04EE, 0x04F5}, {0x04F8, 0x04F9}, {0x0531, 0x0556}, {0x0559, 0x0559}, {0x0561, 0x0586},
     {0x05D0, 0x05EA}, {0x05F0, 0x05F2}, {0x0621, 0x063A}, {0x0641, 0x064A}, {0x0671, 0x06B7},
     {0x06BA, 0x06BE}, {0x06C0, 0x06CE}, {0x06D0, 0x06D3}, {0x06D5, 0x06D5}, {0x06E5, 0x06E6},
     {0x0905, 0x0939}, {0x093D, 0x093D}, {0x0958, 0x0961}, {0x0985, 0x098C}, {0x098F, 0x0990},
     {0x0993, 0x09A8}, {0x09AA, 0x09B0}, {0x09B2, 0x09B2}, {0x09B6, 0x09B9}, {0x09DC, 0x09DD},
     {0x09DF, 0x09E1}, {0x09F0, 0x09F1}, {0x0A05, 0x0A0A}, {0x0A0F, 0x0A10}, {0x0A13, 0x0A28},
     {0x0A2A, 0x0A30}, {0x0A32, 0x0A33}, {0x0A35, 0x0A36}, {0x0A38, 0x0A39}, {0x0A59, 0x0A5C},
     {0x0A5E, 0x0A5E}, {0x0A72, 0x0A74}, {0x0A85, 0x0A8B}, {0x0A8D, 0x0A8D}, {0x0A8F, 0x0A91},
     {0x0A93, 0x0AA8}, {0x0AAA, 0x0AB0}, {0x0AB2, 0x0AB3}, {0x0AB5, 0x0AB9}, {0x0ABD, 0x0ABD},
     {0x0AE0, 0x0AE0}, {0x0B05, 0x0B0C}, {0x0B0F, 0x0B10}, {0x0B13, 0x0B28}, {0x0B2A, 0x0B30},
     {0x0B32, 0x0B33}, {0x0B36, 0x0B39}, {0x0B3D, 0x0B3D}, {0x0B5C, 0x0B5D}, {0x0B5F, 0x0B61},
     {0x0B85, 0x0B8A}, {0x0B8E, 0x0B90}, {0x0B92, 0x0B95}, {0x0B99, 0x0B9A}, {0x0B9C, 0x0B9C},
     {0x0B9E, 0x0B9F}, {0x0BA3, 0x0BA4}, {0x0BA8, 0x0BAA}, {0x0BAE, 0x0BB5}, {0x0BB7, 0x0BB9},
     {0x0C05, 0x0C0C}, {0x0C0E, 0x0C10}, {0x0C12, 0x0C28}, {0x0C2A, 0x0C33}, {0x0C35, 0x0C39},
     {0x0C60, 0x0C61}, {0x0C85, 0x0C8C}, {0x0C8E, 0x0C90}, {0x0C92, 0x0CA8}, {0x0CAA, 0x0CB3},
     {0x0CB5, 0x0CB9}, {0x0CDE, 0x0CDE}, {0x0CE0, 0x0CE1}, {0x0D05, 0x0D0C}, {0x0D0E, 0x0D10},
     {0x0D12, 0x0D28}, {0x0D2A, 0x0D39}, {0x0D60, 0x0D61}, {0x0E01, 0x0E2E}, {0x0E30, 0x0E30},
     {0x0E32, 0x0E33}, {0x0E40, 0x0E45}, {0x0E81, 0x0E82}, {0x0E84, 0x0E84}, {0x0E87, 0x0E88},
     {0x0E8A, 0x0E8A}, {0x0E8D, 0x0E8D}, {0x0E94, 0x0E97}, {0x0E99, 0x0E9F}, {0x0EA1, 0x0EA3},
     {0x0EA5, 0x0EA5}, {0x0EA7, 0x0EA7}, {0x0EAA, 0x0EAB}, {0x0EAD, 0x0EAE}, {0x0EB0, 0x0EB0},
     {0x0EB2, 0x0EB3}, {0x0EBD, 0x0EBD}, {0x0EC0, 0x0EC4}, {0x0F40, 0x0F47}, {0x0F49, 0x0F69},
     {0x10A0, 0x10C5}, {0x10D0, 0x10F6}, {0x1100, 0x1100}, {0x1102, 0x1103}, {0x1105, 0x1107},
     {0x1109, 0x1109}, {0x110B, 0x110C}, {0x110E, 0x1112}, {0x113C, 0x113C}, {0x113E, 0x113E},
     {0x1140, 0x1140}, {0x114C, 0x114C}, {0x114E, 0x114E}, {0x1150, 0x1150}, {0x1154, 0x1155},
     {0x1159, 0x1159}, {0x115F, 0x1161}, {0x1163, 0x1163}, {0x1165, 0x1165}, {0x1167, 0x1167},
     {0x1169, 0x1169}, {0x116D, 0x116E}, {0x1172, 0x1173}, {0x1175, 0x1175}, {0x119E, 0x119E},
     {0x11A8, 0x11A8}, {0x11AB, 0x11AB}, {0x11AE, 0x11AF}, {0x11B7, 0x11B8}, {0x11BA, 0x11BA},
     {0x11BC, 0x11C2}, {0x11EB, 0x11EB}, {0x11F0, 0x11F0}, {0x11F9, 0x11F9}, {0x1E00, 0x1E9B},
     {0x1EA0, 0x1EF9}, {0x1F00, 0x1F15}, {0x1F18, 0x1F1D}, {0x1F20, 0x1F45}, {0x1F48, 0x1F4D},
     {0x1F50, 0x1F57}, {0x1F59, 0x1F59}, {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D}, {0x1F5F, 0x1F7D},
     {0x1F80, 0x1FB4}, {0x1FB6, 0x1FBC}, {0x1FBE, 0x1FBE}, {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC},
     {0x1FD0, 0x1FD3}, {0x1FD6, 0x1FDB}, {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4}, {0x1FF6, 0x1FFC},
     {0x2126, 0x2126}, {0x212A, 0x212B}, {0x212E, 0x212E}, {0x2180, 0x2182}, {0x3007, 0x3007},
     {0x3021, 0x3029}, {0x3041, 0x3094}, {0x30A1, 0x30FA}, {0x3105, 0x312C}, {0x4E00, 0x9FA5},
     {0xAC00, 0xD7A3}}};
constexpr std::array<CharRange, 287> kNameChar10{
    {{0x002D, 0x002E}, {0x0030, 0x003A}, {0x0041, 0x005A}, {0x005F, 0x005F}, {0x0061, 0x007A},
     {0x00B7, 0x00B7}, {0x00C0, 0x00D6}, {0x00D8, 0x00F6}, {0x00F8, 0x0131}, {0x0134, 0x013E},
     {0x0141, 0x0148}, {0x014A, 0x017E}, {0x0180, 0x01C3}, {0x01CD, 0x01F0}, {0x01F4, 0x01F5},
     {0x01FA, 0x0217}, {0x0250, 0x02A8}, {0x02BB, 0x02C1}, {0x02D0, 0x02D1}, {0x0300, 0x0345},
     {0x0360, 0x0361}, {0x0386, 0x038A}, {0x038C, 0x038C}, {0x038E, 0x03A1}, {0x03A3, 0x03CE},
     {0x03D0, 0x03D6}, {0x03DA, 0x03DA}, {0x03DC, 0x03DC}, {0x03DE, 0x03DE}, {0x03E0, 0x03E0},
     {0x03E2, 0x03F3}, {0x0401, 0x040C}, {0x040E, 0x044F}, {0x0451, 0x045C}, {0x045E, 0x0481},
     {0x0483, 0x0486}, {0x0490, 0x04C4}, {0x04C7, 0x04C8}, {0x04CB, 0x04CC}, {0x04D0, 0x04EB},
     {0x04EE, 0x04F5}, {0x04F8, 0x04F9}, {0x0531, 0x0556}, {0x0559, 0x0559}, {0x0561, 0x0586},
     {0x0591, 0x05A1}, {0x05A3, 0x05B9}, {0x05BB, 0x05BD}, {0x05BF, 0x05BF}, {0x05C1, 0x05C2},
     {0x05C4, 0x05C4}, {0x05D0, 0x05EA}, {0x05F0, 0x05F2}, {0x0621, 0x063A}, {0x0640, 0x0652},
     {0x0660, 0x0669}, {0x0670, 0x06B7}, {0x06BA, 0x06BE}, {0x06C0, 0x06CE}, {0x06D0, 0x06D3},
     {0x06D5, 0x06E8}, {0x06EA, 0x06ED}, {0x06F0, 0x06F9}, {0x0901, 0x0903}, {0x0905, 0x0939},
     {0x093C, 0x094D}, {0x0951, 0x0954}, {0x0958, 0x0963}, {0x0966, 0x096F}, {0x0981, 0x0983},
     {0x0985, 0x098C}, {0x098F, 0x0990}, {0x0993, 0x09A8}, {0x09AA, 0x09B0}, {0x09B2, 0x09B2},
     {0x09B6, 0x09B9}, {0x09BC, 0x09BC}, {0x09BE, 0x09C4}, {0x09C7, 0x09C8}, {0x09CB, 0x09CD},
     {0x09D7, 0x09D7}, {0x09DC, 0x09DD}, {0x09DF, 0x09E3}, {0x09E6, 0x09F1}, {0x0A02, 0x0A02},
     {0x0A05, 0x0A0A}, {0x0A0F, 0x0A10}, {0x0A13, 0x0A28}, {0x0A2A, 0x0A30}, {0x0A32, 0x0A33},
     {0x0A35, 0x0A36}, {0x0A38, 0x0A39}, {0x0A3C, 0x0A3C}, {0x0A3E, 0x0A42}, {0x0A47, 0x0A48},
     {0x0A4B, 0x0A4D}, {0x0A59, 0x0A5C}, {0x0A5E, 0x0A5E}, {0x0A66, 0x0A74}, {0x0A81, 0x0A83},
     {0x0A85, 0x0A8B}, {0x0A8D, 0x0A8D}, {0x0A8F, 0x0A91}, {0x0A93, 0x0AA8}, {0x0AAA, 0x0AB0},
     {0x0AB2, 0x0AB3}, {0x0AB5, 0x0AB9}, {0x0ABC, 0x0AC5}, {0x0AC7, 0x0AC9}, {0x0ACB, 0x0ACD},
     {0x0AE0, 0x0AE0}, {0x0AE6, 0x0AEF}, {0x0B01, 0x0B03}, {0x0B05, 0x0B0C}, {0x0B0F, 0x0B10},
     {0x0B13, 0x0B28}, {0x0B2A, 0x0B30}, {0x0B32, 0x0B33}, {0x0B36, 0x0B39}, {0x0B3C, 0x0B43},
     {0x0B47, 0x0B48}, {0x0B4B, 0x0B4D}, {0x0B56, 0x0B57}, {0x0B5C, 0x0B5D}, {0x0B5F, 0x0B61},
     {0x0B66, 0x0B6F}, {0x0B82, 0x0B83}, {0x0B85, 0x0B8A}, {0x0B8E, 0x0B90}, {0x0B92, 0x0B95},
     {0x0B99, 0x0B9A}, {0x0B9C, 0x0B9C}, {0x0B9E, 0x0B9F}, {0x0BA3, 0x0BA4}, {0x0BA8, 0x0BAA},
     {0x0BAE, 0x0BB5}, {0x0BB7, 0x0BB9}, {0x0BBE, 0x0BC2}, {0x0BC6, 0x0BC8}, {0x0BCA, 0x0BCD},
     {0x0BD7, 0x0BD7}, {0x0BE7, 0x0BEF}, {0x0C01, 0x0C03}, {0x0C05, 0x0C0C}, {0x0C0E, 0x0C10},
     {0x0C12, 0x0C28}, {0x0C2A, 0x0C33}, {0x0C35, 0x0C39}, {0x0C3E, 0x0C44}, {0x0C46, 0x0C48},
     {0x0C4A, 0x0C4D}, {0x0C55, 0x0C56}, {0x0C60, 0x0C61}, {0x0C66, 0x0C6F}, {0x0C82, 0x0C83},
     {0x0C85, 0x0C8C}, {0x0C8E, 0x0C90}, {0x0C92, 0x0CA8}, {0x0CAA, 0x0CB3}, {0x0CB5, 0x0CB9},
     {0x0CBE, 0x0CC4}, {0x0CC6, 0x0CC8}, {0x0CCA, 0x0CCD}, {0x0CD5, 0x0CD6}, {0x0CDE, 0x0CDE},
     {0x0CE0, 0x0CE1}, {0x0CE6, 0x0CEF}, {0x0D02, 0x0D03}, {0x0D05, 0x0D0C}, {0x0D0E, 0x0D10},
     {0x0D12, 0x0D28}, {0x0D2A, 0x0D39}, {0x0D3E, 0x0D43}, {0x0D46, 0x0D48}, {0x0D4A, 0x0D4D},
     {0x0D57, 0x0D57}, {0x0D60, 0x0D61}, {0x0D66, 0x0D6F}, {0x0E01, 0x0E2E}, {0x0E30, 0x0E3A},
     {0x0E40, 0x0E4E}, {0x0E50, 0x0E59}, {0x0E81, 0x0E82}, {0x0E84, 0x0E84}, {0x0E87, 0x0E88},
     {0x0E8A, 0x0E8A}, {0x0E8D, 0x0E8D}, {0x0E94, 0x0E97}, {0x0E99, 0x0E9F}, {0x0EA1, 0x0EA3},
     {0x0EA5, 0x0EA5}, {0x0EA7, 0x0EA7}, {0x0EAA, 0x0EAB}, {0x0EAD, 0x0EAE}, {0x0EB0, 0x0EB9},
     {0x0EBB, 0x0EBD}, {0x0EC0, 0x0EC4}, {0x0EC6, 0x0EC6}, {0x0EC8, 0x0ECD}, {0x0ED0, 0x0ED9},
     {0x0F18, 0x0F19}, {0x0F20, 0x0F29}, {0x0F35, 0x0F35}, {0x0F37, 0x0F37}, {0x0F39, 0x0F39},
     {0x0F3E, 0x0F47}, {0x0F49, 0x0F69}, {0x0F71, 0x0F84}, {0x0F86, 0x0F8B}, {0x0F90, 0x0F95},
     {0x0F97, 0x0F97}, {0x0F99, 0x0FAD}, {0x0FB1, 0x0FB7}, {0x0FB9, 0x0FB9}, {0x10A0, 0x10C5},
     {0x10D0, 0x10F6}, {0x1100, 0x1100}, {0x1102, 0x1103}, {0x1105, 0x1107}, {0x1109, 0x1109},
     {0x110B, 0x110C}, {0x110E, 0x1112}, {0x113C, 0x113C}, {0x113E, 0x113E}, {0x1140, 0x1140},
     {0x114C, 0x114C}, {0x114E, 0x114E}, {0x1150, 0x1150}, {0x1154, 0x1155}, {0x1159, 0x1159},
     {0x115F, 0x1161}, {0x1163, 0x1163}, {0x1165, 0x1165}, {0x1167, 0x1167}, {0x1169, 0x1169},
     {0x116D, 0x116E}, {0x1172, 0x1173}, {0x1175, 0x1175}, {0x119E, 0x119E}, {0x11A8, 0x11A8},
     {0x11AB, 0x11AB}, {0x11AE, 0x11AF}, {0x11B7, 0x11B8}, {0x11BA, 0x11BA}, {0x11BC, 0x11C2},
     {0x11EB, 0x11EB}, {0x11F0, 0x11F0}, {0x11F9, 0x11F9}, {0x1E00, 0x1E9B}, {0x1EA0, 0x1EF9},
     {0x1F00, 0x1F15}, {0x1F18, 0x1F1D}, {0x1F20, 0x1F45}, {0x1F48, 0x1F4D}, {0x1F50, 0x1F57},
     {0x1F59, 0x1F59}, {0x1F5B, 0x1F5B}, {0x1F5D, 0x1F5D}, {0x1F5F, 0x1F7D}, {0x1F80, 0x1FB4},
     {0x1FB6, 0x1FBC}, {0x1FBE, 0x1FBE}, {0x1FC2, 0x1FC4}, {0x1FC6, 0x1FCC}, {0x1FD0, 0x1FD3},
     {0x1FD6, 0x1FDB}, {0x1FE0, 0x1FEC}, {0x1FF2, 0x1FF4}, {0x1FF6, 0x1FFC}, {0x20D0, 0x20DC},
     {0x20E1, 0x20E1}, {0x2126, 0x2126}, {0x212A, 0x212B}, {0x212E, 0x212E}, {0x2180, 0x2182},
     {0x3005, 0x3005}, {0x3007, 0x3007}, {0x3021, 0x302F}, {0x3031, 0x3035}, {0x3041, 0x3094},
     {0x3099, 0x309A}, {0x309D, 0x309E}, {0x30A1, 0x30FA}, {0x30FC, 0x30FE}, {0x3105, 0x312C},
     {0x4E00, 0x9FA5}, {0xAC00, 0xD7A3}}};

// XML11Char's name tables for XML 1.1 documents.
constexpr std::array<CharRange, 16> kNameStart11{{
    {0x003A, 0x003A},
    {0x0041, 0x005A},
    {0x005F, 0x005F},
    {0x0061, 0x007A},
    {0x00C0, 0x00D6},
    {0x00D8, 0x00F6},
    {0x00F8, 0x02FF},
    {0x0370, 0x037D},
    {0x037F, 0x1FFF},
    {0x200C, 0x200D},
    {0x2070, 0x218F},
    {0x2C00, 0x2FEF},
    {0x3001, 0xD7FF},
    {0xF900, 0xFDCF},
    {0xFDF0, 0xFFFD},
    {0x10000, 0xEFFFF},
}};
constexpr std::array<CharRange, 18> kNameChar11{{
    {0x002D, 0x002E},
    {0x0030, 0x003A},
    {0x0041, 0x005A},
    {0x005F, 0x005F},
    {0x0061, 0x007A},
    {0x00B7, 0x00B7},
    {0x00C0, 0x00D6},
    {0x00D8, 0x00F6},
    {0x00F8, 0x037D},
    {0x037F, 0x1FFF},
    {0x200C, 0x200D},
    {0x203F, 0x2040},
    {0x2070, 0x218F},
    {0x2C00, 0x2FEF},
    {0x3001, 0xD7FF},
    {0xF900, 0xFDCF},
    {0xFDF0, 0xFFFD},
    {0x10000, 0xEFFFF},
}};

/// Where the document ends; no character has this value.
constexpr char32_t kEof = 0x110000;

/// The JDK's jdk.xml.maxXMLNameLimit: the longest name (or prefix or local part), in UTF-16 units.
constexpr std::size_t kMaxNameLength = 1000;
/// The JDK's jdk.xml.elementAttributeLimit.
constexpr std::size_t kMaxAttributes = 10000;
/// XMLAttributesImpl.SIZE_LIMIT: up to this many attributes, duplicates are looked for pairwise.
constexpr std::size_t kSmallAttributeList = 20;

constexpr std::u32string_view kXmlUri   = U"http://www.w3.org/XML/1998/namespace";
constexpr std::u32string_view kXmlnsUri = U"http://www.w3.org/2000/xmlns/";

// Xerces's messages (XMLMessages.properties) that take no argument.
constexpr std::string_view kPrematureEof = "Premature end of file.";
constexpr std::string_view kMarkupEntityMismatch =
    "XML document structures must start and end within the same entity.";
constexpr std::string_view kContentInProlog   = "Content is not allowed in prolog.";
constexpr std::string_view kReferenceInProlog = "Reference is not allowed in prolog.";
constexpr std::string_view kContentInTrailing = "Content is not allowed in trailing section.";
constexpr std::string_view kMarkupInProlog =
    "The markup in the document preceding the root element must be well-formed.";
constexpr std::string_view kMarkupInMisc =
    "The markup in the document following the root element must be well-formed.";
constexpr std::string_view kMarkupInContent =
    "The content of elements must consist of well-formed character data or markup.";
constexpr std::string_view kMarkupInDtd =
    "The markup declarations contained or pointed to by the document type declaration must be "
    "well-formed.";
constexpr std::string_view kInvalidCommentStart = "Comment must start with \"<!--\".";
constexpr std::string_view kDashDashInComment =
    "The string \"--\" is not permitted within comments.";
constexpr std::string_view kCdEndInContent =
    "The character sequence \"]]>\" must not appear in content unless used to mark the end of a "
    "CDATA section.";
constexpr std::string_view kReservedPiTarget =
    "The processing instruction target matching \"[xX][mM][lL]\" is not allowed.";
constexpr std::string_view kPiTargetRequired =
    "The processing instruction must begin with the name of the target.";
constexpr std::string_view kSpaceRequiredInPi =
    "White space is required between the processing instruction target and data.";
constexpr std::string_view kNameRequiredInReference =
    "The entity name must immediately follow the '&' in the entity reference.";
constexpr std::string_view kCantBindXml =
    "The prefix \"xml\" cannot be bound to any namespace other than its usual namespace; neither "
    "can the namespace for \"xml\" be bound to any prefix other than \"xml\".";
constexpr std::string_view kCantBindXmlns =
    "The prefix \"xmlns\" cannot be bound to any namespace explicitly; neither can the namespace "
    "for \"xmlns\" be bound to any prefix explicitly.";
constexpr std::string_view kVersionRequired     = "The version is required in the XML declaration.";
constexpr std::string_view kXmlDeclUnterminated = "The XML declaration must end with \"?>\".";

/// Ends the scan: the first fatal error, as Xerces throws it. Not a std::exception: scan() catches
/// it and it never leaves this file.
struct Stop
{
    Error error;
};

[[noreturn]] void stop(ErrorCode code, std::string message)
{
    // NOLINTNEXTLINE(bugprone-std-exception-baseclass)
    throw Stop{fail(code, std::move(message)).error()};
}

[[noreturn]] void fatal(std::string message)
{
    stop(ErrorCode::PARSE, std::move(message));
}

[[nodiscard]] bool inRanges(std::span<const CharRange> ranges, char32_t c) noexcept
{
    const auto it = std::ranges::lower_bound(ranges, c, std::ranges::less{}, &CharRange::second);
    return it != ranges.end() && it->first <= c;
}

[[nodiscard]] bool isSpace(char32_t c) noexcept
{
    return c == U' ' || c == U'\t' || c == U'\n' || c == U'\r';
}

[[nodiscard]] std::string utf8(std::u32string_view text)
{
    return Strings::fromCodePoints(text);
}

/// Integer.toString(c, 16), as the invalid-character messages show a character.
[[nodiscard]] std::string hex(char32_t c)
{
    return std::format("{:x}", static_cast<std::uint32_t>(c));
}

/// @p n with a comma between groups of three digits, as MessageFormat formats a number (in an
/// English locale).
[[nodiscard]] std::string grouped(std::size_t n)
{
    std::string digits = std::to_string(n);
    for (std::size_t i = digits.size(); i > 3; i -= 3)
    {
        digits.insert(i - 3, 1, ',');
    }
    return digits;
}

/// The length of @p text in UTF-16 code units.
[[nodiscard]] std::size_t utf16Length(std::u32string_view text) noexcept
{
    return text.size() + static_cast<std::size_t>(
                             std::ranges::count_if(text, [](char32_t c) { return c > 0xFFFF; }));
}

/// The character a predefined entity stands for (amp, lt, gt, quot, apos).
[[nodiscard]] std::optional<char32_t> predefinedEntity(std::u32string_view name) noexcept
{
    if (name == U"amp")
    {
        return U'&';
    }
    if (name == U"lt")
    {
        return U'<';
    }
    if (name == U"gt")
    {
        return U'>';
    }
    if (name == U"quot")
    {
        return U'"';
    }
    if (name == U"apos")
    {
        return U'\'';
    }
    return std::nullopt;
}

/// A qualified name as Xerces's QName holds it.
struct QName
{
    std::u32string rawname;
    /// The part before the colon; none when the name has no colon after its first character.
    std::optional<std::u32string> prefix;
    std::u32string                localpart;
    std::optional<std::u32string> uri;
};

/// QName.toString(), which one message shows.
[[nodiscard]] std::string toString(const QName& name)
{
    std::string text;
    if (name.prefix.has_value())
    {
        text += std::format("prefix=\"{}\",", utf8(*name.prefix));
    }
    text += std::format(R"(localpart="{}",rawname="{}")", utf8(name.localpart), utf8(name.rawname));
    if (name.uri.has_value())
    {
        text += std::format(",uri=\"{}\"", utf8(*name.uri));
    }
    return text;
}

class Scanner
{
public:
    explicit Scanner(std::u32string chars) : m_chars(std::move(chars)) { }

    [[nodiscard]] XmlScanner::Report run()
    {
        XmlScanner::Report report;
        try
        {
            m_xml11      = detectVersion();
            report.xml11 = m_xml11;
            normalizeLineEnds();
            scanXmlDecl();
            scanProlog();
            if (!scanStartElement())
            {
                scanContent();
            }
            scanTrailingMisc();
        }
        catch (Stop& stopped)
        {
            report.error = std::move(stopped.error);
        }
        report.elementEvents = m_events;
        return report;
    }

private:
    // ---- characters -------------------------------------------------------------------------

    /// Whether @p c may stand in the document as itself (Xerces's isInvalidLiteral(), negated).
    [[nodiscard]] bool isValidLiteral(char32_t c) const noexcept
    {
        if (m_xml11)
        {
            // XML11Char.isXML11ValidLiteral(); U+0085 was a line end and is gone by now.
            return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0x7E) ||
                   (c >= 0xA0 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) ||
                   (c >= 0x10000 && c <= 0x10FFFF);
        }
        return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) ||
               (c >= 0xE000 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0x10FFFF);
    }

    [[nodiscard]] bool isInvalidLiteral(char32_t c) const noexcept { return !isValidLiteral(c); }

    /// Whether a character reference may stand for @p value (Xerces's isInvalid()).
    [[nodiscard]] bool isValidReference(std::uint32_t value) const noexcept
    {
        if (m_xml11)
        {
            return (value >= 0x1 && value <= 0xD7FF) || (value >= 0xE000 && value <= 0xFFFD) ||
                   (value >= 0x10000 && value <= 0x10FFFF);
        }
        return value == 0x9 || value == 0xA || value == 0xD || (value >= 0x20 && value <= 0xD7FF) ||
               (value >= 0xE000 && value <= 0xFFFD) || (value >= 0x10000 && value <= 0x10FFFF);
    }

    [[nodiscard]] bool isNameStart(char32_t c) const noexcept
    {
        return m_xml11 ? inRanges(kNameStart11, c) : inRanges(kNameStart10, c);
    }

    [[nodiscard]] bool isNameChar(char32_t c) const noexcept
    {
        return m_xml11 ? inRanges(kNameChar11, c) : inRanges(kNameChar10, c);
    }

    [[nodiscard]] bool isNcNameStart(char32_t c) const noexcept
    {
        return c != U':' && isNameStart(c);
    }

    // ---- the entity scanner --------------------------------------------------------------------

    [[nodiscard]] bool atEnd() const noexcept { return m_position >= m_chars.size(); }

    /// The end of the document where more is needed: XMLDocumentFragmentScannerImpl.endEntity()
    /// fails when markup is open, and otherwise the driver reports the premature end.
    [[noreturn]] void endOfDocument() const
    {
        if (m_markupDepth != 0 && !m_inInternalSubset)
        {
            fatal(std::string(kMarkupEntityMismatch));
        }
        fatal(std::string(kPrematureEof));
    }

    /// The current character (peekChar()); the end of the document is an error.
    [[nodiscard]] char32_t peek() const
    {
        if (atEnd())
        {
            endOfDocument();
        }
        return m_chars[m_position];
    }

    /// The character @p offset places ahead, where Xerces looks ahead for a delimiter.
    [[nodiscard]] char32_t peekAhead(std::size_t offset) const
    {
        if (m_position + offset >= m_chars.size())
        {
            endOfDocument();
        }
        return m_chars[m_position + offset];
    }

    /// The character up to which a name reaches, kEof at the end (scanName() stops there without
    /// an error; whatever reads next reports it).
    [[nodiscard]] char32_t current() const noexcept { return atEnd() ? kEof : m_chars[m_position]; }

    char32_t scanChar()
    {
        const char32_t c = peek();
        m_position++;
        return c;
    }

    bool skipChar(char32_t c)
    {
        if (peek() != c)
        {
            return false;
        }
        m_position++;
        return true;
    }

    /// skipString(): false, not an error, when the document ends first.
    bool skipString(std::u32string_view text) noexcept
    {
        if (!std::u32string_view(m_chars).substr(m_position).starts_with(text))
        {
            return false;
        }
        m_position += text.size();
        return true;
    }

    bool skipSpaces()
    {
        bool skipped = false;
        while (isSpace(peek()))
        {
            m_position++;
            skipped = true;
        }
        return skipped;
    }

    [[noreturn]] static void nameTooLong(std::u32string_view name)
    {
        fatal(
            std::format("JAXP00010005: The length of entity \"[xml]\" is \"{}\" that exceeds the "
                        "\"{}\" limit set by \"FEATURE_SECURE_PROCESSING\".",
                        grouped(utf16Length(name)), grouped(kMaxNameLength)));
    }

    static void checkNameLength(std::u32string_view name)
    {
        if (utf16Length(name) > kMaxNameLength)
        {
            nameTooLong(name);
        }
    }

    /// A Name (scanName()), empty when the current character cannot start one.
    [[nodiscard]] std::u32string scanName()
    {
        if (!isNameStart(peek()))
        {
            return {};
        }
        const std::size_t start = m_position++;
        while (isNameChar(current()))
        {
            m_position++;
        }
        std::u32string name = m_chars.substr(start, m_position - start);
        checkNameLength(name);
        return name;
    }

    /// A QName (scanQName()); the current character starts a name. A colon that is not the first
    /// character separates the prefix, and a second colon ends the name. XML 1.1's scanner
    /// refuses a name that starts with a colon and reads nothing: the caller then goes on with
    /// @p stale, the QName it read last (Xerces reuses the object), and fails.
    [[nodiscard]] QName scanQName(const QName& stale)
    {
        if (m_xml11 && peek() == U':')
        {
            return stale;
        }
        const std::size_t          start = m_position++;
        std::optional<std::size_t> colon;
        while (isNameChar(current()))
        {
            if (current() == U':')
            {
                if (colon.has_value())
                {
                    break;
                }
                colon = m_position;
            }
            m_position++;
        }
        QName name;
        name.rawname = m_chars.substr(start, m_position - start);
        if (!colon.has_value())
        {
            checkNameLength(name.rawname);
            name.localpart = name.rawname;
            return name;
        }
        name.prefix = m_chars.substr(start, *colon - start);
        checkNameLength(*name.prefix);
        const std::size_t localStart = *colon + 1;
        // Deviation: when the document ends right after the colon, Xerces checks a stale
        // character of its buffer instead, and may report the end of the document.
        if (!isNcNameStart(localStart < m_chars.size() ? m_chars[localStart] : kEof))
        {
            fatal(
                std::format("Element or attribute \"{}\" do not match QName production: "
                            "QName::=(NCName:)?NCName.",
                            utf8(name.rawname)));
        }
        name.localpart = m_chars.substr(localStart, m_position - localStart);
        checkNameLength(name.localpart);
        return name;
    }

    // ---- the XML declaration -------------------------------------------------------------------

    /// XMLVersionDetector.determineDocVersion(): whether the document is XML 1.1, from the first
    /// characters of the version in the XML declaration.
    [[nodiscard]] bool detectVersion()
    {
        std::size_t position = 0;
        const auto  at       = [this, &position] -> char32_t {
            if (position >= m_chars.size())
            {
                fatal(std::string(kPrematureEof));
            }
            return m_chars[position];
        };
        const auto skipDeclSpaces = [&] {
            bool skipped = false;
            while (isSpace(at()))
            {
                position++;
                skipped = true;
            }
            return skipped;
        };
        const auto skipString = [this, &position](std::u32string_view text) {
            if (!std::u32string_view(m_chars).substr(position).starts_with(text))
            {
                return false;
            }
            position += text.size();
            return true;
        };

        if (!skipString(U"<?xml") || !skipDeclSpaces() || !skipString(U"version"))
        {
            return false;
        }
        skipDeclSpaces();
        if (at() != U'=')
        {
            return false;
        }
        position++;
        skipDeclSpaces();
        // scanChar(): the quote, whatever it is, three characters and one more, with "\r\n" and
        // "\r" read as "\n"
        const auto scanChar = [&] {
            char32_t c = at();
            position++;
            if (c == U'\r')
            {
                if (position < m_chars.size() && m_chars[position] == U'\n')
                {
                    position++;
                }
                c = U'\n';
            }
            return c;
        };
        std::u32string rewritten = U"<?xml version=";
        for (int i = 0; i < 5; i++)
        {
            rewritten += scanChar();
        }
        // XMLVersionDetector.fixupCurrentEntity(): what was read gives way to the canonical
        // "<?xml version=" and the five characters, and white space pads out the rest, so that
        // white space around "version" and "=" moves into the value.
        rewritten.append(position - rewritten.size(), U' ');
        m_chars.replace(0, position, rewritten);
        return rewritten.substr(15, 3) == U"1.1";
    }

    /// Line ends become "\n" as the entity scanner reads them: "\r\n" and "\r", and in XML 1.1
    /// also "\r\u0085", U+0085 and U+2028.
    void normalizeLineEnds()
    {
        std::u32string normalized;
        normalized.reserve(m_chars.size());
        for (std::size_t i = 0; i < m_chars.size(); i++)
        {
            const char32_t c = m_chars[i];
            if (c == U'\r')
            {
                if (i + 1 < m_chars.size() &&
                    (m_chars[i + 1] == U'\n' || (m_xml11 && m_chars[i + 1] == 0x85)))
                {
                    i++;
                }
                normalized += U'\n';
            }
            else if (m_xml11 && (c == 0x85 || c == 0x2028))
            {
                normalized += U'\n';
            }
            else
            {
                normalized += c;
            }
        }
        m_chars = std::move(normalized);
    }

    /// XMLDeclDriver: the XML declaration, "<?xml" and white space at the very start; anything
    /// else there ("<?xml?>", "<?xml-stylesheet ...?>") is left to the prolog as a processing
    /// instruction.
    void scanXmlDecl()
    {
        if (!std::u32string_view(m_chars).starts_with(U"<?xml") || m_chars.size() == 5 ||
            !isSpace(m_chars[5]))
        {
            return;
        }
        m_position += 5;
        m_markupDepth++;
        scanXmlDeclPseudoAttributes();
        m_markupDepth--;
    }

    /// Where the XML declaration's pseudo attributes stand: the one expected next.
    enum class DeclState
    {
        VERSION,
        ENCODING,
        STANDALONE,
        DONE,
    };

    /// scanXMLDeclOrTextDecl() for the XML declaration.
    void scanXmlDeclPseudoAttributes()
    {
        DeclState      state     = DeclState::VERSION;
        bool           dataFound = false;
        bool           sawSpace  = skipSpaces();
        std::u32string value;
        while (peek() != U'?')
        {
            dataFound                   = true;
            const std::string_view name = scanPseudoAttribute(value);
            state                       = acceptPseudoAttribute(state, name, sawSpace, value);
            sawSpace                    = skipSpaces();
        }
        if (!dataFound)
        {
            fatal(std::string(kVersionRequired));
        }
        if (!skipChar(U'?') || !skipChar(U'>'))
        {
            fatal(std::string(kXmlDeclUnterminated));
        }
    }

    /// The state after the pseudo attribute @p name="@p value", read in @p state after white
    /// space when @p sawSpace (the switch of scanXMLDeclOrTextDecl()).
    [[nodiscard]] static DeclState acceptPseudoAttribute(DeclState state, std::string_view name,
                                                         bool sawSpace, std::u32string_view value)
    {
        switch (state)
        {
            case DeclState::VERSION:
                if (name != "version")
                {
                    fatal(std::string(kVersionRequired));
                }
                if (!sawSpace)
                {
                    fatal(
                        "White space is required before the version pseudo attribute in the XML "
                        "declaration.");
                }
                // The detector chose the scanner: "1.0" and "1.1" are the values it can see.
                if (value != U"1.0" && value != U"1.1")
                {
                    fatal(std::format(
                        R"(XML version "{}" is not supported, only XML 1.0 is supported.)",
                        utf8(value)));
                }
                return DeclState::ENCODING;
            case DeclState::ENCODING:
                if (name == "encoding")
                {
                    if (!sawSpace)
                    {
                        fatal(
                            "White space is required before the encoding pseudo attribute in "
                            "the XML declaration.");
                    }
                    // A character stream ignores the encoding, whatever it names.
                    return DeclState::STANDALONE;
                }
                if (name != "standalone")
                {
                    fatal("The encoding declaration is required in the text declaration.");
                }
                scanStandalone(sawSpace, value);
                return DeclState::DONE;
            case DeclState::STANDALONE:
                if (name != "standalone")
                {
                    fatal("The standalone name in XML declaration may be misspelled.");
                }
                scanStandalone(sawSpace, value);
                return DeclState::DONE;
            case DeclState::DONE:
                break;
        }
        fatal("No more pseudo attributes are allowed.");
    }

    static void scanStandalone(bool sawSpace, std::u32string_view value)
    {
        if (!sawSpace)
        {
            fatal(
                "White space is required before the encoding pseudo attribute in the XML "
                "declaration.");
        }
        if (value != U"yes" && value != U"no")
        {
            fatal(
                std::format("The standalone document declaration value must be \"yes\" or "
                            "\"no\", not \"{}\".",
                            utf8(value)));
        }
    }

    /// scanPseudoAttribute(): the name, and the value in @p value.
    [[nodiscard]] std::string_view scanPseudoAttribute(std::u32string& value)
    {
        std::string_view name;
        const char32_t   c = peek();
        if (c == U'v' && skipString(U"version"))
        {
            name = "version";
        }
        else if (c == U'e' && skipString(U"encoding"))
        {
            name = "encoding";
        }
        else if (c == U's' && skipString(U"standalone"))
        {
            name = "standalone";
        }
        else
        {
            fatal("A pseudo attribute name is expected.");
        }
        skipSpaces();
        if (!skipChar(U'='))
        {
            fatal(std::format("The ' = ' character must follow \"{}\" in the XML declaration.",
                              name));
        }
        skipSpaces();
        const char32_t quote = peek();
        if (quote != U'\'' && quote != U'"')
        {
            fatal(std::format(
                "The value following \"{}\" in the XML declaration must be a quoted string.",
                name));
        }
        m_position++;
        value.clear();
        for (char32_t v = peek(); v != quote; v = peek())
        {
            if (isInvalidLiteral(v))
            {
                fatal(std::format(
                    "An invalid XML character (Unicode: 0x{}) was found in the XML declaration.",
                    hex(v)));
            }
            value += v;
            m_position++;
        }
        m_position++;
        return name;
    }

    // ---- the prolog ----------------------------------------------------------------------------

    /// PrologDriver: up to the root element's name, its '<' read and counted as markup.
    void scanProlog()
    {
        while (true)
        {
            skipSpaces();
            if (skipChar(U'<'))
            {
                m_markupDepth++;
                if (isNameStart(peek()))
                {
                    return;
                }
                if (skipChar(U'!'))
                {
                    if (skipChar(U'-'))
                    {
                        if (!skipChar(U'-'))
                        {
                            fatal(std::string(kInvalidCommentStart));
                        }
                        scanComment();
                    }
                    else if (skipString(U"DOCTYPE"))
                    {
                        scanDoctype();
                    }
                    else
                    {
                        fatal(std::string(kMarkupInProlog));
                    }
                }
                else if (skipChar(U'?'))
                {
                    scanPi();
                }
                else
                {
                    fatal(std::string(kMarkupInProlog));
                }
            }
            else if (skipChar(U'&'))
            {
                fatal(std::string(kReferenceInProlog));
            }
            else
            {
                fatal(std::string(kContentInProlog));
            }
        }
    }

    /// A comment after "<!--" (scanComment()).
    void scanComment()
    {
        while (true)
        {
            const char32_t c = peek();
            if (c == U'-' && peekAhead(1) == U'-')
            {
                m_position += 2;
                break;
            }
            if (isInvalidLiteral(c))
            {
                fatal(std::format(
                    "An invalid XML character (Unicode: 0x{}) was found in the comment.", hex(c)));
            }
            m_position++;
        }
        if (!skipChar(U'>'))
        {
            fatal(std::string(kDashDashInComment));
        }
        m_markupDepth--;
    }

    /// A processing instruction after "<?" (scanPI()).
    void scanPi()
    {
        const std::u32string target = scanName();
        if (target.empty())
        {
            fatal(std::string(kPiTargetRequired));
        }
        scanPiData(target);
    }

    void scanPiData(std::u32string_view target)
    {
        const auto lower = [](char32_t c) {
            return c >= U'A' && c <= U'Z' ? c + (U'a' - U'A') : c;
        };
        if (target.size() == 3 && lower(target[0]) == U'x' && lower(target[1]) == U'm' &&
            lower(target[2]) == U'l')
        {
            fatal(std::string(kReservedPiTarget));
        }
        if (!skipSpaces())
        {
            if (!skipString(U"?>"))
            {
                fatal(std::string(kSpaceRequiredInPi));
            }
            m_markupDepth--;
            return;
        }
        while (true)
        {
            const char32_t c = peek();
            if (c == U'?' && peekAhead(1) == U'>')
            {
                m_position += 2;
                break;
            }
            if (isInvalidLiteral(c))
            {
                fatal(
                    std::format("An invalid XML character (Unicode: 0x{}) was found in the "
                                "processing instruction.",
                                hex(c)));
            }
            m_position++;
        }
        m_markupDepth--;
    }

    [[noreturn]] static void unsupportedDoctype(std::string_view what)
    {
        stop(ErrorCode::UNSUPPORTED_FORMAT,
             std::format("Unsupported document type declaration: {}", what));
    }

    /// A document type declaration after "<!DOCTYPE" (scanDoctypeDecl() and the internal subset).
    void scanDoctype()
    {
        if (m_seenDoctype)
        {
            fatal("Already seen doctype.");
        }
        m_seenDoctype = true;
        if (!skipSpaces())
        {
            fatal("White space is required after \"<!DOCTYPE\" in the document type declaration.");
        }
        const std::u32string name = scanName();
        if (name.empty())
        {
            fatal(
                "The root element type must appear after \"<!DOCTYPE\" in the document type "
                "declaration.");
        }
        const std::string unterminated = std::format(
            "The document type declaration for root element type \"{}\" must end with '>'.",
            utf8(name));
        if (skipSpaces())
        {
            if (skipString(U"PUBLIC") || skipString(U"SYSTEM"))
            {
                unsupportedDoctype("an external subset");
            }
            skipSpaces();
        }
        if (skipChar(U'['))
        {
            // Past '[' the end of the document is a premature end, whatever is open.
            m_inInternalSubset = true;
            scanInternalSubset();
            m_position++;  // the ']'
            skipSpaces();
        }
        if (!skipChar(U'>'))
        {
            fatal(unterminated);
        }
        m_inInternalSubset = false;
        m_markupDepth--;
    }

    /// The internal subset up to its ']' (XMLDTDScannerImpl.scanDecls()), as far as white space,
    /// comments and processing instructions go.
    void scanInternalSubset()
    {
        while (true)
        {
            skipSpaces();
            const char32_t c = peek();
            if (c == U']')
            {
                return;
            }
            if (c == U'%')
            {
                unsupportedDoctype("a parameter-entity reference");
            }
            if (!skipChar(U'<'))
            {
                fatal(std::string(kMarkupInDtd));
            }
            if (skipChar(U'?'))
            {
                m_markupDepth++;  // scanPi() closes it
                scanPi();
            }
            else if (skipChar(U'!'))
            {
                if (skipChar(U'-'))
                {
                    if (!skipChar(U'-'))
                    {
                        fatal(std::string(kMarkupInDtd));
                    }
                    m_markupDepth++;  // scanComment() closes it
                    scanComment();
                }
                else if (skipString(U"ELEMENT") || skipString(U"ATTLIST") ||
                         skipString(U"ENTITY") || skipString(U"NOTATION"))
                {
                    unsupportedDoctype("markup declarations");
                }
                else
                {
                    fatal(std::string(kMarkupInDtd));
                }
            }
            else
            {
                fatal(std::string(kMarkupInDtd));
            }
        }
    }

    // ---- elements ------------------------------------------------------------------------------

    struct Attribute
    {
        QName          name;
        std::u32string value;
    };

    /// The start tag whose name is the current character (scanStartElement()). Returns whether
    /// the element is empty, and makes the SAX calls for it.
    bool scanStartElement()
    {
        QName element     = scanQName(m_lastElementName);
        m_lastElementName = element;
        m_contexts.push_back(m_bindings.size());
        std::vector<Attribute> attributes;
        bool                   empty = false;
        while (true)
        {
            const bool     sawSpace = skipSpaces();
            const char32_t c        = peek();
            if (c == U'>')
            {
                m_position++;
                break;
            }
            if (c == U'/')
            {
                m_position++;
                if (!skipChar(U'>'))
                {
                    elementUnterminated(element);
                }
                empty = true;
                break;
            }
            if (!isNameStart(c) || !sawSpace)
            {
                elementUnterminated(element);
            }
            scanAttribute(element, attributes);
            if (attributes.size() > kMaxAttributes)
            {
                fatal(std::format(
                    "JAXP00010002:  Element \"{}\" has more than \"{}\" attributes, "
                    "\"{}\" is the limit imposed by the JDK.",
                    utf8(element.rawname), grouped(kMaxAttributes), grouped(kMaxAttributes)));
            }
        }
        if (m_seenDoctype)
        {
            bindNamesByValidator(element, attributes);
        }
        else
        {
            bindNames(element, attributes);
        }

        if (empty)
        {
            m_markupDepth--;
            popContext();
            m_events += 2;
            return true;
        }
        m_elements.push_back(std::move(element.rawname));
        m_events++;
        return false;
    }

    [[noreturn]] static void elementUnterminated(const QName& element)
    {
        fatal(std::format(
            "Element type \"{}\" must be followed by either attribute specifications, \">\" or "
            "\"/>\".",
            utf8(element.rawname)));
    }

    /// Binds the element and attribute prefixes once the start tag is read, and looks for
    /// attributes with the same expanded name.
    void bindNames(QName& element, std::vector<Attribute>& attributes) const
    {
        if (element.prefix == U"xmlns")
        {
            fatal(std::format(R"(Element "{}" cannot have "xmlns" as its prefix.)",
                              utf8(element.rawname)));
        }
        element.uri = uriOf(element.prefix.value_or(U""));
        if (element.prefix.has_value() && !element.uri.has_value())
        {
            fatal(std::format(R"(The prefix "{}" for element "{}" is not bound.)",
                              utf8(*element.prefix), utf8(element.rawname)));
        }
        for (Attribute& attribute : attributes)
        {
            const std::u32string                prefix = attribute.name.prefix.value_or(U"");
            const std::optional<std::u32string> uri    = uriOf(prefix);
            if (attribute.name.uri.has_value() && attribute.name.uri == uri)
            {
                continue;  // a namespace declaration
            }
            if (!prefix.empty())
            {
                if (!uri.has_value())
                {
                    fatal(std::format(
                        "The prefix \"{}\" for attribute \"{}\" associated with an "
                        "element type \"{}\" is not bound.",
                        utf8(prefix), utf8(attribute.name.rawname), utf8(element.rawname)));
                }
                attribute.name.uri = uri;
            }
        }
        if (attributes.size() > 1)
        {
            checkDuplicates(element, attributes);
        }
    }

    /// XMLAttributesImpl.checkDuplicatesNS(): two attributes with the same local part and URI.
    static void checkDuplicates(const QName& element, const std::vector<Attribute>& attributes)
    {
        const auto same = [](const Attribute& a, const Attribute& b) {
            return a.name.localpart == b.name.localpart && a.name.uri == b.name.uri;
        };
        std::optional<std::size_t> duplicate;
        if (attributes.size() <= kSmallAttributeList)
        {
            // pairwise: the first attribute that has a later twin, and its first twin
            for (std::size_t i = 0; i + 1 < attributes.size() && !duplicate; i++)
            {
                for (std::size_t j = i + 1; j < attributes.size(); j++)
                {
                    if (same(attributes[i], attributes[j]))
                    {
                        duplicate = j;
                        break;
                    }
                }
            }
        }
        else
        {
            // a hash table: the first attribute that has an earlier twin
            std::map<std::pair<std::u32string, std::optional<std::u32string>>, std::size_t> seen;
            for (std::size_t j = 0; j < attributes.size(); j++)
            {
                if (!seen.try_emplace({attributes[j].name.localpart, attributes[j].name.uri}, j)
                         .second)
                {
                    duplicate = j;
                    break;
                }
            }
        }
        if (!duplicate.has_value())
        {
            return;
        }
        const QName& name = attributes[*duplicate].name;
        if (name.uri.has_value())
        {
            fatal(std::format(
                "Attribute \"{}\" bound to namespace \"{}\" was already specified for element "
                "\"{}\".",
                utf8(name.localpart), utf8(*name.uri), utf8(element.rawname)));
        }
        attributeNotUnique(element, name);
    }

    /// XMLNSDTDValidator.startNamespaceScope(): with a DTD, namespaces are bound once the start
    /// tag is read, declarations first, and the duplicates are looked for pairwise.
    void bindNamesByValidator(QName& element, std::vector<Attribute>& attributes)
    {
        if (element.prefix == U"xmlns")
        {
            fatal(std::format(R"(Element "{}" cannot have "xmlns" as its prefix.)",
                              utf8(element.rawname)));
        }
        for (const Attribute& attribute : attributes)
        {
            if (isNamespaceDeclaration(attribute.name))
            {
                declareNamespace(attribute.name, attribute.value, true);
            }
        }
        element.uri = uriOf(element.prefix.value_or(U""));
        if (element.prefix.has_value() && !element.uri.has_value())
        {
            fatal(std::format(R"(The prefix "{}" for element "{}" is not bound.)",
                              utf8(*element.prefix), utf8(element.rawname)));
        }
        for (Attribute& attribute : attributes)
        {
            const std::u32string prefix = attribute.name.prefix.value_or(U"");
            if (attribute.name.rawname == U"xmlns")
            {
                attribute.name.uri = kXmlnsUri;
            }
            else if (!prefix.empty())
            {
                attribute.name.uri = uriOf(prefix);
                if (!attribute.name.uri.has_value())
                {
                    fatal(std::format(
                        "The prefix \"{}\" for attribute \"{}\" associated with an "
                        "element type \"{}\" is not bound.",
                        utf8(prefix), utf8(attribute.name.rawname), utf8(element.rawname)));
                }
            }
        }
        for (std::size_t i = 0; i + 1 < attributes.size(); i++)
        {
            const QName& a = attributes[i].name;
            if (!a.uri.has_value() || a.uri == kXmlnsUri)
            {
                continue;
            }
            for (std::size_t j = i + 1; j < attributes.size(); j++)
            {
                if (attributes[j].name.localpart == a.localpart && attributes[j].name.uri == a.uri)
                {
                    fatal(
                        std::format("Attribute \"{}\" bound to namespace \"{}\" was already "
                                    "specified for element \"{}\".",
                                    utf8(a.localpart), utf8(*a.uri), utf8(element.rawname)));
                }
            }
        }
    }

    [[noreturn]] static void attributeNotUnique(const QName& element, const QName& attribute)
    {
        fatal(std::format(R"(Attribute "{}" was already specified for element "{}".)",
                          utf8(attribute.rawname), utf8(element.rawname)));
    }

    /// An attribute, its name the current character (scanAttribute()); namespace declarations
    /// take effect at once.
    void scanAttribute(const QName& element, std::vector<Attribute>& attributes)
    {
        Attribute attribute{.name = scanQName(m_lastAttributeName), .value = {}};
        m_lastAttributeName = attribute.name;
        skipSpaces();
        if (!skipChar(U'='))
        {
            fatal(
                std::format("Attribute name \"{}\" associated with an element type \"{}\" must be "
                            "followed by the ' = ' character.",
                            utf8(attribute.name.rawname), utf8(element.rawname)));
        }
        skipSpaces();
        attribute.value = scanAttributeValue(element, attribute.name);

        if (m_seenDoctype)
        {
            // With a DTD, the scanner leaves namespaces to the DTD validator and itself rejects a
            // repeated raw name at once.
            checkRawName(element, attributes, attribute.name);
        }
        else if (isNamespaceDeclaration(attribute.name))
        {
            declareNamespace(attribute.name, attribute.value, false);
            if (!m_xml11)
            {
                // The 1.0 scanner rejects a second declaration of a prefix at once.
                checkRawName(element, attributes, attribute.name);
            }
            if (!attribute.name.prefix.has_value())
            {
                attribute.name.prefix = U"xmlns";  // as Xerces files xmlns="..."
            }
            attribute.name.uri = kXmlnsUri;
        }
        attributes.push_back(std::move(attribute));
    }

    static void checkRawName(const QName& element, const std::vector<Attribute>& attributes,
                             const QName& name)
    {
        if (std::ranges::any_of(attributes, [&name](const Attribute& other) {
                return other.name.rawname == name.rawname;
            }))
        {
            attributeNotUnique(element, name);
        }
    }

    /// xmlns="..." or xmlns:prefix="..." (but not prefix:xmlns="...").
    [[nodiscard]] static bool isNamespaceDeclaration(const QName& name)
    {
        return name.prefix == U"xmlns" || (!name.prefix.has_value() && name.localpart == U"xmlns");
    }

    /// Checks and declares the namespace declaration @p name="@p uri": the scanner's checks, or
    /// with @p byValidator those of the DTD validator (XMLNSDTDValidator), which names the
    /// attribute differently in one message.
    void declareNamespace(const QName& name, std::u32string_view uri, bool byValidator)
    {
        const std::u32string_view localpart = name.localpart;
        if ((name.prefix == U"xmlns" && localpart == U"xmlns") || uri == kXmlnsUri)
        {
            fatal(std::string(kCantBindXmlns));
        }
        if ((localpart == U"xml") != (uri == kXmlUri))
        {
            fatal(std::string(kCantBindXml));
        }
        // XML 1.1 lets a declaration undeclare a prefix.
        if (!m_xml11 && uri.empty() && localpart != U"xmlns")
        {
            fatal(
                std::format("The value of the attribute \"{}\" is invalid. Prefixed namespace "
                            "bindings may not be empty.",
                            byValidator ? utf8(name.rawname) : toString(name)));
        }
        m_bindings.emplace_back(localpart != U"xmlns" ? std::u32string(localpart) : U"",
                                uri.empty() ? std::nullopt : std::optional<std::u32string>(uri));
    }

    /// An attribute value (scanAttributeValue()), normalised as CDATA.
    [[nodiscard]] std::u32string scanAttributeValue(const QName& element, const QName& attribute)
    {
        const char32_t quote = peek();
        if (quote != U'\'' && quote != U'"')
        {
            fatal(
                std::format("Open quote is expected for attribute \"{}\" associated with an  "
                            "element type  \"{}\".",
                            utf8(attribute.rawname), utf8(element.rawname)));
        }
        m_position++;
        std::u32string value;
        for (char32_t c = peek(); c != quote; c = peek())
        {
            if (c == U'&')
            {
                m_position++;
                value += skipChar(U'#') ? scanCharReference() : scanEntityReference();
                continue;
            }
            if (c == U'<')
            {
                fatal(
                    std::format("The value of attribute \"{}\" associated with an element type "
                                "\"{}\" must not contain the '<' character.",
                                utf8(attribute.rawname), utf8(element.rawname)));
            }
            if (isInvalidLiteral(c))
            {
                fatal(
                    std::format("An invalid XML character (Unicode: 0x{}) was found in the value "
                                "of attribute \"{}\" and element is \"{}\".",
                                hex(c), utf8(attribute.rawname), utf8(element.rawname)));
            }
            value += c == U'\t' || c == U'\n' ? U' ' : c;
            m_position++;
        }
        m_position++;
        return value;
    }

    /// A character reference after "&#" (scanCharReferenceValue()).
    [[nodiscard]] char32_t scanCharReference()
    {
        const bool hexadecimal = skipChar(U'x');
        const auto isDigit     = [hexadecimal](char32_t c) {
            return (c >= U'0' && c <= U'9') ||
                   (hexadecimal && ((c >= U'a' && c <= U'f') || (c >= U'A' && c <= U'F')));
        };
        if (!isDigit(peek()))
        {
            fatal(hexadecimal ? "A hexadecimal representation must immediately follow the \"&#x\" "
                                "in a character reference."
                              : "A decimal representation must immediately follow the \"&#\" in a "
                                "character reference.");
        }
        std::string   digits;
        std::uint64_t value    = 0;
        bool          overflow = false;
        while (isDigit(peek()))
        {
            const char32_t c = scanChar();
            digits += static_cast<char>(c);
            std::uint64_t digit = c - U'0';
            if (c >= U'a')
            {
                digit = c - U'a' + 10;
            }
            else if (c >= U'A')
            {
                digit = c - U'A' + 10;
            }
            // Integer.parseInt() fails beyond Integer.MAX_VALUE.
            value    = (value * (hexadecimal ? 16U : 10U)) + digit;
            overflow = overflow || value > 0x7FFFFFFF;
            value    = std::min<std::uint64_t>(value, 0x80000000U);
        }
        if (!skipChar(U';'))
        {
            fatal("The character reference must end with the ';' delimiter.");
        }
        if (overflow || !isValidReference(static_cast<std::uint32_t>(value)))
        {
            fatal(std::format("Character reference \"&#{}{}\" is an invalid XML character.",
                              hexadecimal ? "x" : "", digits));
        }
        return static_cast<char32_t>(value);
    }

    /// An entity reference after '&' (scanEntityReference()): only the predefined entities are
    /// declared.
    [[nodiscard]] char32_t scanEntityReference()
    {
        const std::u32string name = scanName();
        if (name.empty())
        {
            fatal(std::string(kNameRequiredInReference));
        }
        if (!skipChar(U';'))
        {
            fatal(std::format("The reference to entity \"{}\" must end with the ';' delimiter.",
                              utf8(name)));
        }
        const std::optional<char32_t> predefined = predefinedEntity(name);
        if (!predefined.has_value())
        {
            fatal(std::format("The entity \"{}\" was referenced, but not declared.", utf8(name)));
        }
        return *predefined;
    }

    /// The content of the root element, up to its end tag (the content driver).
    void scanContent()
    {
        while (!m_elements.empty())
        {
            const char32_t c = peek();
            if (c == U'<')
            {
                m_position++;
                m_markupDepth++;
                scanMarkup();
            }
            else if (c == U'&')
            {
                m_position++;
                m_markupDepth++;
                if (skipChar(U'#'))
                {
                    (void)scanCharReference();
                }
                else
                {
                    (void)scanEntityReference();
                }
                m_markupDepth--;
            }
            else
            {
                scanCharacterData();
            }
        }
    }

    /// Markup in content after '<'.
    void scanMarkup()
    {
        const char32_t c = peek();
        if (isNameStart(c))
        {
            (void)scanStartElement();
        }
        else if (c == U'/')
        {
            m_position++;
            if (m_xml11 && atEnd())
            {
                endOfDocument();  // XML 1.1's scanner looks past "</" first
            }
            scanEndElement();
        }
        else if (c == U'!')
        {
            m_position++;
            if (skipChar(U'-'))
            {
                if (!skipChar(U'-'))
                {
                    fatal(std::string(kInvalidCommentStart));
                }
                scanComment();
            }
            else if (skipString(U"[CDATA["))
            {
                scanCData();
            }
            else if (skipString(U"DOCTYPE"))
            {
                // The JDK's content driver has no state for it and throws a bare SAXException.
                fatal("Scanner State 24 not Recognized ");
            }
            else
            {
                fatal(std::string(kMarkupInContent));
            }
        }
        else if (c == U'?')
        {
            m_position++;
            scanPi();
        }
        else
        {
            fatal(std::string(kMarkupInContent));
        }
    }

    /// Character data up to the next '<' or '&' (scanContent()).
    void scanCharacterData()
    {
        while (true)
        {
            const char32_t c = peek();
            if (c == U'<' || c == U'&')
            {
                return;
            }
            m_position++;
            if (c == U']')
            {
                if (skipChar(U']'))
                {
                    while (skipChar(U']'))
                    {
                    }
                    if (skipChar(U'>'))
                    {
                        fatal(std::string(kCdEndInContent));
                    }
                }
            }
            else if (isInvalidLiteral(c))
            {
                fatal(
                    std::format("An invalid XML character (Unicode: 0x{}) was found in the "
                                "element content of the document.",
                                hex(c)));
            }
        }
    }

    /// A CDATA section after "<![CDATA[" (scanCDATASection()).
    void scanCData()
    {
        while (true)
        {
            const char32_t c = peek();
            if (c == U']' && peekAhead(1) == U']' && peekAhead(2) == U'>')
            {
                m_position += 3;
                break;
            }
            if (isInvalidLiteral(c))
            {
                fatal(std::format(
                    "An invalid XML character (Unicode: 0x{}) was found in the CDATA section.",
                    hex(c)));
            }
            m_position++;
        }
        m_markupDepth--;
    }

    /// An end tag after "</" (scanEndElement()): the open element's name, white space, '>'.
    void scanEndElement()
    {
        const std::u32string rawname = std::move(m_elements.back());
        m_elements.pop_back();
        if (!skipString(rawname))
        {
            fatal(std::format(
                R"(The element type "{}" must be terminated by the matching end-tag "</{}>".)",
                utf8(rawname), utf8(rawname)));
        }
        skipSpaces();
        if (!skipChar(U'>'))
        {
            fatal(std::format("The end-tag for element type \"{}\" must end with a '>' delimiter.",
                              utf8(rawname)));
        }
        m_markupDepth -= 2;
        popContext();
        m_events++;
    }

    // ---- after the root element ----------------------------------------------------------------

    /// TrailingMiscDriver: comments, processing instructions and white space to the end.
    void scanTrailingMisc()
    {
        while (true)
        {
            while (!atEnd() && isSpace(m_chars[m_position]))
            {
                m_position++;
            }
            if (atEnd())
            {
                return;
            }
            if (!skipChar(U'<'))
            {
                fatal(std::string(kContentInTrailing));
            }
            m_markupDepth++;
            if (skipChar(U'?'))
            {
                scanPi();
            }
            else if (skipChar(U'!'))
            {
                if (!skipString(U"--"))
                {
                    fatal(std::string(kInvalidCommentStart));
                }
                scanComment();
            }
            else
            {
                fatal(std::string(kMarkupInMisc));
            }
        }
    }

    // ---- namespaces ----------------------------------------------------------------------------

    /// The namespace @p prefix is bound to ("" is the default namespace), none when unbound.
    [[nodiscard]] std::optional<std::u32string> uriOf(std::u32string_view prefix) const
    {
        const auto binding =
            std::ranges::find(m_bindings.rbegin(), m_bindings.rend(), prefix, &Binding::first);
        if (binding != m_bindings.rend())
        {
            return binding->second;
        }
        if (prefix == U"xml")
        {
            return std::u32string(kXmlUri);
        }
        if (prefix == U"xmlns")
        {
            return std::u32string(kXmlnsUri);
        }
        return std::nullopt;
    }

    void popContext()
    {
        m_bindings.resize(m_contexts.back());
        m_contexts.pop_back();
    }

    using Binding = std::pair<std::u32string, std::optional<std::u32string>>;

    std::u32string m_chars;
    std::size_t    m_position{0};
    bool           m_xml11{false};
    /// fMarkupDepth: open elements plus the markup being read.
    int  m_markupDepth{0};
    bool m_seenDoctype{false};
    bool m_inInternalSubset{false};
    /// The raw names of the open elements.
    std::vector<std::u32string> m_elements;
    /// The namespace declarations in scope, and where each open element's begin.
    std::vector<Binding>     m_bindings;
    std::vector<std::size_t> m_contexts;
    /// The QNames Xerces last read into its element and attribute QName objects ("null" before).
    QName m_lastElementName{.rawname = U"null", .prefix = {}, .localpart = U"null", .uri = {}};
    QName m_lastAttributeName{.rawname = U"null", .prefix = {}, .localpart = U"null", .uri = {}};
    std::size_t m_events{0};
};

}  // namespace

XmlScanner::Report XmlScanner::scan(std::string_view text)
{
    return Scanner(Strings::toCodePoints(text)).run();
}

}  // namespace QtRocket
