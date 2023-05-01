#include "pw-canvas.h"
#include "pw-dummy.h"
#include "pw-pipewire.h"
#include "pw-node.h"
#include "pw-view-controller.h"

typedef struct
{
  gdouble scale;
  gint dr_x, dr_y; // mouse ptr offsets in canvas units
  GtkWidget *dr_obj;
  GtkAdjustment* adj[2];
  GtkScrollablePolicy scroll_policy[2];

  GObject *controller;
  GtkDragSource *dr_src;
  GtkDropTarget *dr_tgt;
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
static void pw_canvas_finalize (GObject *object);

static void pw_canvas_get_property (GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);

static void pw_canvas_set_property (GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);

static GtkSizeRequestMode pw_canvas_get_request_mode (GtkWidget *widget);

static void pw_canvas_measure (GtkWidget *widget, GtkOrientation orientation,
                               int for_size, int *minimum, int *natural,
                               int *minimum_baseline, int *natural_baseline);

static void pw_canvas_size_allocate (GtkWidget *widget, int width, int height,
                                     int baseline);

static void pw_canvas_snapshot (GtkWidget *widget, GtkSnapshot *snapshot);
///////////////////////////////////////////////////////////

PwCanvas *
pw_canvas_new (void)
{
  return g_object_new (PW_TYPE_CANVAS, NULL);
}

static void
pw_canvas_dispose (GObject *object)
{
  GtkWidget *self = GTK_WIDGET (object);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS (object));
  GtkWidget *child = gtk_widget_get_first_child (self);

  g_clear_object (&priv->controller);

  G_OBJECT_CLASS (pw_canvas_parent_class)->dispose (object);
}

static void
pw_canvas_finalize (GObject *object)
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

static void canvas_adjustment_value_changed(GtkAdjustment* adj, gpointer data) {
  gtk_widget_queue_allocate(GTK_WIDGET(data));
}

static void canvas_remove_adjustment(PwCanvas       *self,
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
  printf("floatin: %d\n",g_object_is_floating(adj));
  g_object_ref_sink(adj);

  g_signal_connect(adj, "value-changed",
                   G_CALLBACK(canvas_adjustment_value_changed), self);
  canvas_adjustment_value_changed(adj, self);
}

static void
set_controller (PwCanvas *self, PwViewController *control)
{
  g_return_if_fail (G_IS_OBJECT (control));
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  priv->controller = G_OBJECT (control);
}

static void
pw_canvas_get_property (GObject *object, guint prop_id, GValue *value,
                        GParamSpec *pspec)
{
  PwCanvasPrivate *self = pw_canvas_get_instance_private (PW_CANVAS (object));

  switch (prop_id)
    {
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
set_zoom(PwCanvas* self, gdouble zoom)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);
  priv->scale = zoom;
  gtk_widget_queue_allocate (GTK_WIDGET (self));
}

