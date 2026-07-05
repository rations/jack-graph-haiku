#include "GraphCanvasView.h"

#include <Bitmap.h>
#include <Font.h>
#include <Window.h>

#include <algorithm>
#include <cmath>

// Colors match the Cairo rgba values of the Linux original; alpha is applied
// through B_OP_ALPHA over the opaque canvas background.
static const rgb_color kBackground = {38, 38, 46, 255};
static const rgb_color kBoxFill = {56, 56, 66, 242};
static const rgb_color kBoxBorder = {89, 89, 102, 204};
static const rgb_color kHeaderLine = {89, 89, 102, 128};
static const rgb_color kHeaderText = {230, 230, 235, 255};
static const rgb_color kPortText = {204, 204, 217, 255};
static const rgb_color kAudioPort = {64, 115, 217, 255};
static const rgb_color kMidiPort = {51, 179, 89, 255};
static const rgb_color kCable = {140, 140, 153, 128};
static const rgb_color kDragPreview = {230, 128, 51, 178};

GraphCanvasView::GraphCanvasView()
    : BView("graph_canvas", B_WILL_DRAW | B_FRAME_EVENTS), m_zoom(1.0), m_pan_x(0.0),
      m_pan_y(0.0), m_is_dragging(false), m_is_panning(false), m_is_moving_box(false),
      m_drag_source(nullptr), m_moving_box(nullptr), m_drag_current_x(0), m_drag_current_y(0),
      m_pan_start_x(0), m_pan_start_y(0), m_box_move_offset_x(0), m_box_move_offset_y(0),
      m_did_initial_fit(false), m_offscreen(nullptr), m_offscreen_view(nullptr) {
    // The view is repainted entirely from the offscreen buffer; app_server
    // must not pre-clear or resizing flickers.
    SetViewColor(B_TRANSPARENT_COLOR);
}

GraphCanvasView::~GraphCanvasView() {
    delete m_offscreen;
}

void GraphCanvasView::AttachedToWindow() {
    BView::AttachedToWindow();
    RebuildOffscreen();
}

void GraphCanvasView::DetachedFromWindow() {
    delete m_offscreen;
    m_offscreen = nullptr;
    m_offscreen_view = nullptr;
    BView::DetachedFromWindow();
}

void GraphCanvasView::FrameResized(float newWidth, float newHeight) {
    (void)newWidth;
    (void)newHeight;
    RebuildOffscreen();
    // Fit the graph once the layout system has given the view its real size;
    // before that Bounds() is meaningless for centering.
    if (!m_did_initial_fit && Bounds().Width() > 50 && Bounds().Height() > 50) {
        m_did_initial_fit = true;
        fit_to_window();
    }
    Invalidate();
}

void GraphCanvasView::RebuildOffscreen() {
    delete m_offscreen;
    m_offscreen = nullptr;
    m_offscreen_view = nullptr;

    BRect bounds = Bounds();
    if (!bounds.IsValid() || bounds.Width() < 1 || bounds.Height() < 1)
        return;

    // Offscreen bitmap that accepts a child view: the scene is drawn there
    // and blitted in Draw(), giving flicker-free double buffering.
    BBitmap* bitmap = new (std::nothrow) BBitmap(bounds, B_RGB32, true);
    if (bitmap == nullptr || bitmap->InitCheck() != B_OK || !bitmap->IsValid()) {
        delete bitmap;
        return;
    }
    m_offscreen = bitmap;
    m_offscreen_view = new (std::nothrow) BView(bounds, "offscreen", B_FOLLOW_NONE, 0);
    if (m_offscreen_view == nullptr) {
        delete m_offscreen;
        m_offscreen = nullptr;
        return;
    }
    m_offscreen->AddChild(m_offscreen_view);
}

BPoint GraphCanvasView::ToScreen(double x, double y) const {
    return BPoint((float)(x * m_zoom + m_pan_x), (float)(y * m_zoom + m_pan_y));
}

