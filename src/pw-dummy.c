#include "pw-dummy.h"
#include "pw-node.h"
#include "pw-pad.h"
#include "pw-view-controller.h"

struct _PwDummy
{
  GObject parent_instance;

  GList *nodes;
  GList *pads;
  GList *links;
};

static void pw_view_controller_iface_init (PwViewControllerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (PwDummy, pw_dummy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (PW_TYPE_VIEW_CONTROLLER,
                                                pw_view_controller_iface_init))

enum
{
  PROP_0,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

///////////////////////////////////////////////////////////
static void pw_dummy_finalize (GObject *object);

static void pw_dummy_get_property (GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec);

static void pw_dummy_set_property (GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec);

static PwNode*
pw_dummy_get_node_by_id (GObject* this, gint id);

static void
_link_added_cb (PwPad* self, guint id1, guint id2, gpointer user_data);
///////////////////////////////////////////////////////////

PwDummy *
pw_dummy_new (void)
{
  return g_object_new (PW_TYPE_DUMMY, NULL);
}

static void
free_nodes (gpointer data)
{
  gtk_widget_unparent (GTK_WIDGET (data));
}

static void
pw_dummy_dispose (GObject *object)
{
  PwDummy *dum = PW_DUMMY (object);

  GList *l = dum->nodes;
  g_list_free_full (g_steal_pointer (&dum->nodes), free_nodes);

  G_OBJECT_CLASS (pw_dummy_parent_class)->dispose (object);
}

static void
pw_dummy_finalize (GObject *object)
{
  PwDummy *self = (PwDummy *)object;

  G_OBJECT_CLASS (pw_dummy_parent_class)->finalize (object);
}

static void
pw_dummy_get_property (GObject *object, guint prop_id, GValue *value,
                       GParamSpec *pspec)
{
  PwDummy *self = PW_DUMMY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pw_dummy_set_property (GObject *object, guint prop_id, const GValue *value,
                       GParamSpec *pspec)
{
  PwDummy *self = PW_DUMMY (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
pw_dummy_class_init (PwDummyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = pw_dummy_dispose;
  object_class->finalize = pw_dummy_finalize;
  object_class->get_property = pw_dummy_get_property;
  object_class->set_property = pw_dummy_set_property;
}

static gint cord = 50;

static void
pw_dummy_add_node (GObject *this, PwCanvas *canv, PwNodeData nod)
{
  g_return_if_fail(PW_IS_DUMMY(this));
  PwDummy *con = PW_DUMMY (this);

  PwNode *nnod = pw_node_new (nod.id);
  pw_node_set_xpos (nnod, cord);
  pw_node_set_ypos (nnod, cord);
  g_object_set (G_OBJECT (nnod), "title", nod.title, NULL);

  con->nodes = g_list_prepend (con->nodes, nnod);
  gtk_widget_set_parent (GTK_WIDGET (nnod), GTK_WIDGET (canv));

  cord += 50;
}

static void
pw_dummy_add_pad (GObject *this, PwPadData data)
{
  g_return_if_fail(PW_IS_DUMMY(this));
  PwDummy *con = PW_DUMMY (this);
  PwNode *nod;
  g_return_if_fail (nod = pw_dummy_get_node_by_id(G_OBJECT(con), data.parent_id));

  PwPad *pad = pw_pad_new_with_name (data.id, data.direction, data.name);

  g_signal_connect(pad , "link-added" , G_CALLBACK(_link_added_cb), con);

  con->pads = g_list_prepend(con->pads , pad);
  pw_node_append_pad (nod, pad, data.direction);
}

static void
pw_dummy_add_link(GObject* this, PwLinkData data)
{
  g_return_if_fail(PW_IS_DUMMY(this));
  PwDummy *con = PW_DUMMY (this);

  PwLinkData* el = malloc(sizeof(PwLinkData));
  memcpy(el , &data , sizeof(PwLinkData));

  con->links = g_list_prepend(con->links, el);
}

static PwNode*
pw_dummy_get_node_by_id (GObject* this, gint id)
{
  g_return_val_if_fail(PW_IS_DUMMY(this) , NULL);
  PwDummy* dum = PW_DUMMY(this);
  GList* l=dum->nodes;

  while(l)
    {
      if(pw_node_get_id(PW_NODE(l->data))==id)
        return l->data;

      l=l->next;
    }
  return NULL;
}

static PwPad*
pw_dummy_get_pad_by_id (GObject* this, gint id)
{
  g_return_val_if_fail(PW_IS_DUMMY(this) , NULL);
  PwDummy* dum = PW_DUMMY(this);
  GList* l=dum->pads;

  while(l)
    {
      if(pw_pad_get_id(PW_PAD(l->data))==id)
        return l->data;

      l=l->next;
    }
  return NULL;
}

static void
pw_dummy_node_to_front (GObject *this, gint id)
{
  g_return_if_fail(PW_IS_DUMMY(this));
  PwDummy* dum = PW_DUMMY(this);

  PwNode* n = pw_dummy_get_node_by_id(G_OBJECT(dum), id);

  GList* elem = g_list_find(dum->nodes, n);
  dum->nodes = g_list_remove_link(dum->nodes, elem);

  // NULL because of last to snapshot is one in front
  dum->nodes = g_list_insert_before_link(dum->nodes, NULL, elem);
}

static GList*
pw_dummy_get_node_list(GObject* this)
{
  g_return_val_if_fail(PW_IS_DUMMY(this),NULL);
  PwDummy* dum = PW_DUMMY(this);
  return dum->nodes;
}

static GList*
pw_dummy_get_link_list(GObject* this)
{
  g_return_val_if_fail(PW_IS_DUMMY(this),NULL);
  PwDummy* dum = PW_DUMMY(this);
  return dum->links;
}

static void
pw_view_controller_iface_init (PwViewControllerInterface *iface)
{
  iface->add_node = pw_dummy_add_node;
  iface->add_pad = pw_dummy_add_pad;
  iface->add_link = pw_dummy_add_link;
  iface->get_node_by_id = pw_dummy_get_node_by_id;
  iface->get_pad_by_id = pw_dummy_get_pad_by_id;
  iface->node_to_front = pw_dummy_node_to_front;
  iface->get_node_list = pw_dummy_get_node_list;
  iface->get_link_list = pw_dummy_get_link_list;
}

static void
pw_dummy_init (PwDummy *self)
{
  self->nodes = NULL;
  self->pads  = NULL;
  self->links = NULL;
}

static void
_link_added_cb (PwPad* self, guint id1, guint id2, gpointer user_data)
{
  PwDummy* dum = PW_DUMMY(user_data);
  static int id_gen = 28;

  PwPad* pad = pw_dummy_get_pad_by_id(G_OBJECT(dum), id1);
  guint in,out;
  if(pw_pad_get_direction(pad)==PW_PAD_DIRECTION_IN){
    in = id1;
    out = id2;
  }else{
    in=id2;
    out = id1;
  }

  PwLinkData dat = {.id=id_gen,.in = in, .out=out};
  pw_dummy_add_link(G_OBJECT(dum) , dat);
}