static void
pw_canvas_set_property (GObject *object, guint prop_id, const GValue *value,
                        GParamSpec *pspec)
{
  PwCanvas *self = PW_CANVAS (object);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  switch (prop_id)
    {
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
      set_zoom(self, g_value_get_double(value));
      break;
    case PROP_CONTROLLER:
      set_controller (self, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
pw_canvas_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
pw_canvas_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                   int *minimum, int *natural, int *minimum_baseline,
                   int *natural_baseline)
{
  *minimum = 50;
  *natural = for_size < *minimum ? *minimum : for_size;
  *minimum_baseline = -1;
  *natural_baseline = -1;
}

static void
allocate_node (GtkWidget *self, GtkWidget *child)
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
canvas_get_node_bounds (PwCanvas* self)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  gfloat xmin=G_MAXFLOAT,ymin=G_MAXFLOAT,xmax=G_MINFLOAT,ymax=G_MINFLOAT;

  GtkWidget* child = gtk_widget_get_first_child(GTK_WIDGET(self));
  while(child){
    PwNode* nod = PW_NODE(child);
    GtkAllocation al;
    gtk_widget_get_allocation(child, &al);

    xmin = MIN(xmin,al.x);
    ymin = MIN(ymin,al.y);
    xmax = MAX(xmax, al.x+al.width);
    ymax = MAX(ymax, al.y+al.height);

    child = gtk_widget_get_next_sibling(child);
  }
  graphene_rect_t res = GRAPHENE_RECT_INIT(xmin, ymin, xmax, ymax);

  return res;
}

static void
canvas_configure_adj(PwCanvas* self, GtkOrientation or,graphene_rect_t bounds, int length)
{
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(self);
  GtkAdjustment* adj = (or==GTK_ORIENTATION_VERTICAL)?priv->adj[GTK_ORIENTATION_VERTICAL]:priv->adj[GTK_ORIENTATION_HORIZONTAL];
  if(!adj)
    return;
  gdouble value, lower, upper, step_inc, page_inc, page_size;
  gdouble old_value = gtk_adjustment_get_value(adj);

  value = old_value;

  lower = 0;//(or==GTK_ORIENTATION_VERTICAL)?bounds.origin.y:bounds.origin.x;
  upper = 10000;//(or==GTK_ORIENTATION_VERTICAL)?bounds.size.height:bounds.size.width;
  step_inc = 0;
  page_inc = 0;
  page_size = length;


  gtk_adjustment_configure(adj, value, lower, upper, step_inc, page_inc, page_size);
}

static void
pw_canvas_size_allocate (GtkWidget *widget, int width, int height,
                         int baseline)
{
  PwCanvas* self = PW_CANVAS(widget);

  graphene_rect_t bounds = canvas_get_node_bounds(self);
  canvas_configure_adj(self, GTK_ORIENTATION_HORIZONTAL, bounds, width);
  canvas_configure_adj(self, GTK_ORIENTATION_VERTICAL, bounds, height);

    GtkWidget *child = gtk_widget_get_first_child (widget);
  while (child){
      if (PW_IS_NODE (child)){
        allocate_node (widget, child);
      }
      else{
        child = gtk_widget_get_next_sibling (widget);
        g_warning("Non-node widget on canvas\n");
      }

      child = gtk_widget_get_next_sibling (child);
    }
}

static void
snapshot_bg (GtkWidget *widget, GtkSnapshot *snapshot)
{
  AdwStyleManager *style = adw_style_manager_get_default ();
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS (widget));
  GtkAllocation alt;
  gtk_widget_get_allocation (widget, &alt);
  graphene_rect_t alloc
      = { .origin = { 0, 0 }, .size = { alt.width, alt.height } };
  graphene_rect_t grid_cell;

  gtk_snapshot_push_repeat (snapshot, &alloc, NULL);

  float val = 0.5 + (adw_style_manager_get_dark (style) ? -1 : 1) * 0.25;
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
snapshot_nodes (GtkWidget *widget, GtkSnapshot *snapshot)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (PW_CANVAS(widget));
  PwViewControllerInterface *iface = PW_VIEW_CONTROLLER_GET_IFACE (priv->controller);

  GList* nodes = iface->get_node_list(priv->controller);

  while (nodes)
    {
      gtk_widget_snapshot_child (widget, GTK_WIDGET(nodes->data), snapshot);

      nodes = nodes->next;
    }
}

