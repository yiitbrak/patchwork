#include "pw-pipewire.h"
#include "pw-view-controller.h"
#include <pipewire/pipewire.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"

struct _PwPipewire
{
  GObject parent_instance;

  struct pw_thread_loop *loop;
  struct pw_context *context;
  struct pw_core *core;
  struct pw_registry *registry;
  struct spa_hook reg_listener;

  gint idle_id;
  GAsyncQueue *pw_recv;

  GList *nodes, *pads, *links;
  PwCanvas *canvas;
};

typedef enum
{
  MSG_NODE_ADDED,
  MSG_PORT_ADDED,
  MSG_LINK_ADDED,
  MSG_REMOVED,
  MSG_OTHER
} MessageType;

typedef struct
{
  MessageType type;
  void *data;
} Message;

typedef enum
{
  CAT_OTHER = 0,
  CAT_SOURCE = 1<<0,
  CAT_SINK = 1<<1,
  CAT_DUPLEX = CAT_SOURCE|CAT_SINK,
} NodeCategory;

static void pw_view_controller_iface_init (PwViewControllerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PwPipewire, pw_pipewire, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE (PW_TYPE_VIEW_CONTROLLER, pw_view_controller_iface_init))

enum
{
  PROP_0,
  N_PROPS
};

enum
{
  SIG_CHANGED,
  N_SIG
};

static GParamSpec *properties[N_PROPS];
static int signals[N_SIG];

///////////////////////////////////////////////////////////
static PwNode *pw_pipewire_get_node_by_id (GObject *self, gint id);

static PwPad *pw_pipewire_get_pad_by_id (GObject *this, gint id);

static PwLinkData *pw_pipewire_get_link_by_id (GObject *this, gint id);
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
static MessageType
reg_get_type (const char *type);

static void
default_changed_handler (PwPipewire *self, gpointer user_data);
///////////////////////////////////////////////////////////

PwPipewire *
pw_pipewire_new (PwCanvas *canvas)
{
  PwPipewire *pw = g_object_new (PW_TYPE_PIPEWIRE, NULL);
  pw->canvas = canvas;
  return pw;
}

static void
free_nodes (gpointer data)
{
  gtk_widget_unparent (GTK_WIDGET (data));
}

static void
pw_pipewire_dispose (GObject *object)
{
  PwPipewire *self = (PwPipewire *) object;

  g_source_remove(self->idle_id);

  pw_thread_loop_stop(self->loop);
  pw_proxy_destroy((struct pw_proxy *) self->registry);
  pw_core_disconnect(self->core);
  pw_context_destroy(self->context);
  pw_thread_loop_destroy(self->loop);

  GList *l = self->nodes;
  g_list_free_full (g_steal_pointer (&self->nodes), free_nodes);

  G_OBJECT_CLASS (pw_pipewire_parent_class)->dispose (object);
}

static void
pw_pipewire_finalize (GObject *object)
{
  PwPipewire *self = (PwPipewire *) object;

  G_OBJECT_CLASS (pw_pipewire_parent_class)->finalize (object);
}

