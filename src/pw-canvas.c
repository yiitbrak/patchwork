#include "pw-canvas.h"
#include "pw-dummy.h"
#include "pw-pipewire.h"
#include "pw-node.h"
#include "pw-view-controller.h"
#include "pw-misc.h"

#define MAX_ZOOM 5.0
#define MIN_ZOOM 0.25
#define CANV_EXTRA 100 // units of allocation outside edge

struct _PwRubberband
{
  GtkWidget parent_instance;

  GtkAllocation al;
};

#define PW_TYPE_RUBBERBAND (pw_rubberband_get_type())

G_DECLARE_FINAL_TYPE (PwRubberband, pw_rubberband, RUBBERBAND, PW, GtkWidget)

G_DEFINE_FINAL_TYPE (PwRubberband, pw_rubberband, GTK_TYPE_WIDGET)

static PwRubberband *
pw_rubberband_new(void)
{
  return g_object_new(PW_TYPE_RUBBERBAND, NULL);;
}

static void
pw_rubberband_class_init (PwRubberbandClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  gtk_widget_class_set_css_name(widget_class, "rubberband");
}

static void
pw_rubberband_init (PwRubberband *self){}

typedef struct
{
  gdouble scale;
  gint dr_x, dr_y; // mouse ptr offsets in canvas units or link drag coordinates in screen units
  GtkWidget *dr_obj;
  GtkAdjustment *adj[2];
  GtkScrollablePolicy scroll_policy[2];
  gdouble zoom_gest_prev_scale;

  GObject *controller;
} PwCanvasPrivate;

G_DEFINE_TYPE_WITH_CODE (PwCanvas, pw_canvas, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL)
                         G_ADD_PRIVATE (PwCanvas))

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY,
  PROP_ZOOM,
  PROP_CONTROLLER,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

///////////////////////////////////////////////////////////
static void
pw_canvas_dispose(GObject *object);

static void
pw_canvas_finalize(GObject *object);

static void
pw_canvas_get_property(GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec);

static void
pw_canvas_set_property(GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec);

static GtkSizeRequestMode
pw_canvas_get_request_mode(GtkWidget *widget);

static void
pw_canvas_measure(GtkWidget      *widget,
                  GtkOrientation  orientation,
                  int             for_size,
                  int            *minimum,
                  int            *natural,
                  int            *minimum_baseline,
                  int            *natural_baseline);

static void
pw_canvas_size_allocate(GtkWidget *widget,
                        int        width,
                        int        height,
                        int        baseline);

static void
pw_canvas_snapshot(GtkWidget *widget, GtkSnapshot *snapshot);

static GdkContentProvider *
canvas_dnd_prepare(GtkDragSource *self,
                   gdouble        x,
                   gdouble        y,
                   gpointer       user_data);

static void
canvas_dnd_begin(GtkDragSource *self,
                 GdkDrag       *drag,
                 gpointer       user_data);

static void
canvas_dnd_end(GtkDragSource *self,
               GdkDrag       *drag,
               gboolean       delete_data,
               gpointer       user_data);

static void
canvas_dnd_cancel(GtkDragSource *self,
                  GdkDrag       *drag,
                  gboolean       delete_data,
                  gpointer       user_data);

static gboolean
canvas_dnd_drop(GtkDropTarget *self,
                const GValue  *value,
                gdouble        x,
                gdouble        y,
                gpointer       user_data);

static void
canvas_dnd_motion(GtkDropTarget *self,
                  gdouble        x,
                  gdouble        y,
                  gpointer       user_data);

static void
canvas_zgesture_begin(PwCanvas         *self,
                      GdkEventSequence *sequence,
                      GtkGesture       *gest);

static void
canvas_zgesture_scale_change(PwCanvas       *self,
                             gdouble         scale,
                             GtkGestureZoom *gest);

static void
canvas_drgesture_drag_begin(PwCanvas       *self,
                            gdouble         start_x,
                            gdouble         start_y,
                            GtkGestureDrag *gest);

static void
canvas_drgesture_drag_update(PwCanvas       *self,
                             gdouble         x_offset,
                             gdouble         y_offset,
                             GtkGestureDrag *gest);

static void
canvas_drgesture_drag_end(PwCanvas       *self,
                          gdouble         x_offset,
                          gdouble         y_offset,
                          GtkGestureDrag *gest);

static void
pipewire_changed_cb(GObject *object, gpointer user_data);

static void
get_curve_control_points(PwCanvas* self, PwLinkData* link, graphene_point_t* points);
///////////////////////////////////////////////////////////

PwCanvas *
pw_canvas_new(void)
{
  return g_object_new (PW_TYPE_CANVAS, NULL);
}

