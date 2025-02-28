#pragma once

#include <LibHTML/Layout/LayoutBox.h>
#include <LibHTML/Layout/LineBox.h>

class Element;

class LayoutBlock : public LayoutBox {
public:
    LayoutBlock(const Node*, NonnullRefPtr<StyleProperties>);
    virtual ~LayoutBlock() override;

    virtual const char* class_name() const override { return "LayoutBlock"; }

    virtual void layout() override;
    virtual void render(RenderingContext&) override;

    virtual LayoutNode& inline_wrapper() override;

    bool children_are_inline() const;

    Vector<LineBox>& line_boxes() { return m_line_boxes; }
    const Vector<LineBox>& line_boxes() const { return m_line_boxes; }

    LineBox& ensure_last_line_box();
    LineBox& add_line_box();

    virtual HitTestResult hit_test(const Point&) const override;

    LayoutBlock* previous_sibling() { return to<LayoutBlock>(LayoutNode::previous_sibling()); }
    const LayoutBlock* previous_sibling() const { return to<LayoutBlock>(LayoutNode::previous_sibling()); }
    LayoutBlock* next_sibling() { return to<LayoutBlock>(LayoutNode::next_sibling()); }
    const LayoutBlock* next_sibling() const { return to<LayoutBlock>(LayoutNode::next_sibling()); }

private:
    virtual bool is_block() const override { return true; }

    NonnullRefPtr<StyleProperties> style_for_anonymous_block() const;

    void layout_inline_children();
    void layout_block_children();

    void compute_width();
    void compute_position();
    void compute_height();

    Vector<LineBox> m_line_boxes;
};

template<typename Callback>
void LayoutNode::for_each_fragment_of_this(Callback callback)
{
    auto& block = *containing_block();
    for (auto& line_box : block.line_boxes()) {
        for (auto& fragment : line_box.fragments()) {
            if (callback(fragment) == IterationDecision::Break)
                return;
        }
    }
}

template<>
inline bool is<LayoutBlock>(const LayoutNode& node)
{
    return node.is_block();
}
