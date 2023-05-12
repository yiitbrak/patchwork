#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define PW_TYPE_ZOOM_ENTRY (pw_zoom_entry_get_type())

G_DECLARE_FINAL_TYPE (PwZoomEntry, pw_zoom_entry, PW, ZOOM_ENTRY, GtkWidget)

PwZoomEntry *pw_zoom_entry_new(GtkAdjustment *adjustment);

void pw_zoom_entry_set_adjustment(PwZoomEntry *self, GtkAdjustment *adjustment);

GtkAdjustment *pw_zoom_entry_get_adjustment(PwZoomEntry *self);

G_END_DECLS
