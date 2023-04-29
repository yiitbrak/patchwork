#pragma once

#include "pw-canvas.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define PW_TYPE_PIPEWIRE (pw_pipewire_get_type ())

G_DECLARE_FINAL_TYPE (PwPipewire, pw_pipewire, PW, PIPEWIRE, GObject)

PwPipewire *pw_pipewire_new (PwCanvas *canvas);

void pw_pipewire_run (PwPipewire *self);

G_END_DECLS
