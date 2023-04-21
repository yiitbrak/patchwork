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

struct _PwWindow
{
  AdwApplicationWindow parent_instance;

  /* Template widgets */
  GtkHeaderBar *header_bar;
  PwCanvas *main_vp;
};

G_DEFINE_FINAL_TYPE (PwWindow, pw_window, ADW_TYPE_APPLICATION_WINDOW)

static void
pw_window_class_init (PwWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (
      widget_class, "/org/nidi/patchwork/res/ui/pw-window.ui");
  gtk_widget_class_bind_template_child (widget_class, PwWindow, header_bar);
  gtk_widget_class_bind_template_child (widget_class, PwWindow, main_vp);
}

static void
pw_window_init (PwWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  GtkCssProvider *prov = gtk_css_provider_new ();

  gtk_css_provider_load_from_resource (prov,
                                       "/org/nidi/patchwork/res/css/main.css");

  GdkDisplay *disp = gtk_widget_get_display (GTK_WIDGET (self));
  gtk_style_context_add_provider_for_display (
      disp, GTK_STYLE_PROVIDER (prov),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
