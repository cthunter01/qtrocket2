#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{

struct OrcNodes;

/// An element of an .orc component preset file (an OrcDocument), read as JAXB binds it to
/// OpenRocket's DTOs (info.openrocket.core.preset.xml). JAXB's unmarshaller is namespace-aware
/// and has its own rules for an element's text; QtRocket reimplements them from what OpenRocket
/// answers for the edge cases of the probe tools/openrocket-probes/orc-loader (the case ids in
/// brackets below), not from JAXB's code.
///
/// Names. The DTOs bind elements and attributes of no namespace, by their local names
/// (isNamed(), lastChild(), attribute()). An element in a namespace, by a prefix or by a default
/// namespace (xmlns="urn:x"), is someone else's and is ignored ([71], [T17], [T19], [XT17],
/// [XT18]; xmlns="" is no namespace: [T18], [XT19]); so is an attribute with a prefix ([70],
/// [XT14]). Unknown elements and attributes are ignored silently ([02], [03], [T16], [77]), and
/// so is xsi:nil ([XT09], [XT10]).
///
/// Texts. JAXB collects an element's character data (references expanded, line ends
/// normalised, CDATA sections joined, comments and processing instructions skipped: [69], [T03],
/// [XS00], [XS01], [XS05], [XT03], [XT04], [XT07]) and hands it over at each child element and
/// at the end of the element; what is inside a child element is the child's ([XT01], [XT05]).
/// At a child element it hands over the text collected since the start or the previous child
/// element unless that text is empty or white space only ([XI13], [XI16], [XI17]); at the end it
/// always hands over the text after the last child element, even when that is empty ([T01],
/// [XT02], [XT05], [XT25], [XT45] to [XT47]). texts() are those texts. A field keeps the last
/// value they give it, where a text that its type rejects gives none (JaxbLexical): so a String
/// or an enum holds the last text (text()); a double, an Integer or a Boolean the last one that
/// it reads ([XN11], [XN12], [XI12], [XI14], [XI15], [XB17] to [XB30]).
///
/// Repeated elements. Each element a field is bound to replaces what the ones before it gave: a
/// field of a DTO type (a Length, a Material, the lists of <Components> and <Materials>), a
/// String, an enum and the primitive double are what the last element gives ([25], [43], [T07],
/// [XT21], [XT30], [XT33], [XT35], [XT36], [XT38] to [XT40], [XT44]). An Integer, a Double and a
/// Boolean, though, keep a value of an earlier element when a later one gives none ([XT31],
/// [XT32], [XT34], [XT37], [XT41]). The *Property() functions apply these rules.
///
/// An OrcElement is a view into its OrcDocument and valid while that lives (also when the
/// document is moved). It is cheap to copy.
class OrcElement final
{
public:
    /// The namespace of the element's expanded name, empty for none.
    [[nodiscard]] std::string_view namespaceUri() const noexcept;

    /// The local name, without a prefix.
    [[nodiscard]] std::string_view localName() const noexcept;

    /// Whether the element is in no namespace and has the local name @p name: one the DTOs bind
    /// by that name.
    [[nodiscard]] bool isNamed(std::string_view name) const noexcept;

    /// The child elements, of every namespace, in document order.
    [[nodiscard]] std::vector<OrcElement> children() const;

    /// The child elements isNamed(@p name), in document order.
    [[nodiscard]] std::vector<OrcElement> children(std::string_view name) const;

    /// The last child element isNamed(@p name), or nullopt when there is none: the element a
    /// field of a DTO type binds (AnnotatedLengthDTO, AnnotatedMaterialDTO, MaterialDTO, ...)
    /// and the wrapper of a list (<Components>, <Materials>), which a later one replaces.
    [[nodiscard]] std::optional<OrcElement> lastChild(std::string_view name) const;

    /// The value of the attribute @p name without a namespace prefix ("Unit", "Type", ...), or
    /// nullopt when the element has none. The value is normalised as an XML parser normalises an
    /// attribute: references expanded, a literal tab or line end read as a space ([XT11] to
    /// [XT13], [XT15]); it is not trimmed ([24], [T04]).
    [[nodiscard]] std::optional<std::string_view> attribute(std::string_view name) const;

    /// The texts JAXB hands the element's value, in order (see the class documentation); at
    /// least one, the last of which is text().
    [[nodiscard]] std::span<const std::string> texts() const noexcept;

    /// The text after the last child element: what a String, an enum or an XmlValue String
    /// holds. Never trimmed ([73], [T11], [T12], [XT16]).
    [[nodiscard]] const std::string& text() const noexcept;

    /// The element's value as a double (an AnnotatedLengthDTO's or AnnotatedMassDTO's XmlValue,
    /// or a simple-typed element): the last of texts() that JaxbLexical::parseDouble() reads, or
    /// nullopt when it reads none (a primitive double then keeps its 0.0, a Double its null).
    [[nodiscard]] std::optional<double> doubleValue() const;

    /// The element's value as an Integer: the last of texts() that JaxbLexical::parseInt()
    /// reads, or nullopt (null) when it reads none.
    [[nodiscard]] std::optional<int> intValue() const;

    /// The element's value as a Boolean: what JaxbLexical::parseBoolean() makes of the last of
    /// texts() it does not reject, nullopt for null.
    [[nodiscard]] std::optional<bool> booleanValue() const;

    /// A String field bound to the child elements named @p name (Manufacturer, PartNumber,
    /// Description, Name, Legacy, ...): the text() of the last of them, or nullopt (null) when
    /// there is none. An enum field (Shape, Type, Group) reads this text with
    /// JaxbLexical::parseEnum().
    [[nodiscard]] std::optional<std::string_view> stringProperty(std::string_view name) const;

    /// A primitive double field bound to the child elements named @p name (MaterialDTO's
    /// Density): the doubleValue() of the last of them, or nullopt when there is none or it reads
    /// none, which leaves the field 0.0.
    [[nodiscard]] std::optional<double> doubleProperty(std::string_view name) const;

    /// A Double field bound to the child elements named @p name (MaterialDTO's ShearModulus): the
    /// last value any text of any of them gives, or nullopt (null) when none gives one.
    [[nodiscard]] std::optional<double> boxedDoubleProperty(std::string_view name) const;

    /// An Integer field bound to the child elements named @p name (ParachuteDTO's Sides and
    /// LineCount): the last value any text of any of them gives, or nullopt (null).
    [[nodiscard]] std::optional<int> integerProperty(std::string_view name) const;

    /// A Boolean field bound to the child elements named @p name (Filled): what the last text of
    /// any of them that JaxbLexical::parseBoolean() does not reject makes of it, nullopt for null.
    [[nodiscard]] std::optional<bool> booleanProperty(std::string_view name) const;

    /// A byte[] field bound to the child elements named @p name through OpenRocket's
    /// Base64Adapter (BaseComponentDTO's Thumbnail): the last value any text of any of them gives
    /// (JaxbLexical::parseBase64Binary()), or nullopt (null) when none gives one. As for an
    /// Integer, a text the adapter rejects leaves the value an earlier text gave; the probe has a
    /// single element with a single text only ([75], [76], [XH00] to [XH36]).
    [[nodiscard]] std::optional<std::vector<std::byte>> base64BinaryProperty(
        std::string_view name) const;

private:
    friend class OrcDocument;

    OrcElement(const OrcNodes& nodes, std::size_t index) noexcept;

    const OrcNodes* m_nodes;
    std::size_t     m_index;
};

}  // namespace QtRocket