static void
pw_pipewire_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  PwPipewire *self = PW_PIPEWIRE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pw_pipewire_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  PwPipewire *self = PW_PIPEWIRE (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pw_pipewire_class_init (PwPipewireClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = pw_pipewire_dispose;
  object_class->finalize = pw_pipewire_finalize;
  object_class->get_property = pw_pipewire_get_property;
  object_class->set_property = pw_pipewire_set_property;

  signals[SIG_CHANGED] = g_signal_new_class_handler ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, G_CALLBACK (default_changed_handler), NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
fill_rect_data(PwNode *nod, graphene_rect_t *rect)
{
  gint x,y,w,h;
  pw_node_get_pos(nod, &x, &y);
  rect->origin.x = x;
  rect->origin.y = y;
  gtk_widget_measure(GTK_WIDGET(nod), GTK_ORIENTATION_HORIZONTAL, -1, NULL, &w, NULL, NULL);
  gtk_widget_measure(GTK_WIDGET(nod), GTK_ORIENTATION_VERTICAL, -1, NULL, &h, NULL, NULL);
  rect->size.width = w;
  rect->size.height = h;
}

static void
pipewire_calc_node_pos(PwPipewire *self, PwNodeData *dat, graphene_point_t *pos)
{
  static const int segment = 500;
  static int counter[3] = {0,};
  graphene_rect_t rect;
  int i;
  GList *l = self->nodes;

  int cat = dat->category;
  switch(cat){
  case CAT_SOURCE:
    pos->x = 20;
    i=0;
    break;
  default:
  case CAT_DUPLEX:
  case CAT_OTHER:
    pos->x = segment+20;
    i=1;
    break;
  case CAT_SINK:
    pos->x = segment*2+20;
    i=2;
    break;
  }

  pos->y = 20;
  while(l){
    PwNode *nod = PW_NODE(l->data);
    fill_rect_data(nod, &rect);

    if(rect.origin.x == pos->x && rect.origin.y == pos->y){
      pos->y = counter[i] += 100;
    }

    l=l->next;
  }
}

static void
pw_pipewire_add_node (GObject *self, PwCanvas *canv, PwNodeData nod)
{
  g_return_if_fail (PW_IS_PIPEWIRE (self));
  PwPipewire *con = PW_PIPEWIRE (self);

  PwNode *nnod = pw_node_new (nod.id);
  graphene_point_t pos;
  pipewire_calc_node_pos(con, &nod, &pos);

  g_object_set (G_OBJECT (nnod), "title", nod.title, "type", nod.type,
                "x-pos", (int)pos.x,"y-pos", (int)pos.y, NULL);

  con->nodes = g_list_prepend (con->nodes, nnod);
  gtk_widget_set_parent (GTK_WIDGET (nnod), GTK_WIDGET (canv));
}

static void
link_added_cb(PwPad* self, guint out, guint in, gpointer user_data)
{
  PwPipewire *con = PW_PIPEWIRE (user_data);

  PwPad* out_pad = pw_pipewire_get_pad_by_id(G_OBJECT(con), out);
  PwPad* in_pad = pw_pipewire_get_pad_by_id(G_OBJECT(con), in);

  guint out_node = pw_pad_get_parent_id(out_pad);
  guint in_node = pw_pad_get_parent_id(in_pad);

  char ids[4][20];
  g_snprintf(ids[0], 20, "%u", out);
  g_snprintf(ids[1], 20, "%u", out_node);
  g_snprintf(ids[2], 20, "%u", in);
  g_snprintf(ids[3], 20, "%u", in_node);

  struct spa_dict props;
  struct spa_dict_item items[6];
  props = SPA_DICT_INIT(items, 0);
  items[props.n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_LINK_OUTPUT_PORT, ids[0]);
  items[props.n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_LINK_OUTPUT_NODE, ids[1]);
  items[props.n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_LINK_INPUT_PORT, ids[2]);
  items[props.n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_LINK_INPUT_NODE, ids[3]);
  if(!g_strcmp0(g_getenv("PIPEWIRE_LINK_PASSIVE"), "true"))
    items[props.n_items++] = SPA_DICT_ITEM_INIT(PW_KEY_LINK_PASSIVE, "true");

  pw_thread_loop_lock(con->loop);
  struct pw_proxy *proxy =
    pw_core_create_object(con->core, "link-factory", PW_TYPE_INTERFACE_Link,
                          PW_VERSION_LINK, &props, 0);
  pw_thread_loop_unlock(con->loop);
}

static void
pw_pipewire_add_pad (GObject *self, PwPadData data)
{
  g_return_if_fail (PW_IS_PIPEWIRE (self));
  PwPipewire *con = PW_PIPEWIRE (self);
  PwNode *nod;
  g_return_if_fail (
      nod = pw_pipewire_get_node_by_id (G_OBJECT (con), data.parent_id));

  PwPad *pad = pw_pad_new_with_name (data.id, data.direction, pw_node_get_media_type(nod), data.name);

  g_signal_connect (pad, "link-added", G_CALLBACK (link_added_cb), con);

  con->pads = g_list_prepend (con->pads, pad);
  pw_node_append_pad (nod, pad, data.direction);
}

static void
pw_pipewire_add_link (GObject *self, PwLinkData link)
{
  g_return_if_fail (PW_IS_PIPEWIRE (self));
  PwPipewire *con = PW_PIPEWIRE (self);

  PwLinkData *el = malloc (sizeof (PwLinkData));
  memcpy (el, &link, sizeof (PwLinkData));

  con->links = g_list_prepend (con->links, el);
}

static gboolean
pw_pipewire_remove (GObject *this, gint id)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), FALSE);

  PwPipewire *pw = PW_PIPEWIRE (this);
  GList *l;

  gpointer ptr = pw_pipewire_get_link_by_id (this, id);
  if (ptr)
    {
      PwLinkData *dat = ptr;
      pw->links = g_list_remove (pw->links, ptr);
      return TRUE;
    }

  ptr = pw_pipewire_get_pad_by_id (this, id);
  if (PW_IS_PAD (ptr))
    {
      PwPad *pad = PW_PAD (ptr);
      gtk_widget_unparent (GTK_WIDGET (pad));
      pw->pads = g_list_delete_link (pw->pads, g_list_find (pw->pads, ptr));
      return TRUE;
    }

  ptr = pw_pipewire_get_node_by_id (this, id);
  if (ptr)
    {
      PwNode *nod = PW_NODE (ptr);
      gtk_widget_unparent (GTK_WIDGET (nod));
      pw->nodes = g_list_remove (pw->nodes, ptr);
      return TRUE;
    }

  return FALSE;
}