static void
pw_canvas_dispose(GObject *object)
{
  GtkWidget *self = GTK_WIDGET (object);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS (object));

  gtk_widget_dispose_template(self, PW_TYPE_CANVAS);

  g_clear_object (&priv->controller);

  G_OBJECT_CLASS (pw_canvas_parent_class)->dispose (object);
}

static void
pw_canvas_finalize(GObject *object)
{
  PwCanvas *self = (PwCanvas *)object;
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  G_OBJECT_CLASS (pw_canvas_parent_class)->finalize (object);
}

static void
set_scroll_policy(PwCanvas *self, GtkOrientation or, GtkScrollablePolicy pol)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);

  if(priv->scroll_policy[or] == pol)
    return;

  priv->scroll_policy[or] = pol;
  gtk_widget_queue_resize (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), (or == GTK_ORIENTATION_HORIZONTAL)
                            ? properties[PROP_HSCROLL_POLICY]:
                            properties[PROP_VSCROLL_POLICY]);
}

static void
canvas_adjustment_value_changed(GtkAdjustment *adj, gpointer data)
{
  gtk_widget_queue_allocate(GTK_WIDGET(data));
}

static void
canvas_remove_adjustment(PwCanvas       *self,
                         GtkOrientation  or)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GtkAdjustment* adj = priv->adj[or];

  if (adj) {
    g_signal_handlers_disconnect_by_func(adj, canvas_adjustment_value_changed, self);
    g_object_unref(adj);
    priv->adj[or]=NULL;
  }
}

static void
set_adjustment(PwCanvas* self, GtkOrientation or, GObject* obj)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GtkAdjustment* adj = GTK_ADJUSTMENT(obj);

  if(priv->adj[or] == adj)
    return;

  canvas_remove_adjustment(self, or);
  priv->adj[or] = adj;
  g_object_ref_sink(adj);

  g_signal_connect(adj, "value-changed",
                   G_CALLBACK(canvas_adjustment_value_changed), self);
  canvas_adjustment_value_changed(adj, self);
}

static void
set_controller(PwCanvas *self, PwViewController *control)
{
  g_return_if_fail (G_IS_OBJECT (control));
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  priv->controller = G_OBJECT (control);
}

static void
pw_canvas_get_property(GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  PwCanvasPrivate *self = pw_canvas_get_instance_private (PW_CANVAS (object));

  switch (prop_id){
  case PROP_HADJUSTMENT:
    g_value_set_object(value, self->adj[GTK_ORIENTATION_HORIZONTAL]);
    break;
  case PROP_VADJUSTMENT:
    g_value_set_object(value, self->adj[GTK_ORIENTATION_VERTICAL]);
    break;
  case PROP_HSCROLL_POLICY:
    g_value_set_enum(value, self->scroll_policy[GTK_ORIENTATION_HORIZONTAL]);
    break;
  case PROP_VSCROLL_POLICY:
    g_value_set_enum(value, self->scroll_policy[GTK_ORIENTATION_VERTICAL]);
    break;
  case PROP_ZOOM:
    g_value_set_double (value, self->scale);
    break;
  case PROP_CONTROLLER:
    g_value_set_object (value, self->controller);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
canvas_set_zoom(PwCanvas* self, gdouble zoom, graphene_point_t *anchor)
{
  GtkWidget *widget = GTK_WIDGET(self);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  if(!priv->adj[GTK_ORIENTATION_HORIZONTAL]||!priv->adj[GTK_ORIENTATION_VERTICAL]){
    goto end;
  }

  gdouble x_anchor, y_anchor, hval, vval;
  gdouble old_zoom = priv->scale;
  gint H = gtk_widget_get_width(widget);
  gint W = gtk_widget_get_height(widget);
  if(anchor){
    x_anchor = anchor->x;
    y_anchor = anchor->y;
  }else{
    x_anchor = W*0.5;
    y_anchor = H*0.5;
  }

  GtkAdjustment *hadj = priv->adj[GTK_ORIENTATION_HORIZONTAL];
  GtkAdjustment *vadj = priv->adj[GTK_ORIENTATION_VERTICAL];

  hval = gtk_adjustment_get_value(hadj);
  vval = gtk_adjustment_get_value(vadj);

  hval += x_anchor/old_zoom-x_anchor/zoom;
  vval += y_anchor/old_zoom-y_anchor/zoom;

  gtk_adjustment_set_value(hadj, hval);
  gtk_adjustment_set_value(vadj, vval);

end:
  priv->scale = zoom;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
  g_object_notify(G_OBJECT(self), "zoom");
}

static void
pw_canvas_set_property(GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  PwCanvas *self = PW_CANVAS (object);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  switch (prop_id){
  case PROP_HADJUSTMENT:
    set_adjustment(self, GTK_ORIENTATION_HORIZONTAL, g_value_get_object(value));
    break;
  case PROP_VADJUSTMENT:
    set_adjustment(self, GTK_ORIENTATION_VERTICAL, g_value_get_object(value));
    break;
  case PROP_HSCROLL_POLICY:
    set_scroll_policy(self,GTK_ORIENTATION_HORIZONTAL, g_value_get_enum(value));
    break;
  case PROP_VSCROLL_POLICY:
    set_scroll_policy(self,GTK_ORIENTATION_VERTICAL, g_value_get_enum(value));
    break;
  case PROP_ZOOM:
    canvas_set_zoom(self, g_value_get_double(value), NULL);
    break;
  case PROP_CONTROLLER:
    set_controller (self, g_value_get_object (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static GtkSizeRequestMode
pw_canvas_get_request_mode(GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
pw_canvas_measure(GtkWidget *widget, GtkOrientation orientation, int for_size,
                  int *minimum, int *natural, int *minimum_baseline,
                  int *natural_baseline)
{
  *minimum = 50;
  *natural = for_size < *minimum ? *minimum : for_size;
  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
allocate_node(GtkWidget *self, GtkWidget *child)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS (self));
  int x, y, w, h;
  PwNode *nod = PW_NODE (child);
  pw_node_get_pos (nod, &x, &y);
  int voffset = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_VERTICAL]);
  int hoffset = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_HORIZONTAL]);

  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w, NULL,
                      NULL);
  gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, NULL, &h, NULL,
                      NULL);

  GskTransform *tr = gsk_transform_new ();
  tr = gsk_transform_scale (tr, priv->scale, priv->scale);
  graphene_point_t pt = { .x = x-hoffset, .y = y-voffset };
  tr = gsk_transform_translate (tr, &pt);

  gtk_widget_allocate (child, w, h, -1, tr);
}