static void
draw_single_link(GtkWidget* widget, cairo_t* cr, PwLinkData* dat)
{
  PwCanvas* canv = PW_CANVAS(widget);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);

  PwPad* p1 = pw_view_controller_get_pad_by_id(G_OBJECT(priv->controller), dat->in);
  PwPad* p2 = pw_view_controller_get_pad_by_id(G_OBJECT(priv->controller), dat->out);
  graphene_rect_t rect1,rect2;
  gboolean res = gtk_widget_compute_bounds(GTK_WIDGET(p1), GTK_WIDGET(canv), &rect1);
  res &= gtk_widget_compute_bounds(GTK_WIDGET(p2), GTK_WIDGET(canv), &rect2);
  if(!res)
    {g_log("Patchwork" , G_LOG_LEVEL_WARNING , "Bounds checking failed.");}

  int x1,x2,y1,y2, xmid, ymid;

  x2 = rect2.origin.x + rect2.size.width;
  y2 = rect2.origin.y + rect2.size.height/2;

  x1 = rect1.origin.x;
  y1 = rect1.origin.y + rect1.size.height/2;



  cairo_set_source_rgba(cr, 1, 1, 1, 0.6);
  cairo_set_line_width(cr, 2*priv->scale);

  cairo_move_to(cr, x2, y2);
  cairo_curve_to(cr, x2+abs(x2-x1)/2, y2, x1-abs(x2-x1)/2,y1,x1,y1);
  cairo_stroke(cr);
}

static void
snapshot_links (GtkWidget* widget, GtkSnapshot* snapshot)
{
  PwCanvas* canv = PW_CANVAS(widget);
  PwCanvasPrivate* priv = pw_canvas_get_instance_private(canv);
  GList* link = pw_view_controller_get_link_list(priv->controller);
  GtkAllocation al;
  gtk_widget_get_allocation(widget, &al);
  graphene_rect_t canv_rect = GRAPHENE_RECT_INIT(0, 0, al.width, al.height);

  // TODO: test out if single cairo node aproach is actually faster than spearate nodes
  cairo_t* cai =gtk_snapshot_append_cairo(snapshot, &canv_rect);

  while(link)
    {
      PwLinkData* ldat = link->data;


      draw_single_link(widget, cai, link->data);


      link=link->next;
    }
}

static void
pw_canvas_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  snapshot_bg (widget, snapshot);
  snapshot_nodes (widget, snapshot);
  snapshot_links (widget, snapshot);
}

static void
pw_canvas_class_init (PwCanvasClass *klass)
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
      = g_param_spec_double ("zoom", "Zoom", "Zoom/scale of teh canvas", 0.1,
                             5.0, 1, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  properties[PROP_CONTROLLER] = g_param_spec_object (
      "controller", "Controller", "Driver of the canvas", G_TYPE_OBJECT,
      G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_css_name (widget_class, "canvas");
}

static GdkContentProvider *
prepare_cb (GtkDragSource *self, gdouble x, gdouble y, gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  GtkWidget *widget = GTK_WIDGET (canv);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  GtkWidget *pick = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);
  while (pick)
    {
      if (PW_IS_PAD (pick))
        {
          priv->dr_x = x;
          priv->dr_y = y;
          priv->dr_obj = pick;

          return gdk_content_provider_new_typed (PW_TYPE_PAD, pick);
        }

      if (PW_IS_NODE (pick))
        {
          int nx, ny;
          pw_node_get_pos (PW_NODE (pick), &nx, &ny);
          priv->dr_x = (x / priv->scale) - nx;
          priv->dr_y = (y / priv->scale) - ny;
          priv->dr_obj = pick;

          return gdk_content_provider_new_typed (PW_TYPE_NODE, pick);
        }
      pick = gtk_widget_get_parent (pick);
    }

  return NULL;
}

static void
drag_begin_cb (GtkDragSource *self, GdkDrag *drag, gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  if (PW_IS_NODE (priv->dr_obj))
    {
      PwNode* nod = PW_NODE(priv->dr_obj);

      gdk_drag_set_hotspot (drag, 0, 0);
      PwViewControllerInterface *iface = PW_VIEW_CONTROLLER_GET_IFACE (priv->controller);
      iface->node_to_front(priv->controller, pw_node_get_id(nod));
    }
}

