#pragma once

#include <gtk/gtk.h>
#include "pw-enums.h"

G_BEGIN_DECLS

#define PW_TYPE_PAD (pw_pad_get_type())

G_DECLARE_DERIVABLE_TYPE (PwPad, pw_pad, PW, PAD, GtkWidget)

struct _PwPadClass
{
        GtkWidgetClass parent_class;
};

PwPad *pw_pad_new (guint32 id, PwPadDirection dir);

PwPad *pw_pad_new_with_name (guint32 id, PwPadDirection dir,const char* name);

guint32 pw_pad_get_id(PwPad* self);

guint32 pw_pad_get_parent_id (PwPad *self);

PwPadDirection pw_pad_get_direction(PwPad* self);

G_END_DECLS