static PwNode *
pw_pipewire_get_node_by_id (GObject *this, gint id)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), NULL);
  PwPipewire *pw = PW_PIPEWIRE (this);
  GList *l = pw->nodes;

  while (l)
    {
      if (pw_node_get_id (PW_NODE (l->data)) == id)
        return l->data;

      l = l->next;
    }
  return NULL;
}

static PwPad *
pw_pipewire_get_pad_by_id (GObject *this, gint id)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), NULL);
  PwPipewire *pw = PW_PIPEWIRE (this);
  GList *l = pw->pads;

  while (l)
    {
      if (pw_pad_get_id (PW_PAD (l->data)) == id)
        return l->data;

      l = l->next;
    }
  return NULL;
}

static PwLinkData *
pw_pipewire_get_link_by_id (GObject *this, gint id)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), NULL);
  PwPipewire *pw = PW_PIPEWIRE (this);
  GList *l = pw->links;

  while (l)
    {
      if (((PwPadData *) l->data)->id == id)
        return l->data;

      l = l->next;
    }
  return NULL;
}

static void
pw_pipewire_node_to_front (GObject *this, gint id)
{
  g_return_if_fail (PW_IS_PIPEWIRE (this));
  PwPipewire *pw = PW_PIPEWIRE (this);

  PwNode *n = pw_pipewire_get_node_by_id (G_OBJECT (pw), id);

  GList *elem = g_list_find (pw->nodes, n);
  pw->nodes = g_list_remove_link (pw->nodes, elem);

  // NULL because of last to snapshot is one in front
  pw->nodes = g_list_insert_before_link (pw->nodes, NULL, elem);
}

static GList *
pw_pipewire_get_node_list (GObject *this)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), NULL);
  PwPipewire *pw = PW_PIPEWIRE (this);
  return pw->nodes;
}

static GList *
pw_pipewire_get_link_list (GObject *this)
{
  g_return_val_if_fail (PW_IS_PIPEWIRE (this), NULL);
  PwPipewire *pw = PW_PIPEWIRE (this);
  return pw->links;
}

static void
pw_view_controller_iface_init (PwViewControllerInterface *iface)
{
  iface->add_node = pw_pipewire_add_node;
  iface->add_pad = pw_pipewire_add_pad;
  iface->add_link = pw_pipewire_add_link;
  iface->remove = pw_pipewire_remove;
  iface->get_node_by_id = pw_pipewire_get_node_by_id;
  iface->get_pad_by_id = pw_pipewire_get_pad_by_id;
  iface->node_to_front = pw_pipewire_node_to_front;
  iface->get_node_list = pw_pipewire_get_node_list;
  iface->get_link_list = pw_pipewire_get_link_list;
}

static void
pw_free_recv_queue (void *data)
{
  Message *msg = data;

  switch (msg->type)
    {
    case MSG_NODE_ADDED:
      {
        PwNodeData *dat = msg->data;
        free ((void *) dat->title);
      }
      break;
    case MSG_PORT_ADDED:
      {
        PwPadData *dat = msg->data;
        free ((void *) dat->name);
      }
      break;
    case MSG_LINK_ADDED:
      g_free(msg->data);
      break;
    case MSG_REMOVED:
      free (msg->data);
    default:
      break;
    }
  free (msg);
}