/*
 * Returns a rectangle that contains all nodes. In unscrolled screen (scaled) space
 */
static graphene_rect_t
canvas_get_node_bounds(PwCanvas* self)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GtkWidget *widget = GTK_WIDGET(self);
  gfloat xmin=G_MAXFLOAT,ymin=G_MAXFLOAT,xmax=G_MINFLOAT,ymax=G_MINFLOAT;

  GList *l = pw_view_controller_get_node_list(priv->controller);
  while(l){
    GtkWidget *child = GTK_WIDGET(l->data);
    PwNode* nod = PW_NODE(child);
    int X, Y;
    pw_node_get_pos(PW_NODE(child),&X, &Y);
    int width, height;
    gtk_widget_measure(child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &width, NULL, NULL);
    gtk_widget_measure(child, GTK_ORIENTATION_VERTICAL, -1, NULL, &height, NULL, NULL);

    xmin = MIN(xmin, X);
    ymin = MIN(ymin,Y);
    xmax = MAX(xmax, X+width);
    ymax = MAX(ymax, Y+height);

    l = l->next;
  }
  graphene_rect_t res = GRAPHENE_RECT_INIT(xmin, ymin, xmax, ymax);

  return res;
}

static void
canvas_configure_adj(PwCanvas        *self,
                     GtkOrientation   or,
                     graphene_rect_t  bounds,
                     int              length,
                     uint             extra_alloc)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GtkAdjustment* adj =
    (or==GTK_ORIENTATION_VERTICAL)?
    priv->adj[GTK_ORIENTATION_VERTICAL]:
    priv->adj[GTK_ORIENTATION_HORIZONTAL];

  if(!adj)
    return;

  gdouble old_lower = gtk_adjustment_get_lower(adj);
  gdouble old_upper = gtk_adjustment_get_upper(adj);
  gdouble old_value = gtk_adjustment_get_value(adj);

  gdouble value, lower, upper, step_inc, page_inc, page_size;

  value = old_value;
  page_size = length/priv->scale;
  step_inc = 10;
  page_inc = 0;
  lower = (or==GTK_ORIENTATION_VERTICAL)?bounds.origin.y:bounds.origin.x;
  upper = (or==GTK_ORIENTATION_VERTICAL)?bounds.size.height:bounds.size.width;

  if(priv->dr_obj){
    // do not shrink if there's an ongoing DnD
    lower = MIN(old_lower,lower-extra_alloc);
    upper = MAX(old_upper,upper+extra_alloc);
  }else{
    lower = MIN(value,lower-extra_alloc);
    upper = MAX(value+page_size,upper+extra_alloc);
  }
  gtk_adjustment_configure(adj, value, lower, upper, step_inc, page_inc, page_size);
}

