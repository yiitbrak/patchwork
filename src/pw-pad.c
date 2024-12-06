#include "pw-pad.h"
#include "pw-types.h"

typedef struct
{
  guint32 id;
  guint32 parent_id;
  PwPadDirection direction;
  PwPadType media_type;
  GtkLabel *name;

  GtkDropTarget *dr_tgt;
} PwPadPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (PwPad, pw_pad, GTK_TYPE_WIDGET)

enum
{
  PROP_0,
  PROP_ID,
  PROP_PARENT_ID,
  PROP_DIRECTION,
  PROP_NAME,
  PROP_TYPE,
  N_PROPS
};

enum
{
  SIG_LINK_ADDED,
  SIG_LINK_REMOVED,
  N_SIG
};

static GParamSpec *properties[N_PROPS];

static gint signals[N_SIG] = {
  0,
};

PwPad *
pw_pad_new (guint32 id, PwPadDirection dir, PwPadType type)
{
  return g_object_new (PW_TYPE_PAD, "id", id, "direction", dir, "type", type, NULL);
}

PwPad *
pw_pad_new_with_name (guint32 id, PwPadDirection dir, PwPadType type, const char *name)
{
  return g_object_new (PW_TYPE_PAD, "id", id, "direction", dir, "type", type, "name", name,
                       NULL);
}

static void
pw_pad_dispose (GObject *object)
{
  PwPad *pad = PW_PAD (object);
  PwPadPrivate *priv = pw_pad_get_instance_private (pad);
  GtkWidget *lab = GTK_WIDGET (priv->name);
  g_clear_pointer (&lab, gtk_widget_unparent);
  G_OBJECT_CLASS (pw_pad_parent_class)->dispose (object);
}

static void
pw_pad_finalize (GObject *object)
{
  PwPad *self = (PwPad *)object;
  PwPadPrivate *priv = pw_pad_get_instance_private (self);
  G_OBJECT_CLASS (pw_pad_parent_class)->finalize (object);
}



static void
pw_pad_get_property (GObject *object, guint prop_id, GValue *value,
                     GParamSpec *pspec)
{
  PwPad *self = PW_PAD (object);
  PwPadPrivate *priv = pw_pad_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_uint (value, priv->id);
      break;
    case PROP_PARENT_ID:
      g_value_set_uint (value, priv->parent_id);
      break;
    case PROP_DIRECTION:
      g_value_set_enum (value, priv->direction);
      break;
    case PROP_TYPE:
      g_value_set_enum(value, priv->media_type);
      break;
    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (priv->name));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
set_prop_direction (PwPad *self, const GValue *value)
{
  PwPadPrivate *priv = pw_pad_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);
  const char *class;

  gint dir = g_value_get_enum (value);
  switch (dir)
    {
    case PW_PAD_DIRECTION_IN:
      class = "in";
      break;
    case PW_PAD_DIRECTION_OUT:
      class = "out";
      break;
    default:
      g_log ("Patchwork", G_LOG_LEVEL_WARNING, "Invalid direction\n");
      return;
    }
  priv->direction = dir;
  gtk_widget_add_css_class (widget, class);
}

static const char*
get_css_class_for_type(PwPadType type)
{
  switch(type){
  case PW_PAD_TYPE_AUDIO:
    return "audio";
  case PW_PAD_TYPE_VIDEO:
    return "video";
  case PW_PAD_TYPE_MIDI:
    return "midi";
  case PW_PAD_TYPE_MIDI_PASSTHROUGH:
    return "midi_passthrough";
  case PW_PAD_TYPE_OTHER:
  default:
    return "other";
  }
}

static void
set_prop_type(PwPad *self, const GValue *value)
{
  PwPadPrivate* priv = pw_pad_get_instance_private(self);

  gtk_widget_remove_css_class(GTK_WIDGET(self), get_css_class_for_type(priv->media_type));
  priv->media_type = g_value_get_enum(value);
  gtk_widget_add_css_class(GTK_WIDGET(self), get_css_class_for_type(priv->media_type));
}

static void
pw_pad_set_property (GObject *object, guint prop_id, const GValue *value,
                     GParamSpec *pspec)
{
  PwPad *self = PW_PAD (object);
  PwPadPrivate *priv = pw_pad_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_ID:
      priv->id = g_value_get_uint (value);
      break;
    case PROP_PARENT_ID:
      priv->parent_id = g_value_get_uint (value);
      break;
    case PROP_DIRECTION:
      set_prop_direction (self, value);
      break;
    case PROP_TYPE:
      set_prop_type (self, value);
      break;
    case PROP_NAME:
      gtk_label_set_label (priv->name, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
pw_pad_get_request_mode (GtkWidget *self)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
pw_pad_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                int *minimum, int *natural, int *minimum_baseline,
                int *natural_baseline)
{
  PwPadPrivate *priv = pw_pad_get_instance_private (PW_PAD (widget));

  gtk_widget_measure (GTK_WIDGET (priv->name), orientation, for_size, minimum,
                      natural, minimum_baseline, natural_baseline);
}

static void
pw_pad_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  PwPadPrivate *priv = pw_pad_get_instance_private (PW_PAD (widget));

  gtk_widget_allocate (GTK_WIDGET (priv->name), width, height, -1, NULL);
}