static void
default_changed_handler (PwPipewire *self, gpointer user_data)
{
  Message *msg;
  while (g_async_queue_length (self->pw_recv))
    {
      msg = g_async_queue_pop (self->pw_recv);

      switch (msg->type)
        {
        case MSG_NODE_ADDED:
          pw_pipewire_add_node (G_OBJECT (self), self->canvas,
                                *(PwNodeData *) msg->data);
          break;
        case MSG_PORT_ADDED:
          pw_pipewire_add_pad (G_OBJECT (self), *(PwPadData *) msg->data);
          break;
        case MSG_LINK_ADDED:
          pw_pipewire_add_link (G_OBJECT (self), *(PwLinkData *) msg->data);
          break;
        case MSG_REMOVED:
          pw_pipewire_remove (G_OBJECT (self), *(guint32 *) msg->data);
          break;
        case MSG_OTHER:
        default:
          break;
        }
    }
}

static void
pw_pipewire_init (PwPipewire *self)
{
  self->idle_id = 0;
  self->pw_recv = g_async_queue_new_full (pw_free_recv_queue);
  spa_zero (self->reg_listener);

  self->nodes = NULL;
  self->pads = NULL;
  self->links = NULL;
}

////////////////////////
static MessageType
reg_get_type (const char *type)
{
  MessageType res;
  if (g_strcmp0 (type, PW_TYPE_INTERFACE_Node) == 0)
    res = MSG_NODE_ADDED;
  else if (g_strcmp0 (type, PW_TYPE_INTERFACE_Port) == 0)
    res = MSG_PORT_ADDED;
  else if (g_strcmp0 (type, PW_TYPE_INTERFACE_Link) == 0)
    res = MSG_LINK_ADDED;
  else
    res = MSG_OTHER;

  return res;
}


static PwPadType
port_get_type(const struct spa_dict *props)
{
  const char* str = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if(str == NULL)
    str = spa_dict_lookup(props, PW_KEY_MEDIA_TYPE);
  if(str == NULL)
    return PW_PAD_TYPE_OTHER;

  if(g_strrstr(str, "Audio"))
    return PW_PAD_TYPE_AUDIO;

  if(g_strrstr(str, "Video"))
    return PW_PAD_TYPE_VIDEO;

  if(g_strrstr(str, "Midi"))
    return PW_PAD_TYPE_MIDI;

  return PW_PAD_TYPE_OTHER;
}

static NodeCategory
port_get_category(const struct spa_dict *props)
{
  const char *str = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if(str==NULL)
    str = spa_dict_lookup(props, PW_KEY_MEDIA_CATEGORY);
  if(str==NULL)
    return CAT_OTHER;

  NodeCategory res = CAT_OTHER;
  if(g_strrstr(str, "Output")||g_strrstr(str, "Source"))
     res |= CAT_SOURCE;

  if(g_strrstr(str, "Sink")||g_strrstr(str, "Input"))
    res |= CAT_SINK;

  if(g_strrstr(str, "Filter")||g_strrstr(str, "Bridge"))
    res = CAT_DUPLEX;

  return res;
}

static void
reg_fill_node (Message *msg, guint32 id, const struct spa_dict *props)
{
  PwNodeData *dat = malloc (sizeof (PwNodeData));
  const char *name;

  name = spa_dict_lookup (props, PW_KEY_NODE_DESCRIPTION);
  if (name == NULL)
    name = spa_dict_lookup (props, PW_KEY_NODE_NICK);
  if (name == NULL)
    name = spa_dict_lookup (props, PW_KEY_NODE_NAME);
  if (name == NULL)
    name = "Unnamed node";

  PwPadType type = port_get_type(props);
  NodeCategory cat = port_get_category(props);

  dat->id = id;
  dat->title = g_strdup (name);
  dat->type = type;
  dat->category = cat;

  msg->data = dat;
}

