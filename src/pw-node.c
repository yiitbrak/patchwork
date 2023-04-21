#include "pw-node.h"

typedef struct
{
  gint x, y;
  guint32 id;
  GList *in, *out;

  GtkBox *hbox, *in_box, *out_box, *main_box;
  GtkLabel *node_label;
} PwNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PwNode, pw_node, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_ID,
  PROP_TITLE,
  PROP_XPOS,
  PROP_YPOS,
  N_PROPS
};

enum
{
  SIG_LINK_ADDED,
  SIG_LINK_REMOVED,
  N_SIG
};

static GParamSpec *properties[N_PROPS];

static gint signals[N_SIG];

///////////////////////////////////////////////////////////
static void pw_node_measure (GtkWidget *widget, GtkOrientation orientation,
                             int for_size, int *minimum, int *natural,
                             int *minimum_baseline, int *natural_baseline);

///////////////////////////////////////////////////////////

/**
 * pw_node_new:
 *
 * Create a new #PwNode.
 *
 * Returns: (transfer full): a newly created #PwNode
 */

PwNode *
pw_node_new (guint id)
{
  return g_object_new (PW_TYPE_NODE, "id", id, NULL);
}

static void
pw_node_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), PW_TYPE_NODE);

  G_OBJECT_CLASS (pw_node_parent_class)->dispose (object);
}

static void
pw_node_finalize (GObject *object)
{
  PwNode *self = (PwNode *)object;
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  G_OBJECT_CLASS (pw_node_parent_class)->finalize (object);
}

static void
pw_node_get_property (GObject *object, guint prop_id, GValue *value,
                      GParamSpec *pspec)
{
  PwNode *self = PW_NODE (object);
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_uint (value, priv->id);
      break;
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (priv->node_label));
      break;
    case PROP_XPOS:
      {
        int x;
        pw_node_get_pos (self, &x, NULL);
        g_value_set_int (value, x);
      }
      break;
    case PROP_YPOS:
      {
      int y;
      pw_node_get_pos (self, NULL, &y);
      g_value_set_int (value, y);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pw_node_set_property (GObject *object, guint prop_id, const GValue *value,
                      GParamSpec *pspec)
{
  PwNode *self = PW_NODE (object);
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      break;
    case PROP_TITLE:
      gtk_label_set_label (priv->node_label, g_value_get_string (value));
      break;
    case PROP_XPOS:
      pw_node_set_xpos (self, g_value_get_int (value));
      break;
    case PROP_YPOS:
      pw_node_set_ypos (self, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
pw_node_get_request_mode (GtkWidget *this)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
pw_node_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                 int *minimum, int *natural, int *minimum_baseline,
                 int *natural_baseline)
{
  *minimum_baseline = -1;
  *natural_baseline = -1;

  PwNodePrivate *nod = pw_node_get_instance_private (PW_NODE (widget));
  gtk_widget_measure (GTK_WIDGET (nod->main_box), orientation, -1, minimum,
                      natural, NULL, NULL);
}

static void
pw_node_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  PwNodePrivate *priv = pw_node_get_instance_private (PW_NODE (widget));

  GtkAllocation al = { .x = 0, .y = 0, .width = width, .height = height };
  gtk_widget_size_allocate (GTK_WIDGET (priv->main_box), &al, -1);
}

static void
pw_node_class_init (PwNodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pw_node_dispose;
  object_class->finalize = pw_node_finalize;
  object_class->get_property = pw_node_get_property;
  object_class->set_property = pw_node_set_property;
  widget_class->get_request_mode = pw_node_get_request_mode;
  widget_class->measure = pw_node_measure;
  widget_class->size_allocate = pw_node_size_allocate;

  properties[PROP_ID]
      = g_param_spec_uint ("id", "Id", "Id of the node", 0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  properties[PROP_TITLE] = g_param_spec_string (
      "title", "Title", "Title of the node", "- -", G_PARAM_READWRITE);
  properties[PROP_XPOS]
      = g_param_spec_int ("x-pos", "X position", "X component of the node",
                          G_MININT32, G_MAXINT32, 0, G_PARAM_READWRITE);
  properties[PROP_YPOS]
      = g_param_spec_int ("y-pos", "Y position", "Y component of node",
                          G_MININT32, G_MAXINT32, 0, G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[SIG_LINK_ADDED] = g_signal_new (
      "link-added", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  signals[SIG_LINK_ADDED] = g_signal_new (
      "link-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  gtk_widget_class_set_template_from_resource (
      widget_class, "/org/nidi/patchwork/res/ui/pw-node.ui");
  gtk_widget_class_bind_template_child_private (widget_class, PwNode,
                                                main_box);
  gtk_widget_class_bind_template_child_internal_private (widget_class, PwNode,
                                                         node_label);
  gtk_widget_class_bind_template_child_internal_private (widget_class, PwNode,
                                                         hbox);
  gtk_widget_class_bind_template_child_internal_private (widget_class, PwNode,
                                                         in_box);
  gtk_widget_class_bind_template_child_internal_private (widget_class, PwNode,
                                                         out_box);

  gtk_widget_class_set_css_name (widget_class, "node");
}

static void
pw_node_init (PwNode *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  priv->in = NULL;
  priv->out = NULL;
}

guint32
pw_node_get_id (PwNode *self)
{
  guint32 id;
  g_object_get (self, "id", &id, NULL);

  return id;
}

void
pw_node_get_pos (PwNode *self, gint *X, gint *Y)
{
  g_return_if_fail (PW_IS_NODE (self));
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  if (X)
    *X = priv->x;

  if (Y)
    *Y = priv->y;
}

void
pw_node_set_xpos (PwNode *self, gint X)
{
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  priv->x = X;
}

void
pw_node_set_ypos (PwNode *self, gint Y)
{
  PwNodePrivate *priv = pw_node_get_instance_private (self);

  priv->y = Y;
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
pw_node_append_pad (PwNode *self, PwPad *pad, int direction)
{
  g_return_if_fail (PW_IS_NODE (self));
  g_return_if_fail (PW_IS_PAD (pad));

  PwNodePrivate *priv = pw_node_get_instance_private (self);

  GList **l;
  GtkBox *box;

  switch (direction)
    {
    case PW_PAD_DIRECTION_IN:
      l = &priv->in;
      box = priv->in_box;
      break;
    case PW_PAD_DIRECTION_OUT:
      l = &priv->out;
      box = priv->out_box;
      break;
    default:
      g_log ("Patchwork", G_LOG_LEVEL_ERROR, "Invalid pad direction\n");
      return;
    }

  *l = g_list_append (*l, pad);
  gtk_box_append (box, GTK_WIDGET (pad));
}