static void
pw_pad_class_init (PwPadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pw_pad_dispose;
  object_class->finalize = pw_pad_finalize;
  object_class->get_property = pw_pad_get_property;
  object_class->set_property = pw_pad_set_property;
  widget_class->get_request_mode = pw_pad_get_request_mode;
  widget_class->measure = pw_pad_measure;
  widget_class->size_allocate = pw_pad_size_allocate;

  properties[PROP_ID]
      = g_param_spec_uint ("id", "Id", "Id of the pad", 0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  properties[PROP_PARENT_ID]
      = g_param_spec_uint ("parent-id", "Parent id", "Id of the parent", 0, G_MAXUINT, 0,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  properties[PROP_DIRECTION] = g_param_spec_enum (
      "direction", "Direction", "Direction of the pad", PW_TYPE_PAD_DIRECTION,
      PW_PAD_DIRECTION_IN, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  properties[PROP_TYPE] = g_param_spec_enum(
      "type", "Type", "Type of the pad", PW_TYPE_PAD_TYPE,
      PW_PAD_TYPE_OTHER, G_PARAM_READWRITE|G_PARAM_CONSTRUCT);
  properties[PROP_NAME] = g_param_spec_string (
      "name", "Name", "Name of the pad", "", G_PARAM_READWRITE);
  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[SIG_LINK_ADDED] = g_signal_new (
      "link-added", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_UINT);
  signals[SIG_LINK_REMOVED] = g_signal_new (
      "link-removed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);

  gtk_widget_class_set_css_name (widget_class, "pad");
}

static void
pad_notify_value_cb (GObject* self,
                     GParamSpec* pspec,
                     gpointer user_data)
{
  PwPad* pad = PW_PAD(user_data);
  PwPadPrivate* priv = pw_pad_get_instance_private(pad);
  GtkDropTarget* con = GTK_DROP_TARGET(self);
  const GValue* val = gtk_drop_target_get_value(con);

  if(!val)
    return;

  PwPad* recv = PW_PAD(g_value_get_object(val));

  if(priv->direction == pw_pad_get_direction(recv)||
     priv->media_type != pw_pad_get_media_type(recv))
  {
    gtk_drop_target_reject(con);
  }
}

static gboolean
pad_drop_cb (GtkDropTarget *self, const GValue *value, gdouble x, gdouble y,
             gpointer user_data)
{
  PwPad *pad = PW_PAD (user_data);
  PwPadPrivate *priv = pw_pad_get_instance_private (pad);

  PwPad *received = PW_PAD (g_value_get_object (value));

  if(priv->direction == pw_pad_get_direction(received)||
     priv->media_type != pw_pad_get_media_type(received))
  {
    return FALSE;
  }

  guint out, in;
  if(pw_pad_get_direction(pad)==PW_PAD_DIRECTION_OUT){
    out = pw_pad_get_id(pad);
    in = pw_pad_get_id(received);
  }else{
    out = pw_pad_get_id(received);
    in = pw_pad_get_id(pad);
  }
  g_signal_emit (pad, signals[SIG_LINK_ADDED], 0, out, in);

  return TRUE;
}

static void
pw_pad_init (PwPad *self)
{
  PwPadPrivate *priv = pw_pad_get_instance_private (self);
  priv->name = GTK_LABEL (gtk_label_new (""));
  priv->direction = -1;

  gtk_widget_set_parent (GTK_WIDGET (priv->name), GTK_WIDGET (self));

  priv->dr_tgt = gtk_drop_target_new (PW_TYPE_PAD, GDK_ACTION_MOVE);
  gtk_drop_target_set_preload(priv->dr_tgt, true);
  g_signal_connect (priv->dr_tgt, "notify::value", G_CALLBACK (pad_notify_value_cb), self);
  g_signal_connect (priv->dr_tgt, "drop", G_CALLBACK (pad_drop_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self),
                             GTK_EVENT_CONTROLLER (priv->dr_tgt));
}

guint32
pw_pad_get_id (PwPad *self)
{
  guint32 id;
  g_object_get (self, "id", &id, NULL);

  return id;
}

guint32
pw_pad_get_parent_id (PwPad *self)
{
  guint32 id;
  g_object_get (self, "parent-id", &id, NULL);

  return id;
}

PwPadDirection
pw_pad_get_direction(PwPad* self)
{
  g_return_val_if_fail(PW_IS_PAD(self) , PW_PAD_DIRECTION_INVALID);
  PwPadDirection dir;
  g_object_get(self , "direction", &dir,NULL);
  return dir;
}

PwPadType
pw_pad_get_media_type(PwPad* self)
{
  g_return_val_if_fail(PW_IS_PAD(self) , PW_PAD_TYPE_OTHER);
  PwPadType t;
  g_object_get(self , "type", &t,NULL);
  return t;
}