static void
reg_fill_port (PwPipewire *self, Message *msg, guint32 id, const struct spa_dict *props)
{
  PwPadData *dat = malloc (sizeof (PwPadData));
  const char *str;

  str = spa_dict_lookup (props, PW_KEY_NODE_ID);
  guint32 parent_id = atoi (str);

  str = spa_dict_lookup (props, PW_KEY_PORT_DIRECTION);
  PwPadDirection dir;
  if (g_strcmp0 (str, "in") == 0)
    dir = PW_PAD_DIRECTION_IN;
  else if (g_strcmp0 (str, "out") == 0)
    dir = PW_PAD_DIRECTION_OUT;
  else
    dir = PW_PAD_DIRECTION_INVALID;

  str = spa_dict_lookup (props, PW_KEY_PORT_ALIAS);
  if (str == NULL)
    str = spa_dict_lookup (props, PW_KEY_PORT_NAME);
  if (str == NULL)
    str = "Unnamed port";

  dat->name = g_strdup (str);
  dat->id = id;
  dat->parent_id = parent_id;
  dat->direction = dir;

  msg->data = dat;
}

static void
reg_fill_link (Message *msg, guint32 id, const struct spa_dict *props)
{
  PwLinkData *dat = malloc (sizeof (PwLinkData));
  const char *str;

  str = spa_dict_lookup (props, PW_KEY_LINK_INPUT_PORT);
  if (!str)
    g_error ("'in' returned NULL\n");
  dat->in = atoi (str);

  str = spa_dict_lookup (props, PW_KEY_LINK_OUTPUT_PORT);
  if (!str)
    g_error ("'out' returned NULL\n");
  dat->out = atoi (str);
  dat->id = id;
  dat->selected = false;

  msg->data = dat;
}

G_GNUC_UNUSED
static void
print_obj(guint32 id, const char *type, const struct spa_dict *props)
{
  const struct spa_dict_item* it;
  printf("ID: %u\nType: %s\n", id, type);
  spa_dict_for_each(it, props)
    printf("%s: %s\n",it->key, it->value);
  printf("--------------------------------------------\n");
}

static void
reg_event_global (void *data, guint32 id, guint32 permissions, const char *type, guint32 version, const struct spa_dict *props)
{
  PwPipewire *self = PW_PIPEWIRE (data);
  Message *msg = malloc (sizeof (Message));
  msg->type = reg_get_type (type);

  // print_obj(id, type, props);

  switch (msg->type)
    {
    case MSG_NODE_ADDED:
      reg_fill_node (msg, id, props);
      break;
    case MSG_PORT_ADDED:
      reg_fill_port (self, msg, id, props);
      break;
    case MSG_LINK_ADDED:
      reg_fill_link (msg, id, props);
      break;
    case MSG_OTHER:
    default:
      free (msg);
      return;
    }

  g_async_queue_push (self->pw_recv, msg);
}

static void
remove_event_global (void *data, uint32_t id)
{
  PwPipewire *self = PW_PIPEWIRE (data);
  Message *msg = malloc (sizeof (Message));

  msg->type = MSG_REMOVED;

  msg->data = malloc (sizeof (guint32));
  *(guint32 *) msg->data = id;

  g_async_queue_push (self->pw_recv, msg);
}

static const struct pw_registry_events
    registry_events = { PW_VERSION_REGISTRY_EVENTS, .global = reg_event_global,
                        .global_remove = remove_event_global };

static gboolean
idle_check_query (gpointer data)
{
  PwPipewire *self = PW_PIPEWIRE (data);
  if (g_async_queue_length (self->pw_recv) != 0)
    g_signal_emit (self, signals[SIG_CHANGED], 0);
  return G_SOURCE_CONTINUE;
}

void
pw_pipewire_run (PwPipewire *self)
{
  self->loop = pw_thread_loop_new ("pipewire_thrd", NULL);
  self->context = pw_context_new (pw_thread_loop_get_loop (self->loop), NULL, 0);
  self->core = pw_context_connect (self->context, NULL, 0);

  self->registry = pw_core_get_registry (self->core, PW_VERSION_CORE, 0);
  pw_registry_add_listener (self->registry, &self->reg_listener,
                            &registry_events, self);

  pw_thread_loop_start (self->loop);
  self->idle_id = g_timeout_add (150, idle_check_query, self);
}

#pragma GCC diagnostic pop
