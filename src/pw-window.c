/* patchwork-window.c
 *
 * Copyright 2023 Yiğit
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "pw-canvas.h"
#include "pw-window.h"
#include "pw-zoom-entry.h"

struct _PwWindow
{
  AdwApplicationWindow parent_instance;

  GtkCssProvider *prov;

  /* Template widgets */
  GtkHeaderBar *header_bar;
  PwCanvas *main_vp;
  PwZoomEntry *zoom_entry;
};

G_DEFINE_FINAL_TYPE (PwWindow, pw_window, ADW_TYPE_APPLICATION_WINDOW)

static void
pw_window_dispose(GObject* object)
{
  PwWindow* self = PW_WINDOW(object);

  gtk_widget_dispose_template(GTK_WIDGET(self), PW_TYPE_WINDOW);
  g_clear_object(&self->prov);

  G_OBJECT_CLASS(pw_window_parent_class)->dispose(object);
}

static void
pw_window_zoom_value_changed(PwWindow *self, GtkAdjustment *adj)
{
  pw_canvas_set_zoom(self->main_vp, gtk_adjustment_get_value(adj)/100);
}

static void
pw_window_class_init (PwWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = pw_window_dispose;


  gtk_widget_class_set_template_from_resource (
      widget_class, "/org/nidi/patchwork/res/ui/pw-window.ui");
  gtk_widget_class_bind_template_child (widget_class, PwWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, PwWindow, main_vp);
  gtk_widget_class_bind_template_child (widget_class, PwWindow, zoom_entry);
  gtk_widget_class_bind_template_callback(widget_class, pw_window_zoom_value_changed);
}

static void
theme_notify_cb (AdwStyleManager* man, GParamSpec* pspec, PwWindow* self)
{
  gboolean is_dark = adw_style_manager_get_dark(man);
  GdkDisplay *disp = gtk_widget_get_display (GTK_WIDGET (self));

  if(is_dark){
    gtk_css_provider_load_from_resource(self->prov, "/org/nidi/patchwork/res/css/colors-dark.css");
  } else {
    gtk_css_provider_load_from_resource(self->prov, "/org/nidi/patchwork/res/css/colors-light.css");
  }
}

static void
pw_window_init (PwWindow *self)
{
  GtkWidget* widget = GTK_WIDGET(self);
  gtk_widget_init_template (GTK_WIDGET (self));

  GdkDisplay *disp = gtk_widget_get_display (GTK_WIDGET (self));
  self->prov = gtk_css_provider_new();

  AdwStyleManager *man =  adw_style_manager_get_default ();
  g_signal_connect(man, "notify::dark", G_CALLBACK(theme_notify_cb), self);
  theme_notify_cb(man, NULL, self);

  gtk_style_context_add_provider_for_display(disp, GTK_STYLE_PROVIDER(self->prov),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GtkCssProvider* main_css = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(main_css, "/org/nidi/patchwork/res/css/main.css");

  gtk_style_context_add_provider_for_display(disp, GTK_STYLE_PROVIDER(main_css),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