static bool
check_link_rubberband_selection(PwCanvas* self, PwRubberband* rb, PwLinkData* link)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GList *links = pw_view_controller_get_link_list (priv->controller);
  graphene_rect_t rb_al;
  if(!gtk_widget_compute_bounds (GTK_WIDGET (rb), GTK_WIDGET (self), &rb_al)){
    return false;
  }

  /* 
   *  l1---l2
   *  |    |
   *  l4---l3
   */
  graphene_point_t l1 = { rb_al.origin.x, rb_al.origin.y };
  graphene_point_t l2 = { rb_al.origin.x + rb_al.size.width, rb_al.origin.y };
  graphene_point_t l3 = { rb_al.origin.x + rb_al.size.width, rb_al.origin.y + rb_al.size.height };
  graphene_point_t l4 = { rb_al.origin.x, rb_al.origin.y + rb_al.size.height };

  graphene_point_t cpts[4];
  get_curve_control_points(self, link, cpts);
  
  bool is_intersecting =
    cbezier_line_intersects(l1, l2, cpts[0], cpts[1], cpts[2], cpts[3])||
    cbezier_line_intersects(l2, l3, cpts[0], cpts[1], cpts[2], cpts[3])||
    cbezier_line_intersects(l4, l3, cpts[0], cpts[1], cpts[2], cpts[3])||
    cbezier_line_intersects(l1, l4, cpts[0], cpts[1], cpts[2], cpts[3]);
  
  return is_intersecting;
}

static bool
check_curve_bounding_box_rubberband(PwCanvas* self, PwRubberband* rb, PwLinkData* link)
{
  graphene_point_t pts[4];
  get_curve_control_points(self, link, pts);
  graphene_rect_t curve_box = get_cbezier_bounding_box(pts[0], pts[1], pts[2], pts[3]);

  graphene_rect_t al;
  if(!gtk_widget_compute_bounds (GTK_WIDGET (rb), GTK_WIDGET (self), &al)){
    return false;
  }

  return rect_contains_rect(al, curve_box);
}

static void
check_link_selection(PwCanvas* self, PwRubberband* rb)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GList *links = pw_view_controller_get_link_list (priv->controller);

  while(links){
    PwLinkData* link = links->data;

    bool is_selected = check_link_rubberband_selection(self, rb, link) || check_curve_bounding_box_rubberband(self, rb, link);

    link->selected = is_selected;
    links = links->next;
  }
}

static void
pw_canvas_size_allocate(GtkWidget *widget, int width, int height,
                        int baseline)
{
  PwCanvas* self = PW_CANVAS(widget);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);

  graphene_rect_t bounds = canvas_get_node_bounds(self);
  canvas_configure_adj(self, GTK_ORIENTATION_HORIZONTAL, bounds, width, CANV_EXTRA);
  canvas_configure_adj(self, GTK_ORIENTATION_VERTICAL, bounds, height, CANV_EXTRA);

  GList *list = pw_view_controller_get_node_list(priv->controller);
  while(list){
    allocate_node (widget, GTK_WIDGET(list->data));
    list = list->next;
  }

  PwRubberband *rb = g_object_get_data(G_OBJECT(self), "rubberband");
  if(rb){
    gtk_widget_size_allocate(GTK_WIDGET(rb), &rb->al, -1);
  } else return;
  check_link_selection(self, rb);

}

static void
snapshot_bg(GtkWidget *widget, GtkSnapshot *snapshot)
{
  AdwStyleManager *style = adw_style_manager_get_default ();
  gboolean is_dark = adw_style_manager_get_dark (style);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS (widget));
  graphene_rect_t al;
  gboolean success = gtk_widget_compute_bounds(widget, widget, &al);
  graphene_rect_t alloc
      = { .origin = { 0, 0 }, .size = { al.size.width, al.size.height } };
  graphene_rect_t grid_cell;

  gtk_snapshot_push_repeat (snapshot, &alloc, NULL);

  float val = 0.5 + (is_dark ? -1 : 1) * 0.25;
  const GdkRGBA color = { val, val, val, 1.0f };


  gdouble scale = priv->scale;
  gint vval = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_VERTICAL]);
  gint hval = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_HORIZONTAL]);
  const int GRID_SIZE = 25;
  gint len = GRID_SIZE * scale;
  hval= ((int)((GRID_SIZE- hval%GRID_SIZE)*scale)%len) ;
  vval= ((int)((GRID_SIZE- vval%GRID_SIZE)*scale)%len) ;
  //vertical line
  grid_cell = GRAPHENE_RECT_INIT ((int)hval, 0, 1, len);
  gtk_snapshot_append_color (snapshot, &color, &grid_cell);
  //horizontal line
  grid_cell = GRAPHENE_RECT_INIT (0, (int)vval, len, 1);
  gtk_snapshot_append_color (snapshot, &color, &grid_cell);

  gtk_snapshot_pop (snapshot);
}

static void
snapshot_nodes(GtkWidget *widget, GtkSnapshot *snapshot)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS(widget));
  PwViewControllerInterface *iface = PW_VIEW_CONTROLLER_GET_IFACE (priv->controller);

  GList* nodes = iface->get_node_list(priv->controller);

  while (nodes){
    gtk_widget_snapshot_child (widget, GTK_WIDGET(nodes->data), snapshot);
    nodes = nodes->next;
  }
}

