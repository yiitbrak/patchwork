#pragma once

#include <gtk/gtk.h>
#include "pw-pad.h"

G_BEGIN_DECLS

#define PW_TYPE_NODE (pw_node_get_type())

G_DECLARE_DERIVABLE_TYPE (PwNode, pw_node, PW, NODE, GtkWidget)

struct _PwNodeClass
{
        GtkWidgetClass parent_class;
};

PwNode *pw_node_new (guint id);

guint32 pw_node_get_id(PwNode *self);

void pw_node_get_pos(PwNode* self, gint* X, gint* Y);

void pw_node_set_xpos(PwNode* self, gint X);

void pw_node_set_ypos(PwNode* self, gint Y);

const char* pw_node_get_title(PwNode* self);

void pw_node_set_title(PwNode* self, const char* title);

void pw_node_append_pad(PwNode* self, PwPad* pad, int direction);

G_END_DECLS
