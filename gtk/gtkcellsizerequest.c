/* gtkcellsizerequest.c
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <config.h>
#include "gtkcellrenderer.h"
#include "gtkcellsizerequest.h"
#include "gtkintl.h"


#define DEBUG_CELL_SIZE_REQUEST 0


typedef GtkCellSizeRequestIface GtkCellSizeRequestInterface;
G_DEFINE_INTERFACE (GtkCellSizeRequest, gtk_cell_size_request, GTK_TYPE_CELL_RENDERER);


static void
gtk_cell_size_request_default_init (GtkCellSizeRequestInterface *iface)
{
}


/**
 * gtk_cell_size_request_get_request_mode:
 * @cell: a #GtkCellSizeRequest instance
 *
 * Gets whether the cell renderer prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * Returns: The #GtkSizeRequestMode preferred by this renderer.
 *
 * Since: 3.0
 */
GtkSizeRequestMode
gtk_cell_size_request_get_request_mode (GtkCellSizeRequest *cell)
{
  GtkCellSizeRequestIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell), FALSE);

  iface = GTK_CELL_SIZE_REQUEST_GET_IFACE (cell);
  if (iface->get_request_mode)
    return iface->get_request_mode (cell);

  /* By default cell renderers are height-for-width. */
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

/**
 * gtk_cell_size_request_get_width:
 * @cell: a #GtkCellSizeRequest instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @minimum_size: location to store the minimum size, or %NULL
 * @natural_size: location to store the natural size, or %NULL
 *
 * Retreives a renderer's natural size when rendered to @widget.
 *
 * Since: 3.0
 */
void
gtk_cell_size_request_get_width (GtkCellSizeRequest *cell,
				 GtkWidget          *widget,
				 gint               *minimum_size,
				 gint               *natural_size)
{
  GtkCellSizeRequestIface *iface;
  gint width;

  g_return_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), &width, NULL);

  if (width < 0)
    {
      iface = GTK_CELL_SIZE_REQUEST_GET_IFACE (cell);
      iface->get_width (cell, widget, minimum_size, natural_size);
    }
  else
    {
      if (minimum_size)
	*minimum_size = width;
      if (natural_size)
	*natural_size = width;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s returning minimum width: %d and natural width: %d",
	     G_OBJECT_TYPE_NAME (cell), 
	     minimum_size ? *minimum_size : 20000, 
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_cell_size_request_get_height:
 * @cell: a #GtkCellSizeRequest instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @minimum_size: location to store the minimum size, or %NULL
 * @natural_size: location to store the natural size, or %NULL
 *
 * Retreives a renderer's natural size when rendered to @widget.
 *
 * Since: 3.0
 */
void
gtk_cell_size_request_get_height (GtkCellSizeRequest *cell,
				  GtkWidget          *widget,
				  gint               *minimum_size,
				  gint               *natural_size)
{
  GtkCellSizeRequestIface *iface;
  gint height;

  g_return_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), NULL, &height);

  if (height < 0)
    {
      iface = GTK_CELL_SIZE_REQUEST_GET_IFACE (cell);
      iface->get_height (cell, widget, minimum_size, natural_size);
    }
  else
    {
      if (minimum_size)
	*minimum_size = height;
      if (natural_size)
	*natural_size = height;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s returning minimum height: %d and natural height: %d",
	     G_OBJECT_TYPE_NAME (cell), 
	     minimum_size ? *minimum_size : 20000, 
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_cell_size_request_get_width_for_height:
 * @cell: a #GtkCellSizeRequest instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @height: the size which is available for allocation
 * @minimum_width: location for storing the minimum size, or %NULL
 * @natural_width: location for storing the preferred size, or %NULL
 *
 * Retreives a cell renderers's minimum and natural width if it were rendered to 
 * @widget with the specified @height.
 *
 * Since: 3.0
 */
void
gtk_cell_size_request_get_width_for_height (GtkCellSizeRequest *cell,
					    GtkWidget          *widget,
					    gint                height,
					    gint               *minimum_width,
					    gint               *natural_width)
{
  GtkCellSizeRequestIface *iface;
  gint width;

  g_return_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_width || NULL != natural_width);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), &width, NULL);

  if (width < 0)
    {
      iface = GTK_CELL_SIZE_REQUEST_GET_IFACE (cell);
      iface->get_width_for_height (cell, widget, height, minimum_width, natural_width);
    }
  else
    {
      if (minimum_width)
	*minimum_width = width;
      if (natural_width)
	*natural_width = width;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s width for height: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), height,
	     minimum_width ? *minimum_width : 20000, 
	     natural_width ? *natural_width : 20000);
#endif
}

/**
 * gtk_cell_size_request_get_height_for_width:
 * @cell: a #GtkCellSizeRequest instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @width: the size which is available for allocation
 * @minimum_height: location for storing the minimum size, or %NULL
 * @natural_height: location for storing the preferred size, or %NULL
 *
 * Retreives a cell renderers's minimum and natural height if it were rendered to 
 * @widget with the specified @width.
 *
 * Since: 3.0
 */
void
gtk_cell_size_request_get_height_for_width (GtkCellSizeRequest *cell,
					    GtkWidget          *widget,
					    gint                width,
					    gint               *minimum_height,
					    gint               *natural_height)
{
  GtkCellSizeRequestIface *iface;
  gint height;

  g_return_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_height || NULL != natural_height);

  gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), NULL, &height);

  if (height < 0)
    {
      iface = GTK_CELL_SIZE_REQUEST_GET_IFACE (cell);
      iface->get_height_for_width (cell, widget, width, minimum_height, natural_height);
    }
  else
    {
      if (minimum_height)
	*minimum_height = height;
      if (natural_height)
	*natural_height = height;
    }

#if DEBUG_CELL_SIZE_REQUEST
  g_message ("%s height for width: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), width,
	     minimum_height ? *minimum_height : 20000, 
	     natural_height ? *natural_height : 20000);
#endif
}

/**
 * gtk_cell_size_request_get_size:
 * @cell: a #GtkCellSizeRequest instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @request_natural: Whether to base the contextual request off of the
 *     base natural or the base minimum
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size of a cell taking
 * into account the widget's preference for height-for-width management.
 *
 * If request_natural is specified, the non-contextual natural value will
 * be used to make the contextual request; otherwise the minimum will be used.
 *
 * Since: 3.0
 */
void
gtk_cell_size_request_get_size (GtkCellSizeRequest *cell,
				GtkWidget          *widget,
				GtkRequisition     *minimum_size,
				GtkRequisition     *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_CELL_SIZE_REQUEST (cell));

  if (gtk_cell_size_request_get_request_mode (cell) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_cell_size_request_get_width (cell, widget, &min_width, &nat_width);

      if (minimum_size)
	{
	  minimum_size->width = min_width;
	  gtk_cell_size_request_get_height_for_width (cell, widget, min_width,
						      &minimum_size->height, NULL);
	}

      if (natural_size)
	{
	  natural_size->width = nat_width;
	  gtk_cell_size_request_get_height_for_width (cell, widget, nat_width,
						      NULL, &natural_size->height);
	}
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT */
    {
      gtk_cell_size_request_get_height (cell, widget, &min_height, &nat_height);

      if (minimum_size)
	{
	  minimum_size->height = min_height;
	  gtk_cell_size_request_get_width_for_height (cell, widget, min_height,
						      &minimum_size->width, NULL);
	}

      if (natural_size)
	{
	  natural_size->height = nat_height;
	  gtk_cell_size_request_get_width_for_height (cell, widget, nat_height,
						      NULL, &natural_size->width);
	}
    }
}