static void
draw_single_link(PwCanvas* canv, cairo_t* cr, PwLinkData* link)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);
  AdwStyleManager *style = adw_style_manager_get_default ();
  gboolean is_dark = adw_style_manager_get_dark (style);
  GdkRGBA* accent_col = adw_style_manager_get_accent_color_rgba(style);

  graphene_point_t cpts[4];
  get_curve_control_points(canv, link, cpts);

  gint col = (is_dark?1:0);
  if(link->selected){
    cairo_set_source_rgba(cr, accent_col->red, accent_col->green, accent_col->blue, 1.0);
  }else{
    cairo_set_source_rgba(cr, col, col, col, 0.6);
  }
  cairo_set_line_width(cr, 2*priv->scale);

  cairo_move_to(cr, cpts[0].x, cpts[0].y);
  cairo_curve_to(cr, cpts[1].x, cpts[1].y, cpts[2].x, cpts[2].y, cpts[3].x, cpts[3].y);
  cairo_stroke(cr);
}

static void
draw_dragged_link(PwCanvas* canv, cairo_t* cr)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);
  PwPad* dr = PW_PAD(priv->dr_obj);
  gboolean is_out = pw_pad_get_direction(dr) == PW_PAD_DIRECTION_OUT;
  int voffset = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_VERTICAL]);
  int hoffset = gtk_adjustment_get_value(priv->adj[GTK_ORIENTATION_HORIZONTAL]);

  graphene_rect_t rect;
  gboolean res = gtk_widget_compute_bounds(GTK_WIDGET(dr), GTK_WIDGET(canv), &rect);

  int x1, y1, x2, y2, mid1, mid2;

  if(is_out){
    x1 = rect.origin.x + rect.size.width;
    y1 = rect.origin.y + rect.size.height/2;
    x2 = priv->dr_x;
    y2 = priv->dr_y;
    mid1 = x1+abs(x2-x1)/2;
    mid2 = x2+(x1<x2?-1:1)*abs(x2-x1)/2;
  }else{
    x1 = priv->dr_x;
    y1 = priv->dr_y;
    x2 = rect.origin.x;
    y2 = rect.origin.y + rect.size.height/2;
    mid1 = x1+(x1<x2?1:-1)*abs(x2-x1)/2;
    mid2 = x2-abs(x2-x1)/2;
  }

  gint col = (1);
  cairo_set_source_rgba(cr, col, col, col, 0.6);
  cairo_set_line_width(cr, 2*priv->scale);

  cairo_move_to(cr, x1, y1);
  cairo_curve_to(cr, mid1, y1, mid2, y2, x2, y2);
  cairo_stroke(cr);
}

static void
snapshot_links(GtkWidget* widget, GtkSnapshot* snapshot)
{
  PwCanvas* canv = PW_CANVAS(widget);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);
  GList* link = pw_view_controller_get_link_list(priv->controller);
  graphene_rect_t al;
  gboolean success = gtk_widget_compute_bounds(widget, widget, &al);
  graphene_rect_t canv_rect = GRAPHENE_RECT_INIT(0, 0, al.size.width, al.size.height);

  // TODO: test out if single cairo node approach is actually faster than spearate nodes
  cairo_t* cai =gtk_snapshot_append_cairo(snapshot, &canv_rect);

  if(priv->dr_obj && PW_IS_PAD(priv->dr_obj)){
    draw_dragged_link(canv, cai);
  }

  while(link){
    PwLinkData* ldat = link->data;
    draw_single_link(canv, cai, link->data);
    link=link->next;
  }
  cairo_destroy(cai);
}

