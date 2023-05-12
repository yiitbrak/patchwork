#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PW_TYPE_CANVAS (pw_canvas_get_type ())

G_DECLARE_DERIVABLE_TYPE (PwCanvas, pw_canvas, PW, CANVAS, GtkWidget)

struct _PwCanvasClass
{
  GtkWidgetClass parent_instance;
};

PwCanvas *pw_canvas_new (void);

gdouble pw_canvas_get_zoom (PwCanvas *self);

void pw_canvas_set_zoom (PwCanvas *self, gdouble zoom);

G_END_DECLS
