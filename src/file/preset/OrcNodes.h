#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace QtRocket
{

/// The elements of an OrcDocument, as JAXB's unmarshaller sees them: OrcDocument builds them,
/// OrcElement reads them. Private to src/file/preset/.
struct OrcNodes
{
    /// An attribute without a namespace: its name has no prefix, and it declares none.
    struct Attribute
    {
        std::string name;
        std::string value;
    };

    struct Node
    {
        /// The namespace of the element's expanded name, empty for none.
        std::string namespaceUri;
        std::string localName;
        /// The attributes without a namespace, in document order (pugixml normalises their values
        /// as an XML parser does).
        std::vector<Attribute> attributes;
        /// The texts JAXB hands the element's value, in order (OrcElement::texts()); never empty.
        std::vector<std::string> texts;
        /// The indices of the child elements, of any namespace, in document order.
        std::vector<std::size_t> children;
    };

    /// Every element of the document, the root first, each before its children (document order).
    std::vector<Node> nodes;
};

}  // namespace QtRocket