// ugly, hard to read and probably commits multiple warcrimes
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void
curve_collision_debug (PwCanvas* canv, GtkWidget* rb, GtkSnapshot *snapshot)
{
  graphene_rect_t canv_bounds;
  if(!gtk_widget_compute_bounds (GTK_WIDGET (canv), GTK_WIDGET (canv), &canv_bounds)){
    return;
  }
  cairo_t *cai = gtk_snapshot_append_cairo (snapshot, &canv_bounds);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);

  GList *links = pw_view_controller_get_link_list (priv->controller);
  graphene_rect_t rb_al;
  if(!gtk_widget_compute_bounds (GTK_WIDGET (rb), GTK_WIDGET (canv), &rb_al)){
    return;
  }
  graphene_point_t l1 = { rb_al.origin.x, rb_al.origin.y };
  graphene_point_t l2 = { rb_al.origin.x, rb_al.origin.y + rb_al.size.height };
  graphene_point_t l3 = { rb_al.origin.x + rb_al.size.width, rb_al.origin.y };
  graphene_point_t lbweh = l2;

  while (links)
  {
    PwLinkData *link = links->data;
    
    graphene_point_t cpts[4];
    get_curve_control_points(canv, link, cpts);
    bool redo_once = true;
    double roots[3];
redo_align:
    graphene_point_t aligned_pts[4] = { cpts[0], cpts[1], cpts[2], cpts[3] };
    align_curve(aligned_pts, l1, lbweh);
    get_cubic_roots (aligned_pts[0].y, aligned_pts[1].y, aligned_pts[2].y, aligned_pts[3].y, roots);

    for (int i = 0; i < 3; i++)
    {
      //printf("%f\n", roots[i]);
      if (roots[i] != NAN && (0 < roots[i]) && (roots[i] < 1))
      {
        graphene_point_t r = curve_get_point (cpts[0], cpts[1], cpts[2], cpts[3], roots[i]);
        if(rb_al.size.width > 500){
          printf(" ");}
        if ((r.y >= l1.y && lbweh.y >= r.y) && (r.x >= l1.x && lbweh.x >= r.x))
        {
          cairo_set_source_rgba (cai, 0.0, 1.0, 0.0, 0.6);
        } else
        {
          cairo_set_source_rgba (cai, 1.0, 0.0, 0.0, 0.6);
        }
        cairo_set_line_width (cai, 10);
        cairo_set_line_cap (cai, CAIRO_LINE_CAP_ROUND);
        cairo_move_to (cai, r.x - 0.5, r.y - 0.5);
        cairo_line_to (cai, r.x + 0.5, r.y + 0.5);
        cairo_stroke (cai);
      }
    }
    if(redo_once)
    {
      redo_once = false;
      lbweh = l3;
      goto redo_align;
    } else lbweh = l2;
    
    if(rect_contains_rect(rb_al ,get_cbezier_bounding_box(cpts[0],cpts[1],cpts[2],cpts[3])))
    {
      cairo_set_source_rgba (cai, 0.0, 1.0, 0.0, 0.5);
    } else
    {
      cairo_set_source_rgba (cai, 1.0, 0.0, 0.0, 0.5);
    }
    
    cairo_set_line_width(cai, 1);
    graphene_rect_t bounding_box = get_cbezier_bounding_box(cpts[0], cpts[1], cpts[2], cpts[3]);
    cairo_rectangle(cai, bounding_box.origin.x, bounding_box.origin.y, bounding_box.size.width, bounding_box.size.height);
    cairo_stroke(cai);
    links = links->next;
  }
  cairo_destroy(cai);
}
#pragma GCC diagnostic pop

static void
snapshot_rubberband(GtkWidget* widget, GtkSnapshot* snapshot)
{
  PwCanvas* canv = PW_CANVAS(widget);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);

  GtkWidget *rb = g_object_get_data(G_OBJECT(canv), "rubberband");

  if (rb){
    gtk_widget_snapshot_child (widget, rb, snapshot);
  } else return;
  // curve_collision_debug(canv, rb, snapshot);
}

static void
pw_canvas_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
  snapshot_bg (widget, snapshot);
  snapshot_nodes (widget, snapshot);
  snapshot_links (widget, snapshot);
  snapshot_rubberband(widget, snapshot);
}

static void
pw_canvas_class_init(PwCanvasClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pw_canvas_dispose;
  object_class->finalize = pw_canvas_finalize;
  object_class->get_property = pw_canvas_get_property;
  object_class->set_property = pw_canvas_set_property;
  widget_class->get_request_mode = pw_canvas_get_request_mode;
  widget_class->measure = pw_canvas_measure;
  widget_class->size_allocate = pw_canvas_size_allocate;
  widget_class->snapshot = pw_canvas_snapshot;

  // Implmeent GtkScrollable
  gpointer iface = g_type_default_interface_peek (GTK_TYPE_SCROLLABLE);
  properties[PROP_HADJUSTMENT] = g_param_spec_override ("hadjustment", g_object_interface_find_property (iface, "hadjustment"));
  properties[PROP_VADJUSTMENT] = g_param_spec_override ("vadjustment", g_object_interface_find_property (iface, "vadjustment"));
  properties[PROP_HSCROLL_POLICY] = g_param_spec_override ("hscroll-policy", g_object_interface_find_property (iface, "hscroll-policy"));
  properties[PROP_VSCROLL_POLICY] = g_param_spec_override ("vscroll-policy", g_object_interface_find_property (iface, "vscroll-policy"));

  properties[PROP_ZOOM]
      = g_param_spec_double ("zoom", "Zoom", "Zoom/scale of the canvas", MIN_ZOOM,
                             MAX_ZOOM, 1, G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_EXPLICIT_NOTIFY);
  properties[PROP_CONTROLLER] = g_param_spec_object (
      "controller", "Controller", "Driver of the canvas", G_TYPE_OBJECT,
      G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  g_type_ensure(PW_TYPE_NODE); // for GtkDropTarget's format
  gtk_widget_class_set_template_from_resource(widget_class, "/org/nidi/patchwork/res/ui/pw-canvas.ui");

// signals
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_prepare);
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_begin);
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_end);
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_cancel);
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_drop);
  gtk_widget_class_bind_template_callback(widget_class, canvas_dnd_motion);
  gtk_widget_class_bind_template_callback(widget_class, canvas_zgesture_begin);
  gtk_widget_class_bind_template_callback(widget_class, canvas_zgesture_scale_change);
  gtk_widget_class_bind_template_callback(widget_class, canvas_drgesture_drag_begin);
  gtk_widget_class_bind_template_callback(widget_class, canvas_drgesture_drag_update);
  gtk_widget_class_bind_template_callback(widget_class, canvas_drgesture_drag_end);

  gtk_widget_class_set_css_name (widget_class, "canvas");
}

