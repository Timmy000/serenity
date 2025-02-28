#pragma once

#include <AK/Badge.h>
#include <AK/RefPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibHTML/TreeNode.h>

enum class NodeType : unsigned {
    INVALID = 0,
    ELEMENT_NODE = 1,
    TEXT_NODE = 3,
    COMMENT_NODE = 8,
    DOCUMENT_NODE = 9,
    DOCUMENT_TYPE_NODE = 10,
};

class Document;
class Element;
class HTMLElement;
class HTMLAnchorElement;
class ParentNode;
class LayoutNode;
class StyleResolver;
class StyleProperties;

class Node : public TreeNode<Node> {
public:
    virtual ~Node();

    NodeType type() const { return m_type; }
    bool is_element() const { return type() == NodeType::ELEMENT_NODE; }
    bool is_text() const { return type() == NodeType::TEXT_NODE; }
    bool is_document() const { return type() == NodeType::DOCUMENT_NODE; }
    bool is_document_type() const { return type() == NodeType::DOCUMENT_TYPE_NODE; }
    bool is_comment() const { return type() == NodeType::COMMENT_NODE; }
    bool is_character_data() const { return type() == NodeType::TEXT_NODE || type() == NodeType::COMMENT_NODE; }
    bool is_parent_node() const { return is_element() || is_document(); }

    virtual RefPtr<LayoutNode> create_layout_node(const StyleProperties* parent_style) const;

    virtual String tag_name() const = 0;

    virtual String text_content() const;

    Document& document() { return m_document; }
    const Document& document() const { return m_document; }

    const HTMLAnchorElement* enclosing_link_element() const;
    const HTMLElement* enclosing_html_element() const;

    virtual bool is_html_element() const { return false; }

    template<typename T>
    const T* first_child_of_type() const;

    template<typename T>
    const T* first_ancestor_of_type() const;

    virtual void inserted_into(Node&) {}
    virtual void removed_from(Node&) {}

    const LayoutNode* layout_node() const { return m_layout_node; }
    LayoutNode* layout_node() { return m_layout_node; }

    void set_layout_node(Badge<LayoutNode>, LayoutNode* layout_node) const { m_layout_node = layout_node; }

    const Element* previous_element_sibling() const;
    const Element* next_element_sibling() const;

    virtual bool is_child_allowed(const Node&) const { return true; }

    void invalidate_style();

protected:
    Node(Document&, NodeType);

    Document& m_document;
    mutable LayoutNode* m_layout_node { nullptr };
    NodeType m_type { NodeType::INVALID };
};

template<typename T>
inline bool is(const Node&)
{
    return false;
}

template<typename T>
inline bool is(const Node* node)
{
    return !node || is<T>(*node);
}

template<>
inline bool is<Node>(const Node&)
{
    return true;
}

template<>
inline bool is<ParentNode>(const Node& node)
{
    return node.is_parent_node();
}

template<typename T>
inline const T& to(const Node& node)
{
    ASSERT(is<T>(node));
    return static_cast<const T&>(node);
}

template<typename T>
inline T* to(Node* node)
{
    ASSERT(is<T>(node));
    return static_cast<T*>(node);
}

template<typename T>
inline const T* to(const Node* node)
{
    ASSERT(is<T>(node));
    return static_cast<const T*>(node);
}

template<typename T>
inline T& to(Node& node)
{
    ASSERT(is<T>(node));
    return static_cast<T&>(node);
}

template<typename T>
inline const T* Node::first_child_of_type() const
{
    for (auto* child = first_child(); child; child = child->next_sibling()) {
        if (is<T>(*child))
            return to<T>(child);
    }
    return nullptr;
}

template<typename T>
inline const T* Node::first_ancestor_of_type() const
{
    for (auto* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (is<T>(*ancestor))
            return to<T>(ancestor);
    }
    return nullptr;
}
