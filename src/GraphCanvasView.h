#pragma once

#include <View.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Node.h"
#include "Connection.h"
#include "ClientBox.h"

class BBitmap;

// Patchbay canvas: clients as boxes, ports as connection points, bezier
// cables, drag-to-connect, zoom (wheel) and pan (background drag). The
// geometry, layout and hit-test logic is a direct port of the Linux
// jack-graph GraphCanvas; only the drawing/event plumbing is Interface Kit.
// All methods run on the owning window's looper thread.
class GraphCanvasView : public BView {
public:
    GraphCanvasView();
    ~GraphCanvasView() override;

    void AttachedToWindow() override;
    void DetachedFromWindow() override;
    void Draw(BRect updateRect) override;
    void FrameResized(float newWidth, float newHeight) override;
    void MouseDown(BPoint where) override;
    void MouseUp(BPoint where) override;
    void MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) override;
    void MessageReceived(BMessage* message) override;

    void add_node(std::shared_ptr<Node> node);
    void add_connection(std::shared_ptr<Connection> conn);
    void remove_all();

    void set_zoom(double zoom);
    // Zoom so the canvas point under view point `where` stays under it.
    void set_zoom_around(double zoom, BPoint where);
    double get_zoom() const { return m_zoom; }

    void layout(bool preserve_positions = false);
    void reset_layout();
    void fit_to_window();

    const std::vector<std::shared_ptr<Node>>& get_nodes() const { return m_nodes; }

    // Returns whether the connection was made; the cable is drawn only if it was.
    using ConnectCallback = std::function<bool(const std::string& source, const std::string& dest)>;
    void set_connect_callback(ConnectCallback cb) { m_connect_cb = std::move(cb); }

    using DisconnectCallback =
        std::function<void(const std::string& source, const std::string& dest)>;
    void set_disconnect_callback(DisconnectCallback cb) { m_disconnect_cb = std::move(cb); }

private:
    void build_client_boxes();

    void RebuildOffscreen();
    void DrawScene(BView* view);
    void DrawClientBox(BView* view, const ClientBox& box);
    void DrawPort(BView* view, const Node& node, double x, double y, bool is_output);
    void DrawConnectionLine(BView* view, const Connection& conn);
    void DrawDragPreview(BView* view);
    void DrawCable(BView* view, double x1, double y1, double x2, double y2, ::pattern pat);
    BPoint ToScreen(double x, double y) const;

    std::shared_ptr<Node> find_output_port_at(double x, double y);
    std::shared_ptr<Node> find_input_port_at(double x, double y);
    ClientBox* find_box_at(double x, double y);
    ClientBox* box_for(const std::string& client);
    int connection_at(double x, double y) const;
    static void position_ports(ClientBox* box);
    static bool compatible(const Node& src, const Node& dst);

    std::vector<std::shared_ptr<Node>> m_nodes;
    std::vector<std::shared_ptr<Connection>> m_connections;
    std::vector<ClientBox> m_client_boxes;

    struct SavedPosition {
        double x, y;
    };
    std::map<std::string, SavedPosition> m_saved_positions;

    double m_zoom;
    double m_pan_x;
    double m_pan_y;
    bool m_is_dragging;
    bool m_is_panning;
    bool m_is_moving_box;
    std::shared_ptr<Node> m_drag_source;
    // The box being dragged, held by client name and looked up on every move:
    // a graph refresh rebuilds m_client_boxes, so a pointer would dangle.
    std::string m_moving_box_client;
    double m_drag_current_x;
    double m_drag_current_y;
    double m_pan_start_x;
    double m_pan_start_y;
    double m_box_move_offset_x;
    double m_box_move_offset_y;
    bool m_did_initial_fit;

    ConnectCallback m_connect_cb;
    DisconnectCallback m_disconnect_cb;

    BBitmap* m_offscreen;
    BView* m_offscreen_view;

    static constexpr double LAYOUT_MARGIN = 20.0;
    static constexpr double PORT_RADIUS = 6;
    static constexpr double ROW_GAP = 10;
    static constexpr double COL_GAP = 250;
};
