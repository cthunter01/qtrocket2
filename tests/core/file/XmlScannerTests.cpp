#include "QtRocket/file/XmlScanner.h"

#include <array>
#include <cstddef>
#include <format>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/util/Error.h"

namespace
{

using namespace std::string_view_literals;
using QtRocket::ErrorCode;
using QtRocket::XmlScanner;

/// A document and what the JDK's namespace-aware SAX parser (OpenRocket 5f164fd0e on JDK 17,
/// reading the document through an InputStreamReader) makes of it: the message of its first
/// fatal error (empty when it parses the document) and the startElement() and endElement() calls
/// it makes first. Every expectation was produced by that parser.
struct Case
{
    std::string_view document;
    std::string_view message;
    std::size_t      elementEvents;
    std::string_view description;
};

// clang-format off
constexpr auto kJdkCases = std::to_array<Case>({
    {.document = "<a/>"sv, .message = ""sv, .elementEvents = 2, .description = "the smallest document"sv},
    {.document = R"(<?xml version="1.0" encoding="UTF-8"?>)" "\n" R"(<a b="1">text<c/>&amp;&lt;&gt;&apos;&quot;&#65;&#x42;</a>)" "\n"sv, .message = ""sv, .elementEvents = 4, .description = "references and a child"sv},
    {.document = "<?xml version='1.0' standalone='yes'?><a/>"sv, .message = ""sv, .elementEvents = 2, .description = "standalone"sv},
    {.document = R"(<?xml  version = "1.0"  ?><a/>)"sv, .message = ""sv, .elementEvents = 2, .description = "white space in the declaration"sv},
    {.document = R"(<?xml version="1.0" encoding="no-such-encoding"?><a/>)"sv, .message = ""sv, .elementEvents = 2, .description = "a character stream ignores the encoding"sv},
    {.document = R"(<?xml version="1.1"?><a>&#1;)" "\xc2" "\x85" "\xe2" "\x80" "\xa8" "</a>"sv, .message = ""sv, .elementEvents = 2, .description = "XML 1.1"sv},
    {.document = R"(<?xml-stylesheet href="x"?><a/>)"sv, .message = ""sv, .elementEvents = 2, .description = "a processing instruction whose target starts with xml"sv},
    {.document = "<!-- c --><?pi data?>" "\n" "<a/>" "\n" "<!-- after --><?pi?>" "\n"sv, .message = ""sv, .elementEvents = 2, .description = "misc before and after the root"sv},
    {.document = "<!DOCTYPE a><a/>"sv, .message = ""sv, .elementEvents = 2, .description = "an empty document type declaration"sv},
    {.document = "<!DOCTYPE a [ <!-- c --> <?pi x?> ] ><a/>"sv, .message = ""sv, .elementEvents = 2, .description = "an internal subset of comments and processing instructions"sv},
    {.document = "<a><![CDATA[<&]]]]><![CDATA[>]]></a>"sv, .message = ""sv, .elementEvents = 2, .description = "CDATA"sv},
    {.document = "<a><![CDATA[x]]]]>y]]></a>"sv, .message = R"(The character sequence "]]>" must not appear in content unless used to mark the end of a CDATA section.)"sv, .elementEvents = 1, .description = "CDATA ends at the first ]]>"sv},
    {.document = R"(<a x="1" :b="2" b="3"/>)"sv, .message = ""sv, .elementEvents = 2, .description = "a name that starts with a colon has no prefix"sv},
    {.document = R"(<p:a xmlns:p="u" p:x="1" x="2" xml:lang="en"><p:b/></p:a>)"sv, .message = ""sv, .elementEvents = 4, .description = "namespaces"sv},
    {.document = R"(<a xmlns:xml="http://www.w3.org/XML/1998/namespace"/>)"sv, .message = ""sv, .elementEvents = 2, .description = "xml bound to its own namespace"sv},
    {.document = R"(<a b="&#9;&#10;&#13;x)" "\t" "y" "\n" "z" "\r" "\n" R"(w"/>)"sv, .message = ""sv, .elementEvents = 2, .description = "attribute values"sv},
    {.document = "<a></a >"sv, .message = ""sv, .elementEvents = 2, .description = "white space in an end tag"sv},
    {.document = R"(<?xml version="1.1"?><a xmlns:p=""/>)"sv, .message = ""sv, .elementEvents = 2, .description = "XML 1.1 may undeclare a prefix"sv},
    {.document = R"(<!DOCTYPE a><a xmlns:u="n"><u:b/></a>)"sv, .message = ""sv, .elementEvents = 4, .description = "namespaces with a DTD"sv},
    {.document = "<a>" "\xc3" "\xa9" "\xf0" "\x9f" "\x9a" "\x80" "</a>"sv, .message = ""sv, .elementEvents = 2, .description = "non-ASCII text"sv},
    {.document = "<" "\xc3" "\xa9" "l" "\xc3" "\xa9" "ment att" "\xc3" "\xa9" R"(="v"/>)"sv, .message = ""sv, .elementEvents = 2, .description = "non-ASCII names"sv},
    {.document = R"(<?xml encoding="UTF-8"?><a/>)"sv, .message = "The version is required in the XML declaration."sv, .elementEvents = 0, .description = "no version"sv},
    {.document = "<?xml?><a/>"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 0, .description = "<?xml?> is a processing instruction"sv},
    {.document = R"(<?xml version="2.0"?><a/>)"sv, .message = R"(XML version "2.0" is not supported, only XML 1.0 is supported.)"sv, .elementEvents = 0, .description = "version 2.0"sv},
    {.document = R"(<?xml version="1.1x"?><a/>)"sv, .message = R"(XML version "1.1x" is not supported, only XML 1.0 is supported.)"sv, .elementEvents = 0, .description = "version 1.1x"sv},
    {.document = R"(<?xml  version="1.0x"?><a/>)"sv, .message = R"(XML version "1.0x " is not supported, only XML 1.0 is supported.)"sv, .elementEvents = 0, .description = "the version detector pads the value"sv},
    {.document = R"(<?xml version="1.0"encoding="UTF-8"?><a/>)"sv, .message = "White space is required before the encoding pseudo attribute in the XML declaration."sv, .elementEvents = 0, .description = "no space before encoding"sv},
    {.document = R"(<?xml version="1.0" standalone="maybe"?><a/>)"sv, .message = R"(The standalone document declaration value must be "yes" or "no", not "maybe".)"sv, .elementEvents = 0, .description = "standalone maybe"sv},
    {.document = R"(<?xml version="1.0" standalone="yes" encoding="UTF-8"?><a/>)"sv, .message = "No more pseudo attributes are allowed."sv, .elementEvents = 0, .description = "encoding after standalone"sv},
    {.document = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes" foo="1"?><a/>)"sv, .message = "A pseudo attribute name is expected."sv, .elementEvents = 0, .description = "an unknown pseudo attribute"sv},
    {.document = R"(<?xml version "1.0"?><a/>)"sv, .message = R"(The ' = ' character must follow "version" in the XML declaration.)"sv, .elementEvents = 0, .description = "no ="sv},
    {.document = "<?xml version=1.0?><a/>"sv, .message = R"(The value following "version" in the XML declaration must be a quoted string.)"sv, .elementEvents = 0, .description = "no quote"sv},
    {.document = R"(<?xml version="1.0" ?<a/>)"sv, .message = R"(The XML declaration must end with "?>".)"sv, .elementEvents = 0, .description = "no ?>"sv},
    {.document = R"(<?xml version="1)" "\x01" R"(0"?><a/>)"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the XML declaration."sv, .elementEvents = 0, .description = "a control character in the declaration"sv},
    {.document = R"(  <?xml version="1.0"?><a/>)"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 0, .description = "white space before the declaration"sv},
    {.document = R"(<?XML version="1.0"?><a/>)"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 0, .description = "an upper-case declaration"sv},
    {.document = R"(<?xml version="1.0"?><?xml version="1.0"?><a/>)"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 0, .description = "two declarations"sv},
    {.document = ""sv, .message = "Premature end of file."sv, .elementEvents = 0, .description = "empty"sv},
    {.document = "   " "\n"sv, .message = "Premature end of file."sv, .elementEvents = 0, .description = "only white space"sv},
    {.document = "\xef" "\xbb" "\xbf" "<a/>"sv, .message = "Content is not allowed in prolog."sv, .elementEvents = 0, .description = "a byte-order mark"sv},
    {.document = "x<a/>"sv, .message = "Content is not allowed in prolog."sv, .elementEvents = 0, .description = "text in the prolog"sv},
    {.document = "&amp;<a/>"sv, .message = "Reference is not allowed in prolog."sv, .elementEvents = 0, .description = "a reference in the prolog"sv},
    {.document = "< a/>"sv, .message = "The markup in the document preceding the root element must be well-formed."sv, .elementEvents = 0, .description = "markup in the prolog"sv},
    {.document = "<![CDATA[x]]><a/>"sv, .message = "The markup in the document preceding the root element must be well-formed."sv, .elementEvents = 0, .description = "CDATA in the prolog"sv},
    {.document = "<!-x><a/>"sv, .message = R"(Comment must start with "<!--".)"sv, .elementEvents = 0, .description = "a bad comment in the prolog"sv},
    {.document = "<!DOCTYPE a><!DOCTYPE a><a/>"sv, .message = "Already seen doctype."sv, .elementEvents = 0, .description = "two document type declarations"sv},
    {.document = "<a/>junk"sv, .message = "Content is not allowed in trailing section."sv, .elementEvents = 2, .description = "text after the root"sv},
    {.document = "<a/>" "\x00" "junk"sv, .message = "Content is not allowed in trailing section."sv, .elementEvents = 2, .description = "NUL after the root"sv},
    {.document = "<a/>&amp;"sv, .message = "Content is not allowed in trailing section."sv, .elementEvents = 2, .description = "a reference after the root"sv},
    {.document = "<a/><b/>"sv, .message = "The markup in the document following the root element must be well-formed."sv, .elementEvents = 2, .description = "a second root"sv},
    {.document = "<a/></a>"sv, .message = "The markup in the document following the root element must be well-formed."sv, .elementEvents = 2, .description = "an end tag after the root"sv},
    {.document = "<a/><!DOCTYPE a>"sv, .message = R"(Comment must start with "<!--".)"sv, .elementEvents = 2, .description = "a document type declaration after the root"sv},
    {.document = "<a/><"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 2, .description = "a lone < at the end"sv},
    {.document = "<a/><!--" "\x01" "-->"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the comment."sv, .elementEvents = 2, .description = "a control character in a trailing comment"sv},
    {.document = "<a/><?x " "\x01" "?>"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the processing instruction."sv, .elementEvents = 2, .description = "a control character in a trailing processing instruction"sv},
    {.document = "<!DOCTYPEa><a/>"sv, .message = R"(White space is required after "<!DOCTYPE" in the document type declaration.)"sv, .elementEvents = 0, .description = "no space after DOCTYPE"sv},
    {.document = "<!DOCTYPE 1a><a/>"sv, .message = R"(The root element type must appear after "<!DOCTYPE" in the document type declaration.)"sv, .elementEvents = 0, .description = "no root element type"sv},
    {.document = "<!DOCTYPE a b><a/>"sv, .message = R"(The document type declaration for root element type "a" must end with '>'.)"sv, .elementEvents = 0, .description = "an unterminated declaration"sv},
    {.document = "<!DOCTYPE a [ ] x><a/>"sv, .message = R"(The document type declaration for root element type "a" must end with '>'.)"sv, .elementEvents = 0, .description = "text after the internal subset"sv},
    {.document = "<!DOCTYPE a [ x ]><a/>"sv, .message = "The markup declarations contained or pointed to by the document type declaration must be well-formed."sv, .elementEvents = 0, .description = "text in the internal subset"sv},
    {.document = "<!DOCTYPE a [ <!-x ]><a/>"sv, .message = "The markup declarations contained or pointed to by the document type declaration must be well-formed."sv, .elementEvents = 0, .description = "a bad comment in the internal subset"sv},
    {.document = "<!DOCTYPE a [ <![INCLUDE[ ]]> ]><a/>"sv, .message = "The markup declarations contained or pointed to by the document type declaration must be well-formed."sv, .elementEvents = 0, .description = "a conditional section in the internal subset"sv},
    {.document = "<!DOCTYPE a [ " "\x01" " ]><a/>"sv, .message = "The markup declarations contained or pointed to by the document type declaration must be well-formed."sv, .elementEvents = 0, .description = "a control character in the internal subset"sv},
    {.document = "<!DOCTYPE a [ <!-- " "\x01" " --> ]><a/>"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the comment."sv, .elementEvents = 0, .description = "a control character in a comment of the internal subset"sv},
    {.document = "<!DOCTYPE a [ <?xml x?> ]><a/>"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 0, .description = "an xml processing instruction in the internal subset"sv},
    {.document = "<!DOCTYPE a [ <!-- c -- d --> ]><a/>"sv, .message = R"(The string "--" is not permitted within comments.)"sv, .elementEvents = 0, .description = "a double dash in the internal subset"sv},
    {.document = "<!DOCTYPE a [ "sv, .message = "Premature end of file."sv, .elementEvents = 0, .description = "the end in the internal subset"sv},
    {.document = "<!DOCTYPE a [ ] "sv, .message = "Premature end of file."sv, .elementEvents = 0, .description = "the end after the internal subset"sv},
    {.document = "<!DOCTYPE a"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 0, .description = "the end in the declaration"sv},
    {.document = R"(<a b="1"c="2"/>)"sv, .message = R"(Element type "a" must be followed by either attribute specifications, ">" or "/>".)"sv, .elementEvents = 0, .description = "no space between attributes"sv},
    {.document = "<a/ >"sv, .message = R"(Element type "a" must be followed by either attribute specifications, ">" or "/>".)"sv, .elementEvents = 0, .description = "a space in />"sv},
    {.document = R"(<a b="1" 1="2"/>)"sv, .message = R"(Element type "a" must be followed by either attribute specifications, ">" or "/>".)"sv, .elementEvents = 0, .description = "an attribute name starting with a digit"sv},
    {.document = "<a b/>"sv, .message = R"(Attribute name "b" associated with an element type "a" must be followed by the ' = ' character.)"sv, .elementEvents = 0, .description = "an attribute without a value"sv},
    {.document = "<a b=1/>"sv, .message = R"(Open quote is expected for attribute "b" associated with an  element type  "a".)"sv, .elementEvents = 0, .description = "an unquoted value"sv},
    {.document = R"(<a b="1/>)"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 0, .description = "an unterminated value"sv},
    {.document = R"(<a b="1" b="2"/>)"sv, .message = R"(Attribute "b" was already specified for element "a".)"sv, .elementEvents = 0, .description = "a repeated attribute"sv},
    {.document = R"(<a b="1" c="1" c="2" b="2"/>)"sv, .message = R"(Attribute "b" was already specified for element "a".)"sv, .elementEvents = 0, .description = "the first repeated pair is reported"sv},
    {.document = R"(<a mfg="A<B"/>)"sv, .message = R"(The value of attribute "mfg" associated with an element type "a" must not contain the '<' character.)"sv, .elementEvents = 0, .description = "a < in a value"sv},
    {.document = R"(<a mfg="Es&tes"/>)"sv, .message = R"(The reference to entity "tes" must end with the ';' delimiter.)"sv, .elementEvents = 0, .description = "a bare & in a value"sv},
    {.document = R"(<a b="&;"/>)"sv, .message = "The entity name must immediately follow the '&' in the entity reference."sv, .elementEvents = 0, .description = "no entity name"sv},
    {.document = R"(<a b="&x;"/>)"sv, .message = R"(The entity "x" was referenced, but not declared.)"sv, .elementEvents = 0, .description = "an undeclared entity in a value"sv},
    {.document = R"(<a b="&#1;"/>)"sv, .message = R"(Character reference "&#1" is an invalid XML character.)"sv, .elementEvents = 0, .description = "an invalid character reference in a value"sv},
    {.document = R"(<a mfg="E)" "\x02" R"(s"/>)"sv, .message = R"(An invalid XML character (Unicode: 0x2) was found in the value of attribute "mfg" and element is "a".)"sv, .elementEvents = 0, .description = "a control character in a value"sv},
    {.document = "<a:b:c/>"sv, .message = R"(Element type "a:b" must be followed by either attribute specifications, ">" or "/>".)"sv, .elementEvents = 0, .description = "two colons in an element name"sv},
    {.document = R"(<a b:c:d="1"/>)"sv, .message = R"(Attribute name "b:c" associated with an element type "a" must be followed by the ' = ' character.)"sv, .elementEvents = 0, .description = "two colons in an attribute name"sv},
    {.document = R"(<a:1b xmlns:a="u"/>)"sv, .message = R"(Element or attribute "a:1b" do not match QName production: QName::=(NCName:)?NCName.)"sv, .elementEvents = 0, .description = "a local part that starts with a digit"sv},
    {.document = "<a:/>"sv, .message = R"(Element or attribute "a:" do not match QName production: QName::=(NCName:)?NCName.)"sv, .elementEvents = 0, .description = "an empty local part"sv},
    {.document = "<x:a/>"sv, .message = R"(The prefix "x" for element "x:a" is not bound.)"sv, .elementEvents = 0, .description = "an unbound element prefix"sv},
    {.document = R"(<a y:b="1"/>)"sv, .message = R"(The prefix "y" for attribute "y:b" associated with an element type "a" is not bound.)"sv, .elementEvents = 0, .description = "an unbound attribute prefix"sv},
    {.document = "<xmlns:a/>"sv, .message = R"(Element "xmlns:a" cannot have "xmlns" as its prefix.)"sv, .elementEvents = 0, .description = "the xmlns prefix on an element"sv},
    {.document = R"(<a xmlns:p=""/>)"sv, .message = R"(The value of the attribute "prefix="xmlns",localpart="p",rawname="xmlns:p"" is invalid. Prefixed namespace bindings may not be empty.)"sv, .elementEvents = 0, .description = "an empty prefixed binding"sv},
    {.document = R"(<a xmlns:xml="x"/>)"sv, .message = R"(The prefix "xml" cannot be bound to any namespace other than its usual namespace; neither can the namespace for "xml" be bound to any prefix other than "xml".)"sv, .elementEvents = 0, .description = "xml bound elsewhere"sv},
    {.document = R"(<a xmlns:p="http://www.w3.org/XML/1998/namespace"/>)"sv, .message = R"(The prefix "xml" cannot be bound to any namespace other than its usual namespace; neither can the namespace for "xml" be bound to any prefix other than "xml".)"sv, .elementEvents = 0, .description = "the xml namespace bound to another prefix"sv},
    {.document = R"(<a xmlns:xmlns="u"/>)"sv, .message = R"(The prefix "xmlns" cannot be bound to any namespace explicitly; neither can the namespace for "xmlns" be bound to any prefix explicitly.)"sv, .elementEvents = 0, .description = "xmlns bound"sv},
    {.document = R"(<a xmlns="http://www.w3.org/2000/xmlns/"/>)"sv, .message = R"(The prefix "xmlns" cannot be bound to any namespace explicitly; neither can the namespace for "xmlns" be bound to any prefix explicitly.)"sv, .elementEvents = 0, .description = "the xmlns namespace bound"sv},
    {.document = R"(<a xmlns:u="n" xmlns:v="n" u:x="1" v:x="2"/>)"sv, .message = R"(Attribute "x" bound to namespace "n" was already specified for element "a".)"sv, .elementEvents = 0, .description = "two attributes with the same expanded name"sv},
    {.document = R"(<a xmlns:p="u" b="1" xmlns:p="v" b="2"/>)"sv, .message = R"(Attribute "xmlns:p" was already specified for element "a".)"sv, .elementEvents = 0, .description = "a repeated namespace declaration"sv},
    {.document = R"(<a xmlns:p="u" xmlns:p=""/>)"sv, .message = R"(The value of the attribute "prefix="xmlns",localpart="p",rawname="xmlns:p"" is invalid. Prefixed namespace bindings may not be empty.)"sv, .elementEvents = 0, .description = "the declaration checks come before the repeat check"sv},
    {.document = R"(<?xml version="1.1"?><a xmlns:p="u" xmlns:p=""/>)"sv, .message = R"(Attribute "p" bound to namespace "http://www.w3.org/2000/xmlns/" was already specified for element "a".)"sv, .elementEvents = 0, .description = "XML 1.1 checks repeated declarations at the end"sv},
    {.document = R"(<a xmlns:p=""c="1"/>)"sv, .message = R"(The value of the attribute "prefix="xmlns",localpart="p",rawname="xmlns:p"" is invalid. Prefixed namespace bindings may not be empty.)"sv, .elementEvents = 0, .description = "namespace declarations are checked at once"sv},
    {.document = R"(<!DOCTYPE a><a xmlns:p=""c="1"/>)"sv, .message = R"(Element type "a" must be followed by either attribute specifications, ">" or "/>".)"sv, .elementEvents = 0, .description = "with a DTD, once the tag is read"sv},
    {.document = R"(<!DOCTYPE a><a xmlns:p=""/>)"sv, .message = R"(The value of the attribute "xmlns:p" is invalid. Prefixed namespace bindings may not be empty.)"sv, .elementEvents = 0, .description = "the validator names the attribute"sv},
    {.document = R"(<!DOCTYPE a><a b="1" b="<"/>)"sv, .message = R"(The value of attribute "b" associated with an element type "a" must not contain the '<' character.)"sv, .elementEvents = 0, .description = "with a DTD, a repeat is found at once"sv},
    {.document = R"(<!DOCTYPE a><p:a q:y="1" xmlns:x=""/>)"sv, .message = R"(The value of the attribute "xmlns:x" is invalid. Prefixed namespace bindings may not be empty.)"sv, .elementEvents = 0, .description = "with a DTD, declarations come first"sv},
    {.document = R"(<?xml version="1.1"?><a :b="1"/>)"sv, .message = R"(Attribute name "null" associated with an element type "a" must be followed by the ' = ' character.)"sv, .elementEvents = 0, .description = "XML 1.1 names cannot start with a colon"sv},
    {.document = R"(<?xml version="1.1"?><a x="1"><b :="2"/></a>)"sv, .message = R"(Attribute name "x" associated with an element type "b" must be followed by the ' = ' character.)"sv, .elementEvents = 1, .description = "the last attribute name read is reported"sv},
    {.document = "<a>AT&T rocks</a>"sv, .message = R"(The reference to entity "T" must end with the ';' delimiter.)"sv, .elementEvents = 1, .description = "a bare & in text"sv},
    {.document = "<a>&amp b</a>"sv, .message = R"(The reference to entity "amp" must end with the ';' delimiter.)"sv, .elementEvents = 1, .description = "a reference without ;"sv},
    {.document = "<a>& b</a>"sv, .message = "The entity name must immediately follow the '&' in the entity reference."sv, .elementEvents = 1, .description = "no entity name in text"sv},
    {.document = "<a>&nbsp;</a>"sv, .message = R"(The entity "nbsp" was referenced, but not declared.)"sv, .elementEvents = 1, .description = "an undeclared entity"sv},
    {.document = "<a>&a:b;</a>"sv, .message = R"(The entity "a:b" was referenced, but not declared.)"sv, .elementEvents = 1, .description = "an entity name with a colon"sv},
    {.document = "<a>&#0;</a>"sv, .message = R"(Character reference "&#0" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference to NUL"sv},
    {.document = "<a>&#1;</a>"sv, .message = R"(Character reference "&#1" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference to U+0001"sv},
    {.document = "<a>&#x110000;</a>"sv, .message = R"(Character reference "&#x110000" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference beyond Unicode"sv},
    {.document = "<a>&#xD800;</a>"sv, .message = R"(Character reference "&#xD800" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference to a surrogate"sv},
    {.document = "<a>&#xFFFE;</a>"sv, .message = R"(Character reference "&#xFFFE" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference to U+FFFE"sv},
    {.document = "<a>&#99999999999;</a>"sv, .message = R"(Character reference "&#99999999999" is an invalid XML character.)"sv, .elementEvents = 1, .description = "a character reference that overflows"sv},
    {.document = "<a>&#x;</a>"sv, .message = R"(A hexadecimal representation must immediately follow the "&#x" in a character reference.)"sv, .elementEvents = 1, .description = "no hexadecimal digits"sv},
    {.document = "<a>&#;</a>"sv, .message = R"(A decimal representation must immediately follow the "&#" in a character reference.)"sv, .elementEvents = 1, .description = "no decimal digits"sv},
    {.document = "<a>&#X41;</a>"sv, .message = R"(A decimal representation must immediately follow the "&#" in a character reference.)"sv, .elementEvents = 1, .description = "an upper-case X"sv},
    {.document = "<a>&#65</a>"sv, .message = "The character reference must end with the ';' delimiter."sv, .elementEvents = 1, .description = "a character reference without ;"sv},
    {.document = "<a>a" "\x01" "z</a>"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the element content of the document."sv, .elementEvents = 1, .description = "a control character in text"sv},
    {.document = "<a>a" "\x00" "z</a>"sv, .message = "An invalid XML character (Unicode: 0x0) was found in the element content of the document."sv, .elementEvents = 1, .description = "NUL in text"sv},
    {.document = "<a>a" "\xef" "\xbf" "\xbe" "z</a>"sv, .message = "An invalid XML character (Unicode: 0xfffe) was found in the element content of the document."sv, .elementEvents = 1, .description = "U+FFFE in text"sv},
    {.document = "<a>" "\xc2" "\x86" "</a>"sv, .message = ""sv, .elementEvents = 2, .description = "a C1 control is text in XML 1.0"sv},
    {.document = R"(<?xml version="1.1"?><a>)" "\xc2" "\x86" "</a>"sv, .message = "An invalid XML character (Unicode: 0x86) was found in the element content of the document."sv, .elementEvents = 1, .description = "but not in XML 1.1"sv},
    {.document = "<a>a]]>z</a>"sv, .message = R"(The character sequence "]]>" must not appear in content unless used to mark the end of a CDATA section.)"sv, .elementEvents = 1, .description = "]]> in text"sv},
    {.document = "<a>a]]]>z</a>"sv, .message = R"(The character sequence "]]>" must not appear in content unless used to mark the end of a CDATA section.)"sv, .elementEvents = 1, .description = "]]]> in text"sv},
    {.document = "<a><!-- a -- b --></a>"sv, .message = R"(The string "--" is not permitted within comments.)"sv, .elementEvents = 1, .description = "a double dash in a comment"sv},
    {.document = "<a><!-- a ---></a>"sv, .message = R"(The string "--" is not permitted within comments.)"sv, .elementEvents = 1, .description = "a comment that ends with --->"sv},
    {.document = "<a><!--" "\x01" "--></a>"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the comment."sv, .elementEvents = 1, .description = "a control character in a comment"sv},
    {.document = "<a><![CDATA[x" "\x01" "]]></a>"sv, .message = "An invalid XML character (Unicode: 0x1) was found in the CDATA section."sv, .elementEvents = 1, .description = "a control character in CDATA"sv},
    {.document = "<a><!DOCTYPE b></a>"sv, .message = "Scanner State 24 not Recognized "sv, .elementEvents = 1, .description = "a document type declaration in content"sv},
    {.document = "<a><!ELEMENT b></a>"sv, .message = "The content of elements must consist of well-formed character data or markup."sv, .elementEvents = 1, .description = "a declaration in content"sv},
    {.document = "<a><!-x></a>"sv, .message = R"(Comment must start with "<!--".)"sv, .elementEvents = 1, .description = "a bad comment in content"sv},
    {.document = "<a><?xml x?></a>"sv, .message = R"(The processing instruction target matching "[xX][mM][lL]" is not allowed.)"sv, .elementEvents = 1, .description = "an xml processing instruction in content"sv},
    {.document = "<a><? x?></a>"sv, .message = "The processing instruction must begin with the name of the target."sv, .elementEvents = 1, .description = "a processing instruction without a target"sv},
    {.document = "<a><?x" "\x01" "?></a>"sv, .message = "White space is required between the processing instruction target and data."sv, .elementEvents = 1, .description = "no space after the target"sv},
    {.document = "<a>x<1</a>"sv, .message = "The content of elements must consist of well-formed character data or markup."sv, .elementEvents = 1, .description = "a < that starts no markup"sv},
    {.document = "<a></ a>"sv, .message = R"(The element type "a" must be terminated by the matching end-tag "</a>".)"sv, .elementEvents = 1, .description = "a space after </"sv},
    {.document = "<a></ax>"sv, .message = R"(The end-tag for element type "a" must end with a '>' delimiter.)"sv, .elementEvents = 1, .description = "a longer end tag"sv},
    {.document = "<a></b>"sv, .message = R"(The element type "a" must be terminated by the matching end-tag "</a>".)"sv, .elementEvents = 1, .description = "a mismatched end tag"sv},
    {.document = "<a><b></a></b>"sv, .message = R"(The element type "b" must be terminated by the matching end-tag "</b>".)"sv, .elementEvents = 2, .description = "crossed elements"sv},
    {.document = "<a>"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 1, .description = "the end in content"sv},
    {.document = "<a><b>text"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 2, .description = "the end in content, deeper"sv},
    {.document = "<a><!--"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 1, .description = "the end in a comment"sv},
    {.document = "<a></"sv, .message = R"(The element type "a" must be terminated by the matching end-tag "</a>".)"sv, .elementEvents = 1, .description = "the end in an end tag"sv},
    {.document = R"(<?xml version="1.1"?><a></)"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 1, .description = "the end in an end tag, XML 1.1"sv},
    {.document = R"(<a b="1")"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 0, .description = "the end in a start tag"sv},
    {.document = "<p:"sv, .message = R"(Element or attribute "p:" do not match QName production: QName::=(NCName:)?NCName.)"sv, .elementEvents = 0, .description = "the end after a prefix"sv},
    {.document = "<?xml"sv, .message = "Premature end of file."sv, .elementEvents = 0, .description = "the end in the declaration"sv},
    {.document = R"(<?xml version="1.0")"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 0, .description = "the end in the declaration, later"sv},
    {.document = "<!--"sv, .message = "XML document structures must start and end within the same entity."sv, .elementEvents = 0, .description = "the end in a comment in the prolog"sv},
});
// clang-format on

/// The message of the scan's error, "" when the document is well-formed.
[[nodiscard]] std::string errorOf(const XmlScanner::Report& report)
{
    return report.error.has_value() ? report.error->message : std::string();
}

/// The code of the scan's error, UNKNOWN when the document is well-formed.
[[nodiscard]] ErrorCode codeOf(const XmlScanner::Report& report)
{
    return report.error.has_value() ? report.error->code : ErrorCode::UNKNOWN;
}

TEST(XmlScanner, DecidesAsTheJdkParserDoes)
{
    for (const Case& c : kJdkCases)
    {
        const XmlScanner::Report report = XmlScanner::scan(c.document);
        EXPECT_EQ(errorOf(report), c.message) << c.description;
        EXPECT_EQ(codeOf(report), c.message.empty() ? ErrorCode::UNKNOWN : ErrorCode::PARSE)
            << c.description;
        EXPECT_EQ(report.elementEvents, c.elementEvents) << c.description;
    }
}

TEST(XmlScanner, ReportsXml11)
{
    EXPECT_TRUE(XmlScanner::scan(R"(<?xml version="1.1"?><a/>)").xml11);
    EXPECT_TRUE(XmlScanner::scan("<?xml version = '1.1' ?><a/>").xml11);
    EXPECT_FALSE(XmlScanner::scan(R"(<?xml version="1.0"?><a/>)").xml11);
    EXPECT_FALSE(XmlScanner::scan("<a/>").xml11);
}

TEST(XmlScanner, CountsTheElementEventsOfAWellFormedDocument)
{
    EXPECT_EQ(XmlScanner::scan("<a><b/><c>t</c><d><e/></d></a>").elementEvents, 10U);
    EXPECT_EQ(XmlScanner::scan("<a/>").elementEvents, 2U);
}

TEST(XmlScanner, AppliesTheJdkNameLengthLimit)
{
    // jdk.xml.maxXMLNameLimit: 1000 characters per name, prefix or local part
    const std::string name(1000, 'a');
    EXPECT_FALSE(XmlScanner::scan("<" + name + "/>").error.has_value());
    EXPECT_EQ(errorOf(XmlScanner::scan("<" + name + "b/>")),
              R"(JAXP00010005: The length of entity "[xml]" is "1,001" that exceeds the )"
              R"("1,000" limit set by "FEATURE_SECURE_PROCESSING".)");
    const std::string part(600, 'p');
    EXPECT_FALSE(XmlScanner::scan("<" + part + ":" + part + " xmlns:" + part + R"(="u"/>)")
                     .error.has_value());
    EXPECT_TRUE(XmlScanner::scan("<a>&" + std::string(1001, 'e') + ";</a>").error.has_value());
    // XML 1.1 names are counted in UTF-16 units: a supplementary character counts twice.
    std::string supplementary = R"(<?xml version="1.1"?><a)";
    for (int i = 0; i < 500; i++)
    {
        supplementary += "\U00010000";
    }
    EXPECT_NE(errorOf(XmlScanner::scan(supplementary + "/>")).find(R"("1,001")"),
              std::string::npos);
}

TEST(XmlScanner, AppliesTheJdkAttributeLimit)
{
    // jdk.xml.elementAttributeLimit: 10000 attributes, namespace declarations included
    std::string attributes;
    for (int i = 0; i < 10000; i++)
    {
        attributes += std::format(R"( a{}="1")", i);
    }
    EXPECT_FALSE(XmlScanner::scan("<a" + attributes + "/>").error.has_value());
    EXPECT_EQ(errorOf(XmlScanner::scan("<a" + attributes + R"( xmlns:p="u"/>)")),
              R"(JAXP00010002:  Element "a" has more than "10,000" attributes, "10,000" is the )"
              "limit imposed by the JDK.");
}

TEST(XmlScanner, FindsDuplicatesInLongAttributeListsAsXercesDoes)
{
    // Up to 20 attributes Xerces pairs them up; beyond, it reports the first attribute that repeats
    // an earlier one.
    std::string filler;
    for (int i = 0; i < 17; i++)
    {
        filler += std::format(R"( a{}="1")", i);
    }
    EXPECT_EQ(errorOf(XmlScanner::scan(R"(<a b="1" c="1" c="2" b="2"/>)")),
              R"(Attribute "b" was already specified for element "a".)");
    EXPECT_EQ(errorOf(XmlScanner::scan(R"(<a b="1" c="1" c="2" b="2")" + filler + "/>")),
              R"(Attribute "c" was already specified for element "a".)");
}

TEST(XmlScanner, ReadsDeepDocumentsWithoutRecursion)
{
    constexpr int kDepth = 100000;
    std::string   document;
    for (int i = 0; i < kDepth; i++)
    {
        document += "<a>";
    }
    for (int i = 0; i < kDepth; i++)
    {
        document += "</a>";
    }
    const XmlScanner::Report report = XmlScanner::scan(document);
    EXPECT_FALSE(report.error.has_value());
    EXPECT_EQ(report.elementEvents, 2U * kDepth);
}

TEST(XmlScanner, LeavesDeclarationsAndExternalSubsetsUnread)
{
    // The JDK's parser would read these (and expand, default or fetch); XmlScanner stops.
    for (const std::string_view document :
         {R"(<!DOCTYPE a [<!ENTITY m "Estes">]><a b="&m;"/>)"sv,
          R"(<!DOCTYPE a [<!ATTLIST a b CDATA "7">]><a/>)"sv,
          "<!DOCTYPE a [<!ELEMENT a ANY>]><a/>"sv,
          R"(<!DOCTYPE a [<!NOTATION n SYSTEM "x">]><a/>)"sv,
          R"(<!DOCTYPE a [<!ENTITY % p "x"> %p;]><a/>)"sv, "<!DOCTYPE a [ %p; ]><a/>"sv,
          R"(<!DOCTYPE a SYSTEM "a.dtd"><a/>)"sv, R"(<!DOCTYPE a PUBLIC "-//X//Y" "a.dtd"><a/>)"sv})
    {
        const XmlScanner::Report report = XmlScanner::scan(document);
        EXPECT_EQ(codeOf(report), ErrorCode::UNSUPPORTED_FORMAT) << document;
        EXPECT_EQ(report.elementEvents, 0U) << document;
    }
    // Errors before the unsupported part still come first.
    EXPECT_EQ(codeOf(XmlScanner::scan(R"(<?xml version="2.0"?><!DOCTYPE a SYSTEM "a.dtd"><a/>)")),
              ErrorCode::PARSE);
}

TEST(XmlScanner, ReadsMalformedUtf8AsReplacementCharacters)
{
    // U+FFFD is a valid character; the decoder makes it of any malformed byte.
    EXPECT_EQ(errorOf(XmlScanner::scan("<a>\xff\xfe</a>")), "");
}

}  // namespace
