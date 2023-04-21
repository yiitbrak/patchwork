#include "pw-canvas.h"
#include "pw-dummy.h"
#include "pw-node.h"
#include "pw-view-controller.h"

typedef struct
{
  gdouble scale;
  gint dr_x, dr_y; // mouse ptr offsets in canvas units
  GtkWidget *dr_obj;

  GObject *controller;
  GtkDragSource *dr_src;
  GtkDropTarget *dr_tgt;
} PwCanvasPrivate;

G_DEFINE_TYPE_WITH_CODE (PwCanvas, pw_canvas, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (PwCanvas))

enum
{
  PROP_0,
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
pw_canvas_set_property (GObject *object, guint prop_id, const GValue *value,
                        GParamSpec *pspec)
{
  PwCanvas *self = PW_CANVAS (object);
  PwCanvasPrivate *priv = pw_canvas_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ZOOM:
      priv->scale = g_value_get_double (value);
      gtk_widget_queue_allocate (GTK_WIDGET (self));
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

  gtk_widget_measure (child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w, NULL,
                      NULL);
  gtk_widget_measure (child, GTK_ORIENTATION_VERTICAL, -1, NULL, &h, NULL,
                      NULL);

  GskTransform *tr = gsk_transform_new ();
  tr = gsk_transform_scale (tr, priv->scale, priv->scale);
  graphene_point_t pt = { .x = x, .y = y };
  tr = gsk_transform_translate (tr, &pt);

  gtk_widget_allocate (child, w, h, -1, tr);
}

static void
pw_canvas_size_allocate (GtkWidget *widget, int width, int height,
                         int baseline)
{
  GtkWidget *child = gtk_widget_get_first_child (widget);
  while (child)
    {
      if (PW_IS_NODE (child))
        {
          allocate_node (widget, child);
        }
      else
        {
          child = gtk_widget_get_next_sibling (widget);
          g_log ("ugagagagu", G_LOG_LEVEL_CRITICAL, "Uh oooooooh, stinky");
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
  graphene_rect_t repeat_rect;

  gint len = 25 * priv->scale;

  float val = 0.5 + (adw_style_manager_get_dark (style) ? -1 : 1) * 0.25;
  const GdkRGBA color = { val, val, val, 1.0f };

  gtk_snapshot_push_repeat (snapshot, &alloc, NULL);

  repeat_rect = GRAPHENE_RECT_INIT (15, 0, 1, len);
  gtk_snapshot_append_color (snapshot, &color, &repeat_rect);
  repeat_rect = GRAPHENE_RECT_INIT (0, 15, len, 1);
  gtk_snapshot_append_color (snapshot, &color, &repeat_rect);
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
  PwDummy *con = pw_dummy_new ();
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

  _tmp_canv_init (self);
}
