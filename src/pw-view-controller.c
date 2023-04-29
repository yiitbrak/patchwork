#include "pw-view-controller.h"

G_DEFINE_INTERFACE (PwViewController, pw_view_controller, G_TYPE_OBJECT)

enum
{
  SIG_CHANGE_NOTIFY,
  N_SIG
};

static gint signals[N_SIG];

static void
pw_view_controller_default_init (PwViewControllerInterface *iface)
{
  signals[SIG_CHANGE_NOTIFY]
      = g_signal_new ("change-notify", G_TYPE_FROM_INTERFACE (iface),
                      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

void
pw_view_controller_add_node (GObject *this, PwCanvas *canv, PwNodeData nod)
{
  PwViewControllerInterface *iface;
  g_return_if_fail (PW_IS_VIEW_CONTROLLER (this));

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  iface->add_node (this, canv, nod);
}
void
pw_view_controller_add_link (GObject *this, PwLinkData link)
{
  PwViewControllerInterface *iface;
  g_return_if_fail (PW_IS_VIEW_CONTROLLER (this));

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  iface->add_link (this, link);
}

void
pw_view_controller_add_pad (GObject *this, PwPadData pad)
{
  PwViewControllerInterface *iface;
  g_return_if_fail (PW_IS_VIEW_CONTROLLER (this));

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  iface->add_pad (this, pad);
}

gboolean
pw_view_controller_remove (GObject *this, gint id)
{
  PwViewControllerInterface *iface;
  g_return_val_if_fail (PW_IS_VIEW_CONTROLLER (this),false);

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  return iface->remove(this, id);
}

PwNode*
pw_view_controller_get_node_by_id (GObject* this, gint id)
{
  PwViewControllerInterface *iface;
  g_return_val_if_fail (PW_IS_VIEW_CONTROLLER (this),NULL);

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  return iface->get_node_by_id (this, id);
}

PwPad*
pw_view_controller_get_pad_by_id (GObject* this, gint id)
{
  PwViewControllerInterface *iface;
  g_return_val_if_fail (PW_IS_VIEW_CONTROLLER (this),NULL);

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  return iface->get_pad_by_id (this, id);
}

void
pw_view_controller_node_to_front(GObject *this, gint nod)
{
  PwViewControllerInterface *iface;
  g_return_if_fail (PW_IS_VIEW_CONTROLLER (this));

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  iface->node_to_front (this, nod);
}

GList*
pw_view_controller_get_node_list (GObject *this)
{
  PwViewControllerInterface *iface;
  g_return_val_if_fail (PW_IS_VIEW_CONTROLLER (this), NULL);

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  return iface->get_node_list (this);
}

GList*
pw_view_controller_get_link_list (GObject *this)
{
  PwViewControllerInterface *iface;
  g_return_val_if_fail (PW_IS_VIEW_CONTROLLER (this), NULL);

  iface = PW_VIEW_CONTROLLER_GET_IFACE (this);
  return iface->get_link_list (this);
}