static void
drag_end_cb (GtkDragSource *self, GdkDrag *drag, gboolean delete_data,
             gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  if (PW_IS_NODE (priv->dr_obj))
    {
      gdk_drag_set_hotspot (drag, 0, 0);
      priv->dr_obj = NULL;
    }
}

static void
drag_cancel_cb (GtkDragSource *self, GdkDrag *drag, gboolean delete_data,
                gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  if (PW_IS_NODE (priv->dr_obj))
    {
      gdk_drag_set_hotspot (drag, priv->dr_x, priv->dr_y);
      priv->dr_obj = NULL;
    }
}

static gboolean
drop_cb (GtkDropTarget *self, const GValue *value, gdouble x, gdouble y,
         gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  PwNode *nod = PW_NODE (g_value_get_object (value));

  pw_node_set_xpos (nod, (x / priv->scale) - priv->dr_x);
  pw_node_set_ypos (nod, (y / priv->scale) - priv->dr_y);
  priv->dr_obj = NULL;

  return TRUE;
}

static GdkDragAction
motion_cb (GtkDropTarget *self, gdouble x, gdouble y, gpointer user_data)
{
  PwCanvas *canv = PW_CANVAS (user_data);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (canv);

  PwNode *nod = PW_NODE (priv->dr_obj);

  pw_node_set_xpos (nod, (x / priv->scale) - priv->dr_x);
  pw_node_set_ypos (nod, (y / priv->scale) - priv->dr_y);

  return GDK_ACTION_MOVE;
}

static void
_tmp_canv_init (PwCanvas *self)
{
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);
  PwDummy *con = PW_DUMMY (priv->controller);

  PwViewControllerInterface *iface = PW_VIEW_CONTROLLER_GET_IFACE (con);

  PwNodeData dat = { 1, "sample-1" };
  iface->add_node (G_OBJECT (con), self, dat);

  PwPadData bat = {
    .id = 2, .parent_id = 1, .name = "out", .direction = PW_PAD_DIRECTION_OUT
  };
  iface->add_pad (G_OBJECT (con), bat);

  PwPadData gat = {
    .id = 3, .parent_id = 1, .name = "in", .direction = PW_PAD_DIRECTION_IN
  };
  iface->add_pad (G_OBJECT (con), gat);

  dat.id = 4;
  dat.title = "sample-2";
  iface->add_node (G_OBJECT (con), self, dat);

  bat.id = 5;
  bat.parent_id = 4;
  iface->add_pad (G_OBJECT (con), bat);

  gat.id = 6;
  gat.parent_id = 4;
  iface->add_pad (G_OBJECT (con), gat);
}

static void
pw_canvas_init (PwCanvas *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);
  PwPipewire *con = pw_pipewire_new (self);
  priv->controller = G_OBJECT (con);

  priv->dr_src = gtk_drag_source_new ();
  gtk_drag_source_set_icon (priv->dr_src, NULL, 0, 0);
  gtk_drag_source_set_actions (priv->dr_src, GDK_ACTION_MOVE);
  g_signal_connect (priv->dr_src, "prepare", G_CALLBACK (prepare_cb), self);
  g_signal_connect (priv->dr_src, "drag-begin", G_CALLBACK (drag_begin_cb),
                    self);
  g_signal_connect (priv->dr_src, "drag-end", G_CALLBACK (drag_end_cb), self);
  g_signal_connect (priv->dr_src, "drag-cancel", G_CALLBACK (drag_cancel_cb),
                    self);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->dr_src));

  priv->dr_tgt = gtk_drop_target_new (PW_TYPE_NODE, GDK_ACTION_MOVE);
  g_signal_connect (priv->dr_tgt, "drop", G_CALLBACK (drop_cb), self);
  g_signal_connect (priv->dr_tgt, "motion", G_CALLBACK (motion_cb), self);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (priv->dr_tgt));

  gtk_widget_set_overflow(widget, GTK_OVERFLOW_HIDDEN);

  pw_pipewire_run(con);
}