void GraphCanvasView::Draw(BRect updateRect) {
    if (m_offscreen == nullptr || m_offscreen_view == nullptr) {
        // Fallback (offscreen allocation failed): draw directly.
        DrawScene(this);
        return;
    }

    if (m_offscreen->Lock()) {
        DrawScene(m_offscreen_view);
        m_offscreen_view->Sync();
        m_offscreen->Unlock();
    }
    DrawBitmap(m_offscreen, updateRect, updateRect);
}

void GraphCanvasView::DrawScene(BView* view) {
    view->SetDrawingMode(B_OP_COPY);
    view->SetHighColor(kBackground);
    view->FillRect(view->Bounds());
    view->SetDrawingMode(B_OP_ALPHA);

    for (auto& conn : m_connections) {
        DrawConnectionLine(view, *conn);
    }

    for (const auto& box : m_client_boxes) {
        DrawClientBox(view, box);
    }

    if (m_is_dragging && m_drag_source) {
        DrawDragPreview(view);
    }
}

void GraphCanvasView::DrawClientBox(BView* view, const ClientBox& box) {
    BPoint lt = ToScreen(box.x, box.y);
    BPoint rb = ToScreen(box.x + box.width, box.y + box.height);
    BRect rect(lt, rb);
    float radius = (float)(6 * m_zoom);

    view->SetHighColor(kBoxFill);
    view->FillRoundRect(rect, radius, radius);

    view->SetHighColor(kBoxBorder);
    view->SetPenSize((float)(1.5 * m_zoom));
    view->StrokeRoundRect(rect, radius, radius);

    BPoint header_l = ToScreen(box.x, box.y + ClientBox::HEADER_HEIGHT);
    BPoint header_r = ToScreen(box.x + box.width, box.y + ClientBox::HEADER_HEIGHT);
    view->SetHighColor(kHeaderLine);
    view->SetPenSize((float)(1.0 * m_zoom));
    view->StrokeLine(header_l, header_r);

    // Client name, bold, centered in the header.
    BFont header_font(be_bold_font);
    header_font.SetSize((float)(12 * m_zoom));
    view->SetFont(&header_font);
    BString header(box.client_name.c_str());
    header_font.TruncateString(&header, B_TRUNCATE_END, (float)((box.width - 8) * m_zoom));
    float hw = view->StringWidth(header.String());
    font_height fh;
    header_font.GetHeight(&fh);
    float header_center_y = (float)(lt.y + (ClientBox::HEADER_HEIGHT * m_zoom) / 2);
    float baseline = header_center_y + (fh.ascent - fh.descent) / 2;
    view->SetHighColor(kHeaderText);
    view->DrawString(header.String(), BPoint((lt.x + rb.x - hw) / 2, baseline));

    double in_x = box.x + ClientBox::SIDE_PAD;
    double out_x = box.x + ClientBox::SIDE_PAD + ClientBox::COL_WIDTH + ClientBox::SIDE_PAD;
    double port_y = box.y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD;

    for (const auto& node : box.inputs) {
        DrawPort(view, *node, in_x, port_y, false);
        port_y += ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD;
    }

    port_y = box.y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD;
    for (const auto& node : box.outputs) {
        DrawPort(view, *node, out_x, port_y, true);
        port_y += ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD;
    }
}

