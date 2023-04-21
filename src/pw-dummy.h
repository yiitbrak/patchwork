#pragma once

#include <glib-object.h>
#include "pw-canvas.h"

G_BEGIN_DECLS

#define PW_TYPE_DUMMY (pw_dummy_get_type())

G_DECLARE_FINAL_TYPE (PwDummy, pw_dummy, PW, DUMMY, GObject)

PwDummy *pw_dummy_new (void);

G_END_DECLS
