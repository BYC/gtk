/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_STYLE_PROVIDER_H__
#define __GTK_STYLE_PROVIDER_H__

#include <glib-object.h>
#include "gtkstyleset.h"
#include "gtkenums.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_PROVIDER          (gtk_style_provider_get_type ())
#define GTK_STYLE_PROVIDER(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_STYLE_PROVIDER, GtkStyleProvider))
#define GTK_IS_STYLE_PROVIDER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_STYLE_PROVIDER))
#define GTK_STYLE_PROVIDER_GET_IFACE(o)  (G_TYPE_INSTANCE_GET_INTERFACE ((o), GTK_TYPE_STYLE_PROVIDER, GtkStyleProviderIface))

typedef struct GtkStyleProviderIface GtkStyleProviderIface;
typedef struct GtkStyleProvider GtkStyleProvider; /* dummy typedef */

typedef enum {
  GTK_SELECTOR_TYPE_PATH,
  GTK_SELECTOR_TYPE_NAME,
  GTK_SELECTOR_TYPE_CLASS_PATH,
  GTK_SELECTOR_TYPE_CLASS_NAME
} GtkSelectorType;

struct GtkStyleProviderIface
{
  GTypeInterface g_iface;

  GtkStyleSet * (* get_style) (GtkStyleProvider *provider);
};

GType gtk_style_provider_get_type (void) G_GNUC_CONST;

GtkStyleSet *gtk_style_provider_get_style (GtkStyleProvider *provider);


G_END_DECLS

#endif /* __GTK_STYLE_PROVIDER_H__ */