void GraphCanvasView::DrawPort(BView* view, const Node& node, double x, double y,
                               bool is_output) {
    double w = ClientBox::PORT_BG_WIDTH;
    double h = ClientBox::PORT_HEIGHT;

    rgb_color color = (node.type == PortType::AUDIO) ? kAudioPort : kMidiPort;

    double bg_x = is_output ? x - (ClientBox::PORT_BG_WIDTH - ClientBox::COL_WIDTH) : x;
    BRect bg_rect(ToScreen(bg_x, y), ToScreen(bg_x + w, y + h));
    rgb_color bg_color = color;
    bg_color.alpha = 31; /* 0.12 */
    view->SetHighColor(bg_color);
    float radius = (float)(3 * m_zoom);
    view->FillRoundRect(bg_rect, radius, radius);

    double port_dot_x = is_output ? bg_x + w : bg_x;
    double port_dot_y = y + h / 2;
    rgb_color dot_color = color;
    dot_color.alpha = 242; /* 0.95 */
    view->SetHighColor(dot_color);
    float dot_r = (float)(PORT_RADIUS * m_zoom);
    view->FillEllipse(ToScreen(port_dot_x, port_dot_y), dot_r, dot_r);

    BFont font(be_plain_font);
    font.SetSize((float)(10 * m_zoom));
    view->SetFont(&font);
    BString label(node.display_name().c_str());
    float max_text_width = (float)((w - PORT_RADIUS * 2 - 12) * m_zoom);
    font.TruncateString(&label, B_TRUNCATE_END, max_text_width);
    float tw = view->StringWidth(label.String());
    font_height fh;
    font.GetHeight(&fh);

    double text_x;
    if (is_output) {
        text_x = bg_x + w - PORT_RADIUS - 4;
    } else {
        text_x = bg_x + PORT_RADIUS + 4;
    }
    BPoint text_origin = ToScreen(text_x, y + h / 2);
    if (is_output)
        text_origin.x -= tw;
    text_origin.y += (fh.ascent - fh.descent) / 2;
    view->SetHighColor(kPortText);
    view->DrawString(label.String(), text_origin);
}

void GraphCanvasView::DrawCable(BView* view, double x1, double y1, double x2, double y2,
                                ::pattern pat) {
    double dx = std::abs(x2 - x1) * 0.5;
    BPoint points[4];
    points[0] = ToScreen(x1, y1);
    points[1] = ToScreen(x1 + dx, y1);
    points[2] = ToScreen(x2 - dx, y2);
    points[3] = ToScreen(x2, y2);
    view->SetPenSize((float)(2.0 * m_zoom));
    view->StrokeBezier(points, pat);
}

void GraphCanvasView::DrawConnectionLine(BView* view, const Connection& conn) {
    view->SetHighColor(kCable);
    DrawCable(view, conn.source->x + conn.source->width,
              conn.source->y + conn.source->height / 2, conn.destination->x,
              conn.destination->y + conn.destination->height / 2, B_SOLID_HIGH);
}

void GraphCanvasView::DrawDragPreview(BView* view) {
    // B_MIXED_COLORS with a transparent low color approximates the dashed
    // preview of the original (Haiku patterns are stipples, not dashes).
    view->SetHighColor(kDragPreview);
    view->SetLowColor(rgb_color{0, 0, 0, 0});
    DrawCable(view, m_drag_source->x + m_drag_source->width,
              m_drag_source->y + m_drag_source->height / 2, m_drag_current_x, m_drag_current_y,
              B_MIXED_COLORS);
}

void GraphCanvasView::add_node(std::shared_ptr<Node> node) {
    m_nodes.push_back(std::move(node));
}

void GraphCanvasView::add_connection(std::shared_ptr<Connection> conn) {
    m_connections.push_back(std::move(conn));
}

void GraphCanvasView::remove_all() {
    for (const auto& box : m_client_boxes) {
        m_saved_positions[box.client_name] = {box.x, box.y};
    }
    m_nodes.clear();
    m_connections.clear();
    m_client_boxes.clear();
    Invalidate();
}

void GraphCanvasView::set_zoom(double zoom) {
    m_zoom = std::max(0.25, std::min(zoom, 4.0));
    Invalidate();
}

