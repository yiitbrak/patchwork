#pragma once

#include "pw-canvas.h"
#include "pw-node.h"
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
  guint32 id;
  const char *title;
} PwNodeData;

typedef struct
{
  guint32 id;
  guint32 parent_id;
  const char *name;
  gint direction;
} PwPadData;

typedef struct
{
  guint32 id;
  guint32 in, out; // IDs
} PwLinkData;

#define PW_TYPE_VIEW_CONTROLLER (pw_view_controller_get_type ())

G_DECLARE_INTERFACE (PwViewController, pw_view_controller, PW, VIEW_CONTROLLER,
                     GObject)

struct _PwViewControllerInterface
{
  GTypeInterface parent;

  void (*add_node) (GObject *self, PwCanvas *canv, PwNodeData nod);
  void (*add_pad) (GObject *self, PwPadData pad);
  void (*add_link) (GObject *self, PwLinkData link);
  PwNode* (*get_node_by_id) (GObject* self, gint id);
  PwPad* (*get_pad_by_id) (GObject* self, gint id);
  void (*node_to_front) (GObject *self, gint nod);
  GList* (*get_node_list) (GObject *self);
  GList* (*get_link_list) (GObject *self);
};

void pw_view_controller_add_node (GObject *self, PwCanvas *canv,
                                  PwNodeData nod);
void pw_view_controller_add_link (GObject *self, PwLinkData link);
void pw_view_controller_add_pad (GObject *self, PwPadData pad);
PwNode* pw_view_controller_get_node_by_id (GObject* self, gint id);
PwPad* pw_view_controller_get_pad_by_id (GObject* self, gint id);
void pw_view_controller_node_to_front(GObject *self, gint nod);
GList* pw_view_controller_get_node_list (GObject *self);
GList* pw_view_controller_get_link_list (GObject *self);

G_END_DECLS
