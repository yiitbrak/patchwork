#include "pw-zoom-entry.h"
#include <locale.h>

struct _PwZoomEntry
{
  GtkWidget parent_instance;
  GSimpleActionGroup *actions;

  GtkAdjustment *adj;
  GtkBox *hbox;
  GtkButton *inc_but, *dec_but;
  GtkEntry *entry;
  GtkPopoverMenu *popover;
};

enum
{
  PROP_0,
  PROP_ADJUSTMENT,
  N_PROPS
};

enum
{
  SIG_ACTIVATE,
  N_SIG
};

static GParamSpec *properties[N_PROPS];

static int signals[N_SIG];

///////////////////////////////////////////////////////////
static void
pw_zoom_entry_finalize (GObject *object);

static void
pw_zoom_entry_dispose (GObject *object);

static void
pw_zoom_entry_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec);

static void
pw_zoom_entry_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec);

static void
update_value_string(PwZoomEntry *self);

static void
zoom_entry_set_adjustment (PwZoomEntry *self, GtkAdjustment *adjustment);

static GtkSizeRequestMode
pw_zoom_entry_get_request_mode (GtkWidget *widget);

static void
pw_zoom_entry_measure (GtkWidget *widget,
                       GtkOrientation orientation,
                       int for_size,
                       int *minimum,
                       int *natural,
                       int *minimum_baseline,
                       int *natural_baseline);

static GAction *
pw_zoom_entry_lookup_action (GActionMap *action_map,
                             const char *action_name);

static void
pw_zoom_entry_add_action (GActionMap *action_map,
                          GAction *action);

static void
pw_zoom_entry_remove_action (GActionMap *action_map,
                             const char *action_name);

static void
g_action_map_iface_init (GActionMapInterface *iface);

static void
pw_zoom_entry_icon_press (PwZoomEntry          *self,
               GtkEntryIconPosition  icon_pos,
               GtkEntry             *entry);

static gboolean
read_value(const char* string, gdouble *value);

static void
pw_zoom_entry_activate (PwZoomEntry *self,
                        GtkEntry    *entry);

static void
pw_zoom_entry_set_value_action(GSimpleAction *simple,
                               GVariant      *parameter,
                               gpointer       user_data);

static void
pw_zoom_entry_increase_value_action(GSimpleAction *simple,
                                    GVariant      *parameter,
                                    gpointer       user_data);
///////////////////////////////////////////////////////////

static const GActionEntry zoom_entry_actions[] = {
  { "set-value", pw_zoom_entry_set_value_action, "d" },
  { "increase-value", pw_zoom_entry_increase_value_action, "d" },
};

G_DEFINE_FINAL_TYPE_WITH_CODE (PwZoomEntry, pw_zoom_entry, GTK_TYPE_WIDGET,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP, g_action_map_iface_init))

PwZoomEntry *
pw_zoom_entry_new(GtkAdjustment *adjustment)
{
  g_return_val_if_fail(GTK_IS_ADJUSTMENT(adjustment), NULL);
  return g_object_new (PW_TYPE_ZOOM_ENTRY, "adjustment", adjustment, NULL);
}

static void
pw_zoom_entry_finalize (GObject *object)
{
  PwZoomEntry *self = (PwZoomEntry *) object;

  G_OBJECT_CLASS (pw_zoom_entry_parent_class)->finalize (object);
}

static void
pw_zoom_entry_dispose (GObject *object)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (object);

  gtk_widget_unparent (GTK_WIDGET (g_steal_pointer (&ze->popover)));
  gtk_widget_dispose_template (GTK_WIDGET (object), PW_TYPE_ZOOM_ENTRY);

  G_OBJECT_CLASS (pw_zoom_entry_parent_class)->dispose (object);
}

static void
pw_zoom_entry_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  PwZoomEntry *self = PW_ZOOM_ENTRY (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      g_value_set_object (value, G_OBJECT(self->adj));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
update_value_string(PwZoomEntry *self)
{
  static char string[16];
  struct lconv *local = localeconv();
  GtkEntryBuffer *entry_buffer = gtk_entry_get_buffer (self->entry);

  gdouble value = gtk_adjustment_get_value(self->adj);

  g_snprintf (string, 16, "%.1f%%",  value);
  uint length = g_utf8_strlen (string, 16);
  gtk_entry_buffer_set_text (entry_buffer, string, length);
}

static void
zoom_entry_set_adjustment (PwZoomEntry *self, GtkAdjustment *adjustment)
{
  g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));

  if(adjustment==self->adj)
    return;

  if(self->adj){
    g_signal_handlers_disconnect_by_func(self->adj, update_value_string, self);
    g_object_unref(self->adj);
  }

  self->adj = adjustment;
  g_object_ref_sink(self->adj);

  g_signal_connect_swapped(self->adj, "value-changed", G_CALLBACK(update_value_string), self);
  gtk_actionable_set_action_target(GTK_ACTIONABLE(self->inc_but), "d",
                                   gtk_adjustment_get_step_increment(self->adj));
  gtk_actionable_set_action_target(GTK_ACTIONABLE(self->dec_but), "d",
                                   -gtk_adjustment_get_step_increment(self->adj));

  update_value_string(self);
}