void GraphCanvasView::fit_to_window() {
    if (m_client_boxes.empty())
        return;

    double min_x = m_client_boxes[0].x;
    double min_y = m_client_boxes[0].y;
    double max_x = m_client_boxes[0].x + m_client_boxes[0].width;
    double max_y = m_client_boxes[0].y + m_client_boxes[0].height;

    for (const auto& box : m_client_boxes) {
        min_x = std::min(min_x, box.x);
        min_y = std::min(min_y, box.y);
        max_x = std::max(max_x, box.x + box.width);
        max_y = std::max(max_y, box.y + box.height);
    }

    double content_w = max_x - min_x;
    double content_h = max_y - min_y;
    if (content_w <= 0.0 || content_h <= 0.0)
        return;

    double view_w = Bounds().Width();
    double view_h = Bounds().Height();
    if (view_w <= 1 || view_h <= 1)
        return;

    constexpr double MARGIN = 40.0;
    double new_zoom =
        std::min((view_w - MARGIN * 2) / content_w, (view_h - MARGIN * 2) / content_h);
    m_zoom = std::max(0.25, std::min(new_zoom, 1.0));

    /* screen = canvas * zoom + pan  =>  pan = screen_center - canvas_center * zoom */
    m_pan_x = view_w / 2.0 - ((min_x + max_x) / 2.0) * m_zoom;
    m_pan_y = view_h / 2.0 - ((min_y + max_y) / 2.0) * m_zoom;

    Invalidate();
}

void GraphCanvasView::build_client_boxes() {
    m_client_boxes.clear();

    std::map<std::string, std::unique_ptr<ClientBox>> boxes;

    for (auto& node : m_nodes) {
        std::string cname = node->client_name;
        if (cname.empty())
            continue;

        if (boxes.find(cname) == boxes.end()) {
            boxes[cname] = std::make_unique<ClientBox>(cname);
        }
        boxes[cname]->add_port(node);
    }

    for (auto& [name, box] : boxes) {
        m_client_boxes.push_back(std::move(*box));
    }
}

void GraphCanvasView::layout(bool preserve_positions) {
    if (preserve_positions) {
        for (const auto& box : m_client_boxes) {
            m_saved_positions[box.client_name] = {box.x, box.y};
        }
    }

    build_client_boxes();

    /* Split boxes by signal role:
     *   sources — OUTPUT ports only  → left column  (capture, apps)
     *   sinks   — INPUT ports only   → right column (playback)
     *   mixed   — both directions    → middle column */
    std::vector<ClientBox*> sources, sinks, mixed;
    for (auto& box : m_client_boxes) {
        if (!box.outputs.empty() && box.inputs.empty())
            sources.push_back(&box);
        else if (box.outputs.empty() && !box.inputs.empty())
            sinks.push_back(&box);
        else
            mixed.push_back(&box);
    }

    /* Sort each column so MIDI clients appear above audio clients. */
    auto is_midi = [](const ClientBox* b) {
        for (const auto& p : b->inputs)
            if (p->type == PortType::MIDI)
                return true;
        for (const auto& p : b->outputs)
            if (p->type == PortType::MIDI)
                return true;
        return false;
    };
    auto midi_first = [&](const ClientBox* a, const ClientBox* b) {
        return is_midi(a) > is_midi(b);
    };
    std::stable_sort(sources.begin(), sources.end(), midi_first);
    std::stable_sort(sinks.begin(), sinks.end(), midi_first);
    std::stable_sort(mixed.begin(), mixed.end(), midi_first);

    double max_box_width = 0;
    for (const auto& box : m_client_boxes)
        max_box_width = std::max(max_box_width, box.width);

    double left_x = LAYOUT_MARGIN;
    double mid_x = LAYOUT_MARGIN + max_box_width + COL_GAP;
    double right_x = LAYOUT_MARGIN + max_box_width * 2 + COL_GAP * 2;

    auto position_ports = [&](ClientBox* box) {
        for (size_t i = 0; i < box->inputs.size(); ++i) {
            box->inputs[i]->x = box->x + ClientBox::SIDE_PAD;
            box->inputs[i]->y = box->y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD +
                                i * (ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD);
            box->inputs[i]->width = ClientBox::COL_WIDTH;
            box->inputs[i]->height = ClientBox::PORT_HEIGHT;
        }
        for (size_t i = 0; i < box->outputs.size(); ++i) {
            box->outputs[i]->x =
                box->x + ClientBox::SIDE_PAD + ClientBox::COL_WIDTH + ClientBox::SIDE_PAD;
            box->outputs[i]->y = box->y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD +
                                 i * (ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD);
            box->outputs[i]->width = ClientBox::COL_WIDTH;
            box->outputs[i]->height = ClientBox::PORT_HEIGHT;
        }
    };

    double left_y = LAYOUT_MARGIN;
    double mid_y = LAYOUT_MARGIN;
    double right_y = LAYOUT_MARGIN;

    /* Pass 1: restore saved boxes at their saved positions and find the
     * lowest occupied Y in each column so new boxes never overlap them. */
    auto place_saved = [&](ClientBox* box, double& col_floor) {
        auto it = m_saved_positions.find(box->client_name);
        if (it == m_saved_positions.end())
            return;
        box->x = it->second.x;
        box->y = it->second.y;
        position_ports(box);
        col_floor = std::max(col_floor, box->y + box->height + ROW_GAP);
    };

    for (auto* box : sources)
        place_saved(box, left_y);
    for (auto* box : mixed)
        place_saved(box, mid_y);
    for (auto* box : sinks)
        place_saved(box, right_y);

    /* Pass 2: place new (unsaved) boxes below all saved content in their column. */
    auto place_new = [&](ClientBox* box, double col_x, double& col_y) {
        if (m_saved_positions.count(box->client_name))
            return;
        box->x = col_x;
        box->y = col_y;
        position_ports(box);
        col_y += box->height + ROW_GAP;
    };

    for (auto* box : sources)
        place_new(box, left_x, left_y);
    for (auto* box : mixed)
        place_new(box, mid_x, mid_y);
    for (auto* box : sinks)
        place_new(box, right_x, right_y);

    Invalidate();
}

