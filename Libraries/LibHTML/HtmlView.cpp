#include <LibCore/CFile.h>
#include <LibGUI/GApplication.h>
#include <LibGUI/GPainter.h>
#include <LibGUI/GScrollBar.h>
#include <LibGUI/GWindow.h>
#include <LibHTML/DOM/Element.h>
#include <LibHTML/DOM/HTMLAnchorElement.h>
#include <LibHTML/Dump.h>
#include <LibHTML/Frame.h>
#include <LibHTML/HtmlView.h>
#include <LibHTML/Layout/LayoutDocument.h>
#include <LibHTML/Layout/LayoutNode.h>
#include <LibHTML/Parser/HTMLParser.h>
#include <LibHTML/RenderingContext.h>
#include <LibHTML/ResourceLoader.h>
#include <stdio.h>

HtmlView::HtmlView(GWidget* parent)
    : GScrollableWidget(parent)
    , m_main_frame(Frame::create())
{
    main_frame().on_set_needs_display = [this](auto& content_rect) {
        Rect adjusted_rect = content_rect;
        adjusted_rect.set_location(to_widget_position(content_rect.location()));
        update(adjusted_rect);
    };

    set_frame_shape(FrameShape::Container);
    set_frame_shadow(FrameShadow::Sunken);
    set_frame_thickness(2);
    set_should_hide_unnecessary_scrollbars(true);
    set_background_color(Color::White);
}

HtmlView::~HtmlView()
{
}

void HtmlView::set_document(Document* document)
{
    if (document == m_document)
        return;

    if (m_document)
        m_document->on_layout_updated = nullptr;

    m_document = document;

    if (m_document) {
        m_document->on_layout_updated = [this] {
            layout_and_sync_size();
            update();
        };
    }

    main_frame().set_document(document);

#ifdef HTML_DEBUG
    if (document != nullptr) {
        dbgprintf("\033[33;1mLayout tree before layout:\033[0m\n");
        ::dump_tree(*layout_root());
    }
#endif

    layout_and_sync_size();
    update();
}

void HtmlView::layout_and_sync_size()
{
    if (!document())
        return;

    bool had_vertical_scrollbar = vertical_scrollbar().is_visible();
    bool had_horizontal_scrollbar = horizontal_scrollbar().is_visible();

    main_frame().set_size(available_size());
    document()->layout();
    set_content_size(layout_root()->size());

    // NOTE: If layout caused us to gain or lose scrollbars, we have to lay out again
    //       since the scrollbars now take up some of the available space.
    if (had_vertical_scrollbar != vertical_scrollbar().is_visible() || had_horizontal_scrollbar != horizontal_scrollbar().is_visible()) {
        main_frame().set_size(available_size());
        document()->layout();
        set_content_size(layout_root()->size());
    }

#ifdef HTML_DEBUG
    dbgprintf("\033[33;1mLayout tree after layout:\033[0m\n");
    ::dump_tree(*layout_root());
#endif
}

void HtmlView::resize_event(GResizeEvent& event)
{
    GScrollableWidget::resize_event(event);
    layout_and_sync_size();
}

void HtmlView::paint_event(GPaintEvent& event)
{
    GFrame::paint_event(event);

    GPainter painter(*this);
    painter.add_clip_rect(widget_inner_rect());
    painter.add_clip_rect(event.rect());

    if (!layout_root()) {
        painter.fill_rect(event.rect(), background_color());
        return;
    }

    painter.fill_rect(event.rect(), m_document->background_color());

    painter.translate(frame_thickness(), frame_thickness());
    painter.translate(-horizontal_scrollbar().value(), -vertical_scrollbar().value());

    RenderingContext context { painter };
    context.set_should_show_line_box_borders(m_should_show_line_box_borders);
    context.set_viewport_rect(visible_content_rect());
    layout_root()->render(context);
}

void HtmlView::mousemove_event(GMouseEvent& event)
{
    if (!layout_root())
        return GScrollableWidget::mousemove_event(event);

    bool hovered_node_changed = false;
    bool is_hovering_link = false;
    auto result = layout_root()->hit_test(to_content_position(event.position()));
    if (result.layout_node) {
        auto* node = result.layout_node->node();
        hovered_node_changed = node != m_document->hovered_node();
        m_document->set_hovered_node(const_cast<Node*>(node));
        if (node) {
            if (auto* link = node->enclosing_link_element()) {
                UNUSED_PARAM(link);
#ifdef HTML_DEBUG
                dbg() << "HtmlView: hovering over a link to " << link->href();
#endif
                is_hovering_link = true;
            }
        }
    }
    if (window())
        window()->set_override_cursor(is_hovering_link ? GStandardCursor::Hand : GStandardCursor::None);
    if (hovered_node_changed) {
        update();
        auto* hovered_html_element = m_document->hovered_node() ? m_document->hovered_node()->enclosing_html_element() : nullptr;
        if (hovered_html_element && !hovered_html_element->title().is_null()) {
            auto screen_position = screen_relative_rect().location().translated(event.position());
            GApplication::the().show_tooltip(hovered_html_element->title(), screen_position.translated(4, 4));
        } else {
            GApplication::the().hide_tooltip();
        }
    }
    event.accept();
}

void HtmlView::mousedown_event(GMouseEvent& event)
{
    if (!layout_root())
        return GScrollableWidget::mousemove_event(event);

    bool hovered_node_changed = false;
    auto result = layout_root()->hit_test(to_content_position(event.position()));
    if (result.layout_node) {
        auto* node = result.layout_node->node();
        hovered_node_changed = node != m_document->hovered_node();
        m_document->set_hovered_node(const_cast<Node*>(node));
        if (node) {
            if (auto* link = node->enclosing_link_element()) {
                dbg() << "HtmlView: clicking on a link to " << link->href();
                if (on_link_click)
                    on_link_click(link->href());
            }
        }
    }
    if (hovered_node_changed)
        update();
    event.accept();
}

void HtmlView::reload()
{
    load(main_frame().document()->url());
}

void HtmlView::load(const URL& url)
{
    dbg() << "HtmlView::load: " << url;

    if (window())
        window()->set_override_cursor(GStandardCursor::None);

    if (on_load_start)
        on_load_start(url);

    ResourceLoader::the().load(url, [=](auto data) {
        if (data.is_null()) {
            dbg() << "Load failed!";
            ASSERT_NOT_REACHED();
        }

        auto document = parse_html(data, url);

        set_document(document);

        if (on_title_change)
            on_title_change(document->title());
    });
}

const LayoutDocument* HtmlView::layout_root() const
{
    return document() ? document()->layout_node() : nullptr;
}

LayoutDocument* HtmlView::layout_root()
{
    if (!document())
        return nullptr;
    return const_cast<LayoutDocument*>(document()->layout_node());
}