static void
pw_zoom_entry_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  PwZoomEntry *self = PW_ZOOM_ENTRY (object);

  switch (prop_id)
    {
    case PROP_ADJUSTMENT:
      zoom_entry_set_adjustment(self, g_value_get_object(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GtkSizeRequestMode
pw_zoom_entry_get_request_mode (GtkWidget *widget)
{
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

static void
pw_zoom_entry_measure (GtkWidget *widget,
                       GtkOrientation orientation,
                       int for_size,
                       int *minimum,
                       int *natural,
                       int *minimum_baseline,
                       int *natural_baseline)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (widget);
  gtk_widget_measure (GTK_WIDGET (ze->hbox), orientation, for_size, minimum,
                      natural, minimum_baseline, natural_baseline);
}

static void
pw_zoom_entry_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  PwZoomEntry *self = PW_ZOOM_ENTRY (widget);

  GtkAllocation al = { .x = 0, .y = 0, .width = width, .height = height };
  gtk_widget_size_allocate (GTK_WIDGET (self->hbox), &al, -1);
}

static GAction *
pw_zoom_entry_lookup_action (GActionMap *action_map,
                             const char *action_name)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (action_map);

  if (!ze->actions)
    return NULL;

  return g_action_map_lookup_action (G_ACTION_MAP (ze->actions), action_name);
}

static void
pw_zoom_entry_add_action (GActionMap *action_map,
                          GAction *action)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (action_map);

  if (!ze->actions)
    return;

  g_action_map_add_action (G_ACTION_MAP (ze->actions), action);
}

static void
pw_zoom_entry_remove_action (GActionMap *action_map,
                             const char *action_name)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (action_map);

  if (!ze->actions)
    return;

  g_action_map_remove_action (G_ACTION_MAP (ze->actions), action_name);
}

static void
g_action_map_iface_init (GActionMapInterface *iface)
{
  iface->lookup_action = pw_zoom_entry_lookup_action;
  iface->add_action = pw_zoom_entry_add_action;
  iface->remove_action = pw_zoom_entry_remove_action;
}

static void
pw_zoom_entry_class_init (PwZoomEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = pw_zoom_entry_finalize;
  object_class->dispose = pw_zoom_entry_dispose;
  object_class->get_property = pw_zoom_entry_get_property;
  object_class->set_property = pw_zoom_entry_set_property;
  widget_class->get_request_mode = pw_zoom_entry_get_request_mode;
  widget_class->measure = pw_zoom_entry_measure;
  widget_class->size_allocate = pw_zoom_entry_size_allocate;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/nidi/patchwork/res/ui/pw-zoom-entry.ui");
  gtk_widget_class_bind_template_child (widget_class, PwZoomEntry, hbox);
  gtk_widget_class_bind_template_child (widget_class, PwZoomEntry, inc_but);
  gtk_widget_class_bind_template_child (widget_class, PwZoomEntry, entry);
  gtk_widget_class_bind_template_child (widget_class, PwZoomEntry, dec_but);
  gtk_widget_class_bind_template_child (widget_class, PwZoomEntry, popover);
  gtk_widget_class_bind_template_callback (widget_class, pw_zoom_entry_icon_press);
  gtk_widget_class_bind_template_callback (widget_class, pw_zoom_entry_activate);

  properties[PROP_ADJUSTMENT] = g_param_spec_object("adjustment", "Adjustment",
                                                    "The GtkAdjustment belonging to this zoom entry",
                                                    GTK_TYPE_ADJUSTMENT ,G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
pw_zoom_entry_icon_press (PwZoomEntry *self,
               GtkEntryIconPosition icon_pos,
               GtkEntry *entry)
{
  gtk_popover_popup (GTK_POPOVER (self->popover));
}

static gboolean
read_value(const char* string, gdouble *value)
{
  g_assert(string && value);
  gboolean result = FALSE;

  gdouble tval = atof(string);

  if(tval != 0){
    result = TRUE;
    *value = tval;
  }

  return result;
}

static void
pw_zoom_entry_activate (PwZoomEntry *self,
                        GtkEntry    *entry)
{
  gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))), NULL);


  GtkEntryBuffer *ebuffer = gtk_entry_get_buffer(entry);
  const char* str = gtk_entry_buffer_get_text(ebuffer);
  int len = gtk_entry_buffer_get_length(ebuffer);
  gdouble val;

  if(len==0 || !read_value(str, &val)){
    update_value_string(self);
    return;
  }

  gtk_adjustment_set_value(self->adj, val);
}

static void
pw_zoom_entry_set_value_action(GSimpleAction *simple,
                               GVariant      *parameter,
                               gpointer       user_data)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (user_data);
  gtk_adjustment_set_value(ze->adj, g_variant_get_double (parameter));
}

static void
pw_zoom_entry_increase_value_action(GSimpleAction *simple,
                                    GVariant      *parameter,
                                    gpointer       user_data)
{
  PwZoomEntry *ze = PW_ZOOM_ENTRY (user_data);
  gdouble newv = g_variant_get_double (parameter) + gtk_adjustment_get_value(ze->adj);
  gtk_adjustment_set_value(ze->adj, newv);
}

static void
pw_zoom_entry_init (PwZoomEntry *self)
{
// Initial values
  self->adj = NULL;

// Actions
  GtkWidget *widget = GTK_WIDGET (self);
  self->actions = g_simple_action_group_new ();
  gtk_widget_insert_action_group (widget, "zoom_entry", G_ACTION_GROUP (self->actions));
  g_action_map_add_action_entries (G_ACTION_MAP (self), zoom_entry_actions,
                                   G_N_ELEMENTS (zoom_entry_actions), self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

void
pw_zoom_entry_set_adjustment(PwZoomEntry *self, GtkAdjustment *adjustment)
{
  g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
  g_object_set (self, "adjustment", adjustment, NULL);
}

GtkAdjustment *
pw_zoom_entry_get_adjustment(PwZoomEntry *self)
{
  gpointer ret;
  g_object_get(self, "adjustment", &ret, NULL);
  return ret;
}