std::shared_ptr<Node> GraphCanvasView::find_output_port_at(double x, double y) {
    for (auto& box : m_client_boxes) {
        if (x >= box.x && x <= box.x + box.width && y >= box.y && y <= box.y + box.height) {
            double local_x = x - box.x;
            double local_y = y - box.y;
            return box.find_output_at(local_x, local_y);
        }
    }
    return nullptr;
}

std::shared_ptr<Node> GraphCanvasView::find_input_port_at(double x, double y) {
    for (auto& box : m_client_boxes) {
        if (x >= box.x && x <= box.x + box.width && y >= box.y && y <= box.y + box.height) {
            double local_x = x - box.x;
            double local_y = y - box.y;
            return box.find_input_at(local_x, local_y);
        }
    }
    return nullptr;
}

ClientBox* GraphCanvasView::find_box_at(double x, double y) {
    for (auto& box : m_client_boxes) {
        if (box.contains(x, y)) {
            return &box;
        }
    }
    return nullptr;
}

void GraphCanvasView::MouseDown(BPoint where) {
    int32 buttons = 0;
    if (Window() != NULL && Window()->CurrentMessage() != NULL)
        Window()->CurrentMessage()->FindInt32("buttons", &buttons);

    double x = (where.x - m_pan_x) / m_zoom;
    double y = (where.y - m_pan_y) / m_zoom;

    if (buttons & B_PRIMARY_MOUSE_BUTTON) {
        // Keep receiving MouseMoved/MouseUp while the drag leaves the view.
        SetMouseEventMask(B_POINTER_EVENTS, B_LOCK_WINDOW_FOCUS);

        auto output = find_output_port_at(x, y);
        if (output) {
            m_drag_source = output;
            m_drag_current_x = x;
            m_drag_current_y = y;
            m_is_dragging = true;
            return;
        }

        auto* box = find_box_at(x, y);
        if (box) {
            m_is_moving_box = true;
            m_moving_box = box;
            m_box_move_offset_x = x - box->x;
            m_box_move_offset_y = y - box->y;
            return;
        }

        m_is_panning = true;
        m_pan_start_x = where.x;
        m_pan_start_y = where.y;
        return;
    }

    if (buttons & B_SECONDARY_MOUSE_BUTTON) {
        for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
            auto& conn = *it;
            double x1 = conn->source->x + conn->source->width;
            double y1 = conn->source->y + conn->source->height / 2;
            double x2 = conn->destination->x;
            double y2 = conn->destination->y + conn->destination->height / 2;

            double num = std::abs((y2 - y1) * x - (x2 - x1) * y + x2 * y1 - y2 * x1);
            double den = std::sqrt((y2 - y1) * (y2 - y1) + (x2 - x1) * (x2 - x1));
            double dist = (den > 0) ? num / den : num;

            if (dist < 10 && x >= std::min(x1, x2) - 10 && x <= std::max(x1, x2) + 10) {
                if (m_disconnect_cb) {
                    m_disconnect_cb(conn->source->full_name(), conn->destination->full_name());
                }
                m_connections.erase(it);
                Invalidate();
                return;
            }
        }
    }
}