static GdkContentProvider *
canvas_dnd_prepare(GtkDragSource *self,
                   gdouble        x,
                   gdouble        y,
                   gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  GtkWidget *widget = GTK_WIDGET (canv);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  GtkWidget *pick = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);
  GtkWidget *ancestor;
  if((ancestor = gtk_widget_get_ancestor(pick, PW_TYPE_PAD))){
    priv->dr_x = x;
    priv->dr_y = y;
    priv->dr_obj = ancestor;

    return gdk_content_provider_new_typed (PW_TYPE_PAD, ancestor);
  }

  if((ancestor = gtk_widget_get_ancestor(pick, PW_TYPE_NODE))){
    int nx, ny;
    pw_node_get_pos (PW_NODE (ancestor), &nx, &ny);
    priv->dr_x = (x / priv->scale) - nx;
    priv->dr_y = (y / priv->scale) - ny;
    priv->dr_obj = ancestor;
    return gdk_content_provider_new_typed (PW_TYPE_NODE, ancestor);
  }

  return NULL;
}

static void
canvas_dnd_begin(GtkDragSource *self,
                 GdkDrag       *drag,
                 gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);
  GdkPaintable* empty_icon = gdk_paintable_new_empty(0,0);
  gtk_drag_source_set_icon (self, empty_icon, 0, 0);
  g_object_unref(empty_icon);

  if(PW_IS_NODE (priv->dr_obj)){
    PwNode* nod = PW_NODE(priv->dr_obj);
    pw_view_controller_node_to_front(priv->controller, pw_node_get_id(nod));
  }
}

static void
canvas_dnd_end(GtkDragSource *self,
               GdkDrag       *drag,
               gboolean       delete_data,
               gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  if(PW_IS_NODE (priv->dr_obj)){
    gdk_drag_set_hotspot (drag, 0, 0);
    priv->dr_obj = NULL;
  }else if(PW_IS_PAD(priv->dr_obj)){
    priv->dr_obj = NULL;
    gtk_widget_queue_draw(GTK_WIDGET(canv));
  }
}

static void
canvas_dnd_cancel(GtkDragSource *self,
                  GdkDrag       *drag,
                  gboolean       delete_data,
                  gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  if(PW_IS_NODE (priv->dr_obj)){
    gdk_drag_set_hotspot (drag, priv->dr_x, priv->dr_y);
    priv->dr_obj = NULL;
  }
}

static gboolean
canvas_dnd_drop(GtkDropTarget *self,
                const GValue  *value,
                gdouble        x,
                gdouble        y,
                gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  PwNode *nod = PW_NODE (g_value_get_object (value));

  pw_node_set_xpos (nod, (x / priv->scale) - priv->dr_x);
  pw_node_set_ypos (nod, (y / priv->scale) - priv->dr_y);
  gtk_widget_insert_before(GTK_WIDGET(nod), GTK_WIDGET(canv), NULL);
  priv->dr_obj = NULL;

  return TRUE;
}

static void
canvas_dnd_motion(GtkDropTarget *self,
                  gdouble        x,
                  gdouble        y,
                  gpointer       user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);


  if(PW_IS_NODE(priv->dr_obj)){
    PwNode *nod = PW_NODE (priv->dr_obj);
    pw_node_set_xpos (nod, (x / priv->scale) - priv->dr_x);
    pw_node_set_ypos (nod, (y / priv->scale) - priv->dr_y);
  }else if(PW_IS_PAD(priv->dr_obj)){
    priv->dr_x = x;
    priv->dr_y = y;
    gtk_widget_queue_draw(GTK_WIDGET(canv));
  }
}

static void
canvas_zgesture_begin(PwCanvas         *self,
                      GdkEventSequence *sequence,
                      GtkGesture       *gest)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private(self);
  priv->zoom_gest_prev_scale = 1.0;
}