void GraphCanvasView::MouseUp(BPoint where) {
    if (m_is_dragging && m_drag_source) {
        double x = (where.x - m_pan_x) / m_zoom;
        double y = (where.y - m_pan_y) / m_zoom;

        auto target = find_input_port_at(x, y);
        if (target && target != m_drag_source) {
            if (m_connect_cb) {
                m_connect_cb(m_drag_source->full_name(), target->full_name());
            }

            auto conn = std::make_shared<Connection>(m_drag_source, target, m_drag_source->type);
            m_connections.push_back(conn);
        }

        m_is_dragging = false;
        m_drag_source = nullptr;
        Invalidate();
        return;
    }

    if (m_is_moving_box) {
        m_is_moving_box = false;
        m_moving_box = nullptr;
        return;
    }

    if (m_is_panning) {
        m_is_panning = false;
        return;
    }
}

void GraphCanvasView::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) {
    (void)code;
    (void)dragMessage;

    if (m_is_dragging) {
        m_drag_current_x = (where.x - m_pan_x) / m_zoom;
        m_drag_current_y = (where.y - m_pan_y) / m_zoom;
        Invalidate();
        return;
    }

    if (m_is_moving_box && m_moving_box) {
        double x = (where.x - m_pan_x) / m_zoom;
        double y = (where.y - m_pan_y) / m_zoom;
        m_moving_box->x = x - m_box_move_offset_x;
        m_moving_box->y = y - m_box_move_offset_y;

        for (size_t i = 0; i < m_moving_box->inputs.size(); ++i) {
            m_moving_box->inputs[i]->x = m_moving_box->x + ClientBox::SIDE_PAD;
            m_moving_box->inputs[i]->y =
                m_moving_box->y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD +
                i * (ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD);
        }
        for (size_t i = 0; i < m_moving_box->outputs.size(); ++i) {
            m_moving_box->outputs[i]->x =
                m_moving_box->x + ClientBox::SIDE_PAD + ClientBox::COL_WIDTH + ClientBox::SIDE_PAD;
            m_moving_box->outputs[i]->y =
                m_moving_box->y + ClientBox::HEADER_HEIGHT + ClientBox::PORT_PAD +
                i * (ClientBox::PORT_HEIGHT + ClientBox::PORT_PAD);
        }
        Invalidate();
        return;
    }

    if (m_is_panning) {
        m_pan_x += where.x - m_pan_start_x;
        m_pan_y += where.y - m_pan_start_y;
        m_pan_start_x = where.x;
        m_pan_start_y = where.y;
        Invalidate();
        return;
    }
}

void GraphCanvasView::MessageReceived(BMessage* message) {
    if (message->what == B_MOUSE_WHEEL_CHANGED) {
        float delta_y = 0;
        if (message->FindFloat("be:wheel_delta_y", &delta_y) == B_OK && delta_y != 0) {
            // Wheel up (negative delta) zooms in, matching the original.
            set_zoom(delta_y < 0 ? m_zoom * 1.1 : m_zoom / 1.1);
        }
        return;
    }
    BView::MessageReceived(message);
}