static void
canvas_zgesture_scale_change(PwCanvas       *self,
                             gdouble         scale,
                             GtkGestureZoom *gest)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private(self);
  gdouble X,Y;
  gtk_gesture_get_bounding_box_center(GTK_GESTURE(gest), &X, &Y);
  graphene_point_t pt = {X,Y};

  gdouble delta = priv->zoom_gest_prev_scale - scale;
  canvas_set_zoom(self, pw_canvas_get_zoom(self) - delta, &pt);
  priv->zoom_gest_prev_scale = scale;
}

static void
drgesture_update_allocation(GtkAllocation *al,
                            int            x_start,
                            int            y_start,
                            int            x_offset,
                            int            y_offset)
{
  if(x_offset<0){
    al->x = x_start + x_offset;
    al->width = -x_offset;
  }else{
    al->x = x_start;
    al->width = x_offset;
  }

  if(y_offset<0){
    al->y = y_start + y_offset;
    al->height = -y_offset;
  }else{
    al->y = y_start;
    al->height = y_offset;
  }
}

static void
canvas_drgesture_drag_begin(PwCanvas       *self,
                            gdouble         start_x,
                            gdouble         start_y,
                            GtkGestureDrag *gest)
{
  PwRubberband *rb = pw_rubberband_new();

  gtk_widget_set_parent(GTK_WIDGET(rb), GTK_WIDGET(self));

  drgesture_update_allocation(&rb->al, start_y, start_x, 0, 0);

  gtk_widget_queue_allocate(GTK_WIDGET(self));
  g_object_set_data(G_OBJECT(self), "rubberband", rb);
}

static void
canvas_drgesture_drag_update(PwCanvas       *self,
                             gdouble         x_offset,
                             gdouble         y_offset,
                             GtkGestureDrag *gest)
{
  PwRubberband *rb = g_object_get_data(G_OBJECT(self), "rubberband");

  double x,y;
  gtk_gesture_drag_get_start_point(gest, &x, &y);
  drgesture_update_allocation(&rb->al, x, y, x_offset, y_offset);

  gtk_widget_queue_allocate(GTK_WIDGET(self));
}

static void
canvas_drgesture_drag_end(PwCanvas       *self,
                          gdouble         x_offset,
                          gdouble         y_offset,
                          GtkGestureDrag *gest)
{
  GtkWidget *rb = g_object_get_data(G_OBJECT(self), "rubberband");

  gtk_widget_unparent(rb);
  g_object_set_data(G_OBJECT(self), "rubberband", NULL);
}

static void
pipewire_changed_cb(GObject *object, gpointer user_data)
{
  PwCanvas* self = PW_CANVAS(user_data);
  gtk_widget_queue_allocate(GTK_WIDGET(self));
}

static void
get_curve_control_points(PwCanvas* self, PwLinkData* link, graphene_point_t* points)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);

  PwPad *p1 = pw_view_controller_get_pad_by_id(G_OBJECT (priv->controller), link->out);
  PwPad *p2 = pw_view_controller_get_pad_by_id(G_OBJECT (priv->controller), link->in);
  graphene_rect_t rect1, rect2;
  gboolean res = gtk_widget_compute_bounds(GTK_WIDGET (p1),GTK_WIDGET (self), &rect1);
  res &= gtk_widget_compute_bounds (GTK_WIDGET (p2), GTK_WIDGET (self), &rect2);
  if (!res)
  {
    g_warning ("Bounds checking failed");
  }
  int x1, x2, y1, y2, ydiff, xdiff;
  x1 = rect1.origin.x + rect1.size.width;
  y1 = rect1.origin.y + rect1.size.height / 2;
  x2 = rect2.origin.x;
  y2 = rect2.origin.y + rect2.size.height / 2;
  ydiff = ABS (y1 - y2);
  xdiff = ABS (x1 - x2);

  points[0] = GRAPHENE_POINT_INIT(x1, y1);
  points[1] = GRAPHENE_POINT_INIT(x1 + xdiff / 2 + ydiff / 4, y1);
  points[2] = GRAPHENE_POINT_INIT(x2 - 10 - xdiff / 2 - ydiff / 4, y2);
  points[3] = GRAPHENE_POINT_INIT(x2, y2);
}

static void
pw_canvas_init(PwCanvas *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);
  PwPipewire *con = pw_pipewire_new (self);
  priv->controller = G_OBJECT (con);

  gtk_widget_init_template(widget);
  g_object_set(gtk_widget_get_settings(widget), "gtk-dnd-drag-threshold" , 1, NULL);

  g_signal_connect(con, "changed", G_CALLBACK(pipewire_changed_cb), self);
  pw_pipewire_run(con);
}

gdouble
pw_canvas_get_zoom(PwCanvas *self)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);
  return priv->scale;
}

void
pw_canvas_set_zoom(PwCanvas *self, gdouble zoom)
{
  g_object_set(self, "zoom", MIN(MAX_ZOOM, MAX(MIN_ZOOM, zoom)) ,NULL);
}
