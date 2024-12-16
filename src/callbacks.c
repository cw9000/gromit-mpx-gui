/*
 * Gromit-MPX -- a program for painting on the screen
 *
 * Gromit Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * Gromit-MPX Copyright (C) 2009,2010 Christian Beier <dontmind@freeshell.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include "main.h"
#include "input.h"
#include "callbacks.h"
#include "config.h"
#include "drawing.h"
#include "build-config.h"
#include "coordlist_ops.h"
#include <kpathsea/c-std.h>


typedef struct{
  gint widget_id;
  gint radio_nb;
} CustomData;

gboolean on_expose (GtkWidget *widget,
		    cairo_t* cr,
		    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got draw event\n");

  cairo_save (cr);
  cairo_set_source_surface (cr, data->backbuffer, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  cairo_restore (cr);

  if (data->debug) {
      // draw a pink background to know where the window is
      cairo_save (cr);
      cairo_set_source_rgba(cr, 64, 0, 64, 32);
      cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
      cairo_paint (cr);
      cairo_restore (cr);
  }

  return TRUE;
}




gboolean on_configure (GtkWidget *widget,
		       GdkEventExpose *event,
		       gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got configure event\n");

  return TRUE;
}



void on_screen_changed(GtkWidget *widget,
		       GdkScreen *previous_screen,
		       gpointer   user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got screen-changed event\n");

  GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET (widget));
  GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  gtk_widget_set_visual (widget, visual);
}



void on_monitors_changed ( GdkScreen *screen,
			   gpointer   user_data)
{
  GromitData *data = (GromitData *) user_data;

  // get new sizes
  data->width = gdk_screen_get_width (data->screen);
  data->height = gdk_screen_get_height (data->screen);

  if(data->debug)
    g_printerr("DEBUG: screen size changed to %d x %d!\n", data->width, data->height);

  // change size
  gtk_widget_set_size_request(GTK_WIDGET(data->win), data->width, data->height);
  // try to set transparent for input
  cairo_region_t* r =  cairo_region_create();
  gtk_widget_input_shape_combine_region(data->win, r);
  cairo_region_destroy(r);

  /* recreate the shape surface */
  cairo_surface_t *new_shape = cairo_image_surface_create(CAIRO_FORMAT_ARGB32 ,data->width, data->height);
  cairo_t *cr = cairo_create (new_shape);
  cairo_set_source_surface (cr, data->backbuffer, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  cairo_surface_destroy(data->backbuffer);
  data->backbuffer = new_shape;

  // recreate auxiliary backbuffer
  cairo_surface_destroy(data->aux_backbuffer);
  data->aux_backbuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, data->width, data->height);

  /*
     these depend on the shape surface
  */
  GHashTableIter it;
  gpointer value;
  g_hash_table_iter_init (&it, data->tool_config);
  while (g_hash_table_iter_next (&it, NULL, &value))
    paint_context_free(value);
  g_hash_table_remove_all(data->tool_config);


  parse_config(data); // also calls paint_context_new() :-(


  data->default_pen = paint_context_new (data, GROMIT_PEN, data->red, 7, 0, GROMIT_ARROW_END,
                                         5, 10, 15, 25, 1, 0, G_MAXUINT);
  data->default_eraser = paint_context_new (data, GROMIT_ERASER, data->red, 75, 0, GROMIT_ARROW_END,
                                            5, 10, 15, 25, 1, 0, G_MAXUINT);

  if(!data->composited) // set shape
    {
      cairo_region_t* r = gdk_cairo_region_create_from_surface(data->backbuffer);
      gtk_widget_shape_combine_region(data->win, r);
      cairo_region_destroy(r);
    }

  setup_input_devices(data);


  gtk_widget_show_all (data->win);
}



void on_composited_changed ( GdkScreen *screen,
			   gpointer   user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(data->debug)
    g_printerr("DEBUG: got composited-changed event\n");

  data->composited = gdk_screen_is_composited (data->screen);

  if(data->composited)
    {
      // undo shape
      gtk_widget_shape_combine_region(data->win, NULL);
      // re-apply transparency
      gtk_widget_set_opacity(data->win, 0.75);
    }

  // set anti-aliasing
  GHashTableIter it;
  gpointer value;
  g_hash_table_iter_init (&it, data->tool_config);
  while (g_hash_table_iter_next (&it, NULL, &value))
    {
      GromitPaintContext *context = value;
      cairo_set_antialias(context->paint_ctx, data->composited ? CAIRO_ANTIALIAS_DEFAULT : CAIRO_ANTIALIAS_NONE);
    }


  GdkRectangle rect = {0, 0, data->width, data->height};
  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
}



void on_clientapp_selection_get (GtkWidget          *widget,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time,
				 gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;

  gchar *ans = "";

  if(data->debug)
    g_printerr("DEBUG: clientapp received request.\n");


  if (gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA || gtk_selection_data_get_target(selection_data) == GA_LINEDATA)
    {
      ans = data->clientdata;
    }

  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)ans, strlen (ans));
}


void on_clientapp_selection_received (GtkWidget *widget,
				      GtkSelectionData *selection_data,
				      guint time,
				      gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  /* If someone has a selection for us, Gromit is already running. */

  if(gtk_selection_data_get_data_type(selection_data) == GDK_NONE)
    data->client = 0;
  else
    data->client = 1;

  gtk_main_quit ();
}



static float line_thickener = 0;


gboolean on_buttonpress (GtkWidget *win,
			 GdkEventButton *ev,
			 gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  gdouble pressure = 1;

  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);
  g_print("on_button_press\n");
  if(data->started_from_gui == TRUE)
  {
    g_print("exiting on_button_press\n");
    return TRUE;
  }

  if (ev->type == GDK_BUTTON_PRESS && ev->button == 3 && devdata->is_grabbed)
  {
    toggle_grab(data,ev->device);
    gtk_widget_show(data->gui_window);
    gtk_window_move(data->gui_window,data->gui_window_position.x,data->gui_window_position.y);
    return TRUE;
  }
  if(data->debug)
    g_printerr("DEBUG: Device '%s': Button %i Down State %d at (x,y)=(%.2f : %.2f)\n",
	       gdk_device_get_name(ev->device), ev->button, ev->state, ev->x, ev->y);

  if (!devdata->is_grabbed)
    return FALSE;

  if (gdk_device_get_source(gdk_event_get_source_device((GdkEvent *)ev)) == GDK_SOURCE_PEN) {
      /* Do not drop unprocessed motion events. Smoother drawing for pens of tablets. */
      gdk_window_set_event_compression(gtk_widget_get_window(data->win), FALSE);
  } else {
      /* For all other source types, set back to default. Otherwise, lines were only
	 fully drawn to the end on button release. */
      gdk_window_set_event_compression(gtk_widget_get_window(data->win), TRUE);
  }

  /* See GdkModifierType. Am I fixing a Gtk misbehaviour???  */
  ev->state |= 1 << (ev->button + 7);


  if (ev->state != devdata->state ||
      devdata->lastslave != gdk_event_get_source_device ((GdkEvent *) ev))
    select_tool (data, ev->device, gdk_event_get_source_device ((GdkEvent *) ev), ev->state);
  if (data->use_graphical_menu_items)
    select_tool(data,ev->device,gdk_event_get_source_device((GdkEvent *) ev),ev->state);
  g_print("set type");
  GromitPaintType type = devdata->cur_context->type;

  // store original state to have dynamic update of line and rect
  if (type == GROMIT_LINE || type == GROMIT_RECT || type == GROMIT_SMOOTH || type == GROMIT_ORTHOGONAL)
    {
      copy_surface(data->aux_backbuffer, data->backbuffer);
    }

  devdata->lastx = ev->x;
  devdata->lasty = ev->y;
  devdata->motion_time = ev->time;

  snap_undo_state (data);

  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);
  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
		    (double) (devdata->cur_context->width -
			      devdata->cur_context->minwidth) +
		    devdata->cur_context->minwidth);

  if(data->maxwidth > devdata->cur_context->maxwidth)
    data->maxwidth = devdata->cur_context->maxwidth;

  if (ev->button <= 5)
    draw_line (data, ev->device, ev->x, ev->y, ev->x, ev->y);

  coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);

  return TRUE;
}


gboolean on_motion (GtkWidget *win,
		    GdkEventMotion *ev,
		    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  if(data->started_from_gui == TRUE)
    return TRUE;
  GdkTimeCoord **coords = NULL;
  gint nevents;
  int i;
  gdouble pressure = 1;
  /* get the data for this device */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);

  if (!devdata->is_grabbed)
    return FALSE;

  if(data->debug)
      g_printerr("DEBUG: Device '%s': motion to (x,y)=(%.2f : %.2f)\n", gdk_device_get_name(ev->device), ev->x, ev->y);
  g_print("on_motion\n");
  if (ev->state != devdata->state ||
      devdata->lastslave != gdk_event_get_source_device ((GdkEvent *) ev))
    select_tool (data, ev->device, gdk_event_get_source_device ((GdkEvent *) ev), ev->state);
  g_print("type=\n");
  GromitPaintType type = devdata->cur_context->type;
  g_print("get_history\n");
  gdk_device_get_history (ev->device, ev->window,
			  devdata->motion_time, ev->time,
			  &coords, &nevents);
  g_print("after history\n");

  if(!data->xinerama && nevents > 0)
    {
      g_print("line 355\n");
      if (type != GROMIT_LINE && type != GROMIT_RECT)
        {
          for (i=0; i < nevents; i++)
            {
              gdouble x, y;
              g_print("line 360\n");
              gdk_device_get_axis (ev->device, coords[i]->axes,
                                   GDK_AXIS_PRESSURE, &pressure);
              if (pressure > 0)
                {
                  data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
                                    (double) (devdata->cur_context->width -
                                              devdata->cur_context->minwidth) +
                                    devdata->cur_context->minwidth);

                  if(data->maxwidth > devdata->cur_context->maxwidth)
                    data->maxwidth = devdata->cur_context->maxwidth;
                  g_print("line 372\n");
                  gdk_device_get_axis(ev->device, coords[i]->axes,
                                      GDK_AXIS_X, &x);
                  gdk_device_get_axis(ev->device, coords[i]->axes,
                                      GDK_AXIS_Y, &y);

                  draw_line (data, ev->device, devdata->lastx, devdata->lasty, x, y);

                  coord_list_prepend (data, ev->device, x, y, data->maxwidth);
                  devdata->lastx = x;
                  devdata->lasty = y;
                  g_print("line 383\n");
                }
            }
        }
      g_print("line 388\n");
      devdata->motion_time = coords[nevents-1]->time;
      g_free (coords);
    }
g_print("line 392\n");
  /* always paint to the current event coordinate. */
  gdk_event_get_axis ((GdkEvent *) ev, GDK_AXIS_PRESSURE, &pressure);

  if (pressure > 0)
    {
      g_print("pressure\n");
      data->maxwidth = (CLAMP (pressure + line_thickener, 0, 1) *
			(double) (devdata->cur_context->width -
				  devdata->cur_context->minwidth) +
			devdata->cur_context->minwidth);

      if(data->maxwidth > devdata->cur_context->maxwidth)
	data->maxwidth = devdata->cur_context->maxwidth;

      if(devdata->motion_time > 0)
	{
          if (type == GROMIT_LINE || type == GROMIT_RECT) {
            copy_surface(data->backbuffer, data->aux_backbuffer);
            GdkRectangle rect = {0, 0, data->width, data->height};
            gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
          }
          if (type == GROMIT_LINE)
            {
              GromitArrowType atype = devdata->cur_context->arrow_type;
	      draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);
              if (devdata->cur_context->arrowsize > 0)
                {
                  GromitArrowType atype = devdata->cur_context->arrow_type;
                  gint width = devdata->cur_context->arrowsize * devdata->cur_context->width / 2;
                  gfloat direction =
                      atan2(ev->y - devdata->lasty, ev->x - devdata->lastx);
                  if (atype & GROMIT_ARROW_END)
                    draw_arrow(data, ev->device, ev->x, ev->y, width * 2, direction);
                  if (atype & GROMIT_ARROW_START)
                    draw_arrow(data, ev->device, devdata->lastx, devdata->lasty, width * 2, M_PI + direction);
                }
            }
          else if (type == GROMIT_RECT)
            {
              draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, devdata->lasty);
              draw_line (data, ev->device, ev->x, devdata->lasty, ev->x, ev->y);
              draw_line (data, ev->device, ev->x, ev->y, devdata->lastx, ev->y);
              draw_line (data, ev->device, devdata->lastx, ev->y, devdata->lastx, devdata->lasty);
            }
          else
            {
              draw_line (data, ev->device, devdata->lastx, devdata->lasty, ev->x, ev->y);
	      coord_list_prepend (data, ev->device, ev->x, ev->y, data->maxwidth);
            }
	}
    }

  if (type != GROMIT_LINE && type != GROMIT_RECT)
    {
      devdata->lastx = ev->x;
      devdata->lasty = ev->y;
    }
  devdata->motion_time = ev->time;
  g_print("finished on_motion");
  return TRUE;
}


gboolean on_buttonrelease (GtkWidget *win,
			   GdkEventButton *ev,
			   gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  g_print("on_buttonrelease\n");
  /* get the device data for this event */
  GromitDeviceData *devdata = g_hash_table_lookup(data->devdatatable, ev->device);
  GromitPaintContext *ctx = devdata->cur_context;
  if (data->use_graphical_menu_items)
  {
    g_print("use_graphical_menu_items\n");
    int index = data->current_graph_menu_tool;
    ctx = data->graph_menu_tools[index][data->current_graph_menu_type[index]];
    if (data->started_from_gui == TRUE)
    {
      g_print("data->started_from_gui\n");
      data->started_from_gui = FALSE;
      return TRUE;
    }
  }


  gfloat direction = 0;
  gint width = 0;
  if(ctx)
    width = ctx->arrowsize * ctx->width / 2;
  g_print("on_buttonrelease\n");
  if ((ev->x != devdata->lastx) ||
      (ev->y != devdata->lasty))
    on_motion(win, (GdkEventMotion *) ev, user_data);
  g_print("after on_motion_called\n");
  if (!devdata->is_grabbed)
    return FALSE;
  g_print("after is grabbed\n");
  GromitPaintType type = ctx->type;
  g_print("after typ=\n");
  if (type == GROMIT_SMOOTH || type == GROMIT_ORTHOGONAL)
    {
      gboolean joined = FALSE;
      douglas_peucker(devdata->coordlist, ctx->simplify);
      if (ctx->snapdist > 0)
        joined = snap_ends(devdata->coordlist, ctx->snapdist);
      if (type == GROMIT_SMOOTH) {
          add_points(devdata->coordlist, 200);
          devdata->coordlist = catmull_rom(devdata->coordlist, 5, joined);
      } else {
          orthogonalize(devdata->coordlist, ctx->maxangle, ctx->minlen);
          round_corners(devdata->coordlist, ctx->radius, 6, joined);
      }

      copy_surface(data->backbuffer, data->aux_backbuffer);
      GdkRectangle rect = {0, 0, data->width, data->height};
      gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);

      GList *ptr = devdata->coordlist;
      while (ptr && ptr->next)
        {
          GromitStrokeCoordinate *c1 = ptr->data;
          GromitStrokeCoordinate *c2 = ptr->next->data;
          ptr = ptr->next;
          draw_line (data, ev->device, c1->x, c1->y, c2->x, c2->y);
        }
    }
  g_print("before ctx->arrowsize\n");
  if (ctx->arrowsize != 0)
    {
      GromitArrowType atype = ctx->arrow_type;
      if (type == GROMIT_LINE)
        {
          direction = atan2 (ev->y - devdata->lasty, ev->x - devdata->lastx);
          if (atype & GROMIT_ARROW_END)
            draw_arrow(data, ev->device, ev->x, ev->y, width * 2, direction);
          if (atype & GROMIT_ARROW_START)
            draw_arrow(data, ev->device, devdata->lastx, devdata->lasty, width * 2, M_PI + direction);
        }
      else
        {
          gint x0, y0;
          if ((atype & GROMIT_ARROW_END) &&
              coord_list_get_arrow_param (data, ev->device, width * 3,
                                          GROMIT_ARROW_END, &x0, &y0, &width, &direction))
            draw_arrow (data, ev->device, x0, y0, width, direction);
          if ((atype & GROMIT_ARROW_START) &&
              coord_list_get_arrow_param (data, ev->device, width * 3,
                                          GROMIT_ARROW_START, &x0, &y0, &width, &direction)) {
            draw_arrow (data, ev->device, x0, y0, width, direction);
          }
        }
    }
  g_print("after on_button_release\n");
  coord_list_free (data, ev->device);

  return TRUE;
}

/* Remote control */
void on_mainapp_selection_get (GtkWidget          *widget,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint               time,
			       gpointer            user_data)
{
  GromitData *data = (GromitData *) user_data;

  gchar *uri = "OK";
  GdkAtom action = gtk_selection_data_get_target(selection_data);

  if(action == GA_TOGGLE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_TOGGLEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if(action == GA_LINE)
    {
      /* ask back client for device id */
      gtk_selection_convert (data->win, GA_DATA,
                             GA_LINEDATA, time);
      gtk_main(); /* Wait for the response */
    }
  else if (action == GA_VISIBILITY)
    toggle_visibility (data);
  else if (action == GA_CLEAR)
    clear_screen (data);
  else if (action == GA_RELOAD)
    setup_input_devices(data);
  else if (action == GA_QUIT)
    gtk_main_quit ();
  else if (action == GA_UNDO)
    undo_drawing (data);
  else if (action == GA_REDO)
    redo_drawing (data);
  else if (action == GA_GUIMENU)
  {
    on_menu_toggle(NULL,data);
    gtk_main_quit();
  }
  else if (action == GA_OPENTOGGLE)
  {
    g_print("\nopentoggle\n");
    if(data->gui_menu_toggle)
      on_window_close(NULL,data);
    gtk_main_quit();
  }
  else
    uri = "NOK";


  gtk_selection_data_set (selection_data,
                          gtk_selection_data_get_target(selection_data),
                          8, (guchar*)uri, strlen (uri));
}


void on_mainapp_selection_received (GtkWidget *widget,
				    GtkSelectionData *selection_data,
				    guint time,
				    gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(gtk_selection_data_get_length(selection_data) < 0)
    {
      if(data->debug)
        g_printerr("DEBUG: mainapp got no answer back from client.\n");
    }
  else
    {
      if(gtk_selection_data_get_target(selection_data) == GA_TOGGLEDATA )
        {
	  intptr_t dev_nr = strtoull((gchar*)gtk_selection_data_get_data(selection_data), NULL, 10);

          if(data->debug)
	    g_printerr("DEBUG: mainapp got toggle id '%ld' back from client.\n", (long)dev_nr);

	  if(dev_nr < 0)
	    toggle_grab(data, NULL); /* toggle all */
	  else
	    {
	      /* find dev numbered dev_nr */
	      GHashTableIter it;
	      gpointer value;
	      GromitDeviceData* devdata = NULL;
	      g_hash_table_iter_init (&it, data->devdatatable);
	      while (g_hash_table_iter_next (&it, NULL, &value))
		{
		  devdata = value;
		  if(devdata->index == dev_nr)
		    break;
		  else
		    devdata = NULL;
		}

	      if(devdata)
		toggle_grab(data, devdata->device);
	      else
		g_printerr("ERROR: No device at index %ld.\n", (long)dev_nr);
	    }
        }
      else if (gtk_selection_data_get_target(selection_data) == GA_LINEDATA)
	{

	  gchar** line_args = g_strsplit((gchar*)gtk_selection_data_get_data(selection_data), " ", 6);
	  int startX = atoi(line_args[0]);
	  int startY = atoi(line_args[1]);
	  int endX = atoi(line_args[2]);
	  int endY = atoi(line_args[3]);
	  gchar* hex_code = line_args[4];
	  int thickness = atoi(line_args[5]);

          if(data->debug)
	    {
	      g_printerr("DEBUG: mainapp got line data back from client:\n");
	      g_printerr("startX startY endX endY: %d %d %d %d\n", startX, startY, endX, endY);
	      g_printerr("color: %s\n", hex_code);
	      g_printerr("thickness: %d\n", thickness);
	    }

	  GdkRGBA* color = g_malloc (sizeof (GdkRGBA));
	  GdkRGBA *fg_color=data->red;
	  if (gdk_rgba_parse (color, hex_code))
	    {
	      fg_color = color;
	    }
	  else
	    {
	      g_printerr ("Unable to parse color. "
	      "Keeping default.\n");
	    }
	  GromitPaintContext* line_ctx =
            paint_context_new(data, GROMIT_PEN, fg_color, thickness, 0, GROMIT_ARROW_END,
                              5, 10, 15, 25, 0, thickness, thickness);

	  GdkRectangle rect;
	  rect.x = MIN (startX,endX) - thickness / 2;
	  rect.y = MIN (startY,endY) - thickness / 2;
	  rect.width = ABS (startX-endX) + thickness;
	  rect.height = ABS (startY-endY) + thickness;

	  if(data->debug)
	    g_printerr("DEBUG: draw line from %d %d to %d %d\n", startX, startY, endX, endY);

	  cairo_set_line_width(line_ctx->paint_ctx, thickness);
	  cairo_move_to(line_ctx->paint_ctx, startX, startY);
	  cairo_line_to(line_ctx->paint_ctx, endX, endY);
	  cairo_stroke(line_ctx->paint_ctx);

	  data->modified = 1;
	  gdk_window_invalidate_rect(gtk_widget_get_window(data->win), &rect, 0);
	  data->painted = 1;

	  g_free(line_ctx);
	  g_free (color);
	}
    }

  gtk_main_quit ();
}


void on_device_removed (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(gdk_device_get_device_type(device) != GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;

  if(data->debug)
    g_printerr("DEBUG: device '%s' removed\n", gdk_device_get_name(device));

  setup_input_devices(data);
}

void on_device_added (GdkDeviceManager *device_manager,
		      GdkDevice        *device,
		      gpointer          user_data)
{
  GromitData *data = (GromitData *) user_data;

  if(gdk_device_get_device_type(device) != GDK_DEVICE_TYPE_MASTER
     || gdk_device_get_n_axes(device) < 2)
    return;

  if(data->debug)
    g_printerr("DEBUG: device '%s' added\n", gdk_device_get_name(device));

  setup_input_devices(data);
}
gboolean on_move_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    // Get the window associated with the button (assumes the button is inside the window)
    GtkWidget *window = gtk_widget_get_toplevel(widget);


    // Begin dragging the window if the left mouse button is pressed
    if (event->button == 1) {

        gtk_window_begin_move_drag(GTK_WINDOW(window), event->button,
                                   event->x_root, event->y_root, event->time);

        return TRUE; // Event handled
    }

    return FALSE; // Event not handled
}
void on_window_close(GtkWidget *button, gpointer user_data) {
    
    g_print("\non_window_close()\n");
    GromitData * data = (GromitData *) user_data;
    data->use_graphical_menu_items = FALSE;
    data->gui_menu_toggle = FALSE;
    save_values(data);
    //GtkWidget *window = GTK_WIDGET(user_data);  // Get the window passed as user data
    gtk_window_close(GTK_WINDOW(data->gui_window));  // Close the window
}
/*
static gboolean on_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data) {
    // Set custom tooltip text
    gtk_tooltip_set_text(tooltip, "hello");
    g_print("in custom tooltip");
    // Optionally set custom geometry (e.g., position tooltip relative to widget)

    gtk_tooltip_set_tip_area(tooltip, &(GdkRectangle){x , y, 30, 15});

    return TRUE; // Return TRUE to indicate that we handled the tooltip
}
*/

static void show_hide_additional(GtkWidget *button, gpointer user_data){
  g_print("a0");
  static gboolean are_widgets_hidden = TRUE;  // Track the current state
  //first get the button's vbox so we can go through it's children
  GtkBox *vbox = gtk_widget_get_parent(button);
  g_print("a1");
  GList * children = gtk_container_get_children(vbox);
  gboolean button_encountered = FALSE;
  g_print("a2");
  for (GList *iter = children;iter!=NULL;iter=iter->next)
  {
     g_print("a3");
    CustomData * data = (CustomData *)g_object_get_data(iter->data,"custom-data");
    if (data == NULL)
      continue;
    if (data->widget_id == 99)
    {
        button_encountered = TRUE;
        continue;
    }
     g_print("a4");
    if (button_encountered)
    {
        if (are_widgets_hidden) {
        // Show widgets and reclaim space
             g_print("a5");
            gtk_widget_set_no_show_all(iter->data, FALSE);  // Allow showing the widget in layout
            gtk_widget_show(iter->data);  // Show the widget
        }

        else {
            // Hide widgets and reclaim space
            g_print("a6");
            gtk_widget_set_no_show_all(iter->data, TRUE);  // Ignore widget in layout
            gtk_widget_hide(iter->data);  // Hide the widget
        }

    }
  }
    // Toggle the hidden state
    are_widgets_hidden = !are_widgets_hidden;
  if (are_widgets_hidden)
  {
     g_print("a7");
     GtkWidget *image = gtk_image_new_from_icon_name("arrow-right", GTK_ICON_SIZE_BUTTON);
     gtk_button_set_image(button,image);
  }
  else
  {
     g_print("a8");
     GtkWidget *image = gtk_image_new_from_icon_name("arrow-down", GTK_ICON_SIZE_BUTTON);
     gtk_button_set_image(button,image);

  }

}
void limit_size_vbox(GtkWidget *widget, GdkRectangle *allocation, gpointer data) {
    gint max_width = 70;
    gint max_height = 20;

    allocation->width = MIN(allocation->width, max_width);
    //allocation->height = MIN(allocation->height, max_height);

    // Apply the new size
    gtk_widget_size_allocate(widget, allocation);
}
void print_box_contents(GtkBox * box)
{
  GList * children = gtk_container_get_children(box);
  for(GList *iter = children;iter!=NULL;iter=iter->next)
  {
    g_print("\n%s\n",G_OBJECT_TYPE_NAME(iter->data));
  }
}
void print_list(GList * list,gchar *title)
{
  g_print("\n%s\n",title);
    for (GList *iter = list; iter != NULL; iter = iter->next) {
        g_print("%d\n", GPOINTER_TO_INT(iter->data));
    }
}
void * add_widget_to_other_box_by_idx(GtkBox *oldBox, GtkBox *newBox,int idx,GList ** items)
{
   g_print("746: idx: %d",idx);
   print_list(*items,"items");
   *items = g_list_delete_link(*items, *items);
   GList * children = gtk_container_get_children(oldBox);
   GtkWidget * widget = g_list_nth(children,idx)->data;
   g_object_ref(widget);
   gtk_container_remove(oldBox,widget);
   gtk_box_pack_start(newBox,widget,FALSE,FALSE,0);
   print_list(*items,"updated items");
}
void check_saved(GtkBox *vbox)
{

}
/*

    // Save to a file
    if (!g_key_file_save_to_file(key_file, "config.ini", &error)) {
        g_printerr("Error saving config file: %s\n", error->message);
        g_clear_error(&error);
    }

    // Read the file
    if (!g_key_file_load_from_file(key_file, "config.ini", G_KEY_FILE_NONE, &error)) {
        g_printerr("Error loading config file: %s\n", error->message);
        g_clear_error(&error);
    } else {
        int width = g_key_file_get_integer(key_file, "Settings", "WindowWidth", &error);
        int height = g_key_file_get_integer(key_file, "Settings", "WindowHeight", &error);
        char *theme = g_key_file_get_string(key_file, "Settings", "Theme", &error);

        if (!error) {
            g_print("Width: %d, Height: %d, Theme: %s\n", width, height, theme);
        }

        g_free(theme);
    }

    g_key_file_free(key_file);
*/
void add_tool_to_key_file(GKeyFile *key_file,gchar* tool_str,
                          GromitPaintContext *tool_type,GromitData *data){
      g_print("add_tool_to_key_file");
      g_key_file_set_integer(key_file,tool_str,"type",tool_type->type);
      g_key_file_set_integer(key_file,tool_str,"width",tool_type->width);
      g_key_file_set_double(key_file,tool_str,"arrow_size",tool_type->arrowsize);
      g_key_file_set_integer(key_file,tool_str,"arrow_type",tool_type->arrow_type);
      g_key_file_set_integer(key_file,tool_str,"minwidth",tool_type->minwidth);
      g_key_file_set_integer(key_file,tool_str,"maxwidth",tool_type->maxwidth);
      g_key_file_set_integer(key_file,tool_str,"radius",tool_type->radius);
      g_key_file_set_integer(key_file,tool_str,"minlen",tool_type->minlen);
      g_key_file_set_integer(key_file,tool_str,"maxangle",tool_type->maxangle);
      g_key_file_set_integer(key_file,tool_str,"simplify",tool_type->simplify);
      g_key_file_set_integer(key_file,tool_str,"snapdist",tool_type->snapdist);
      gdouble color[] = {tool_type->paint_color->red,tool_type->paint_color->green,
                          tool_type->paint_color->blue,tool_type->paint_color->alpha};
      guint length = sizeof(color) / sizeof(color[0]);
      g_key_file_set_double_list(key_file,tool_str,"paint_color",color,length);
      g_key_file_set_double(key_file,tool_str,"pressure",tool_type->pressure);


}
void add_general_data_to_key_file(GKeyFile *key_file,GromitData *data)
{
   g_key_file_set_integer(key_file,"General","current_graph_menu_tool",data->current_graph_menu_tool);
   guint length = sizeof(data->current_graph_menu_type) / sizeof(data->current_graph_menu_type[0]);
   g_key_file_set_integer_list(key_file,"General","current_graph_menu_type",data->current_graph_menu_type,length);
}
void save_values(GromitData * data)
{
    g_print("save_values");
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    gchar * tool_str;

    for(int tool_nb=0;tool_nb<GROMIT_NUMBER_OF_GUI_TOOLS;tool_nb++)
    {

      for(int type=GROMIT_PEN;type<GROMIT_NUMBER_OF_PAINT_TYPES;type++)
      {
        tool_str = g_strdup_printf("Tool%d_%s",tool_nb,data->paint_types_str[type]); // keys have format "Tool<nb>_<type>"
        add_tool_to_key_file(key_file,tool_str,data->graph_menu_tools[tool_nb][type],data);
      }
    }
      add_general_data_to_key_file(key_file,data);
      const gchar *filename = g_strdup_printf("%s/.gromit_config",g_get_home_dir());
    if (!g_key_file_save_to_file(key_file, filename, &error)) {
        g_printerr("Error saving config file: %s\n", error->message);
        g_clear_error(&error);
    }
}
gboolean on_vbox_changed(GtkWidget *widget, gpointer userdata)
{
    g_print("on_vbox_chg");
    GromitData *data = (GromitData *) userdata;
    CustomData * custom_data = (CustomData *) g_object_get_data(widget,"custom-data");
    GromitPaintContext *context = data->graph_menu_tools[custom_data->radio_nb][data->current_graph_menu_type[custom_data->radio_nb]];
    switch (custom_data->widget_id)
    {
      case GROMIT_PAINT_COLOR: //color
              GdkRGBA *color = g_malloc(sizeof(GdkRGBA));
              gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), color);
              context->paint_color = color;
              break;
      case GROMIT_WIDTH: //size
              GtkAdjustment *adjustment = gtk_range_get_adjustment(GTK_RANGE(widget));
              context->width = (guint) gtk_adjustment_get_value(adjustment);
              break;
      case GROMIT_ARROW_TYPE: //arrow type
              context->arrow_type = (GromitArrowType) gtk_combo_box_get_active(widget);
              break;
      case GROMIT_ARROWSIZE: //arrow size
              context->arrowsize = (gfloat) gtk_range_get_value(GTK_RANGE(widget));
              break;
      case GROMIT_SIMPLIFY: //simplify
              context->simplify = (guint) gtk_range_get_value(widget);
              break;
      case GROMIT_RADIUS: //radius
              context->radius = (guint) gtk_range_get_value(widget);
              break;
      case GROMIT_MINLEN: //min-length
              context->minlen = (guint) gtk_range_get_value(widget);
              break;
      case GROMIT_SNAPDIST: //snap
              context->snapdist = (guint) gtk_range_get_value(widget);
              break;
    }
    g_print("finished");
    //save_values(data);
    make_paint_ctx(context,data);

}
GtkBox * create_vbox(GtkBox *vbox,GList * capabilities ,GromitPaintType type,gint index,GromitData * data)
{
  g_print("create_vbox");
  GtkBox *vboxOld = vbox;
  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
  gtk_widget_set_name(vbox,"vbox");


// Connect the signal
g_signal_connect(vbox, "size-allocate", G_CALLBACK(limit_size_vbox), NULL);

  GList * items = NULL;
  GList * children = gtk_container_get_children(vboxOld);
  GtkWidget * child = children->data;
  g_object_ref(child);
  gtk_container_remove(vboxOld,child);
  children = gtk_container_get_children(vboxOld);

  for (GList *iter=children;iter != NULL; iter=iter->next)
  {
    CustomData * custom_data = (CustomData *)g_object_get_data(iter->data,"custom-data");
    if (custom_data != NULL)
      items = g_list_append(items,GINT_TO_POINTER(custom_data->widget_id));
  }

  gtk_box_pack_start(vbox,child,FALSE,FALSE,0);
  

  gboolean additional_items=FALSE;
  GtkWidget *overflow_button = gtk_button_new();
  int attr;
  GromitPaintContext *context = data->graph_menu_tools[index][type];
  for (GList *iter = capabilities; iter != NULL; iter=iter->next)
  {
    attr = GPOINTER_TO_INT(iter->data);
    int scale_min=1;
    int scale_max=50;
    int scale_step=1;
    int scale_p_step=10;
    if (attr == GROMIT_PAINT_COLOR)
    {
       g_print("color");
       GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
       if (TRUE) {
          GtkColorButton *colorbutton = gtk_color_button_new_with_rgba(context->paint_color);
          CustomData * custom_data = g_new(CustomData, 1);
          custom_data->widget_id = GROMIT_PAINT_COLOR;
          custom_data->radio_nb = index;

          // Store the custom data in the GtkBox using a unique key
           g_object_set_data(G_OBJECT(colorbutton), "custom-data", custom_data);
          gtk_widget_set_name(colorbutton,"vbox-item");
          g_signal_connect(colorbutton,"color-set",G_CALLBACK(on_vbox_changed),data);
          gtk_box_pack_start(vbox,colorbutton,FALSE,FALSE,0);
       }
       else
       {
          int idx = g_list_position(items, found_node);
          add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
       }
    }
    else if (attr == GROMIT_WIDTH)
    {
      g_print("size");
      GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_WIDTH));
      if (TRUE) {
      if(type == GROMIT_ERASER)
      {
        scale_max=100;
        scale_p_step=20;
      }
      GtkWidget * sizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
      GtkAdjustment *adjustment = gtk_adjustment_new(context->width, scale_min, scale_max, scale_step, scale_p_step, 0);
      // Set the GtkAdjustment for the scale, which will set the initial value
      gtk_range_set_adjustment(GTK_RANGE(sizeSpin), adjustment);
      CustomData *custom_data = g_new(CustomData, 1);
      custom_data->widget_id = GROMIT_WIDTH;
      custom_data->radio_nb = index;
      // Store the custom data in the GtkBox using a unique key
      g_object_set_data(G_OBJECT(sizeSpin), "custom-data", custom_data);
      gtk_widget_set_name(sizeSpin,"vbox-item");
      gtk_widget_set_tooltip_text(sizeSpin,"overall\nsize");
      gtk_widget_set_has_tooltip(sizeSpin,TRUE);
      g_signal_connect(vbox, "size-allocate", G_CALLBACK(limit_size_vbox), NULL);
      g_signal_connect(sizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
      //g_signal_connect(sizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
      gtk_box_pack_start(vbox,sizeSpin,FALSE,FALSE,0);
      }
       else
       {
          int idx = g_list_position(items, found_node);
          add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
       }
    }
    //if this is TRUE we need an overflow button

    if (attr == GROMIT_ARROW_TYPE || attr == GROMIT_ARROWSIZE || attr == GROMIT_SIMPLIFY ||
       attr == GROMIT_RADIUS || attr == GROMIT_MINLEN || attr == GROMIT_SNAPDIST)
       {
            if (!additional_items)
            {
                additional_items = TRUE;
              // Create an image from a file
                GtkWidget *image = gtk_image_new_from_icon_name("arrow-right", GTK_ICON_SIZE_BUTTON);
              // Create a button and add the image to it

              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = 99;
              custom_data->radio_nb = index;
              g_object_set_data(G_OBJECT(overflow_button), "custom-data", custom_data);

              gtk_button_set_image(GTK_BUTTON(overflow_button), image);


              g_signal_connect(overflow_button, "clicked", G_CALLBACK(show_hide_additional), NULL);

              gtk_box_pack_start(vbox,overflow_button,FALSE,FALSE,0);

            }

            if (attr == GROMIT_ARROW_TYPE)
            {
              g_print("arrow type");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_ARROW_TYPE));
              if (TRUE) {
              GtkWidget * arrowTypeCombo = create_arrow_combo(index,data);
              gtk_combo_box_set_active(arrowTypeCombo,context->arrow_type);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_ARROW_TYPE;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(arrowTypeCombo), "custom-data", custom_data);
              gtk_widget_set_name(arrowTypeCombo,"vbox-item");
              gtk_box_pack_start(vbox,arrowTypeCombo,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }

            }
            else if (attr == GROMIT_ARROWSIZE)
            {
              g_print("arrow_size");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_ARROWSIZE));
              if (TRUE) {
              GtkWidget * arrowSizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
              GtkAdjustment *adjustment = gtk_adjustment_new(context->arrowsize, scale_min, scale_max, scale_step, scale_p_step, 0);

              // Set the GtkAdjustment for the scale, which will set the initial value
              gtk_range_set_adjustment(GTK_RANGE(arrowSizeSpin), adjustment);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_ARROWSIZE;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(arrowSizeSpin), "custom-data", custom_data);
              gtk_widget_set_name(arrowSizeSpin,"vbox-item");
              gtk_widget_set_tooltip_text(arrowSizeSpin,"arrow\nsize");
              //g_signal_connect(arrowSizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
              g_signal_connect(arrowSizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
              gtk_box_pack_start(vbox,arrowSizeSpin,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }
            }
            else if (attr == GROMIT_SIMPLIFY)
            {
              g_print("simplify");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_SIMPLIFY));
              if (TRUE) {
              GtkWidget * simplifySizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
              GtkAdjustment *adjustment = gtk_adjustment_new(context->simplify, scale_min, scale_max, scale_step, scale_p_step, 0);

              // Set the GtkAdjustment for the scale, which will set the initial value
              gtk_range_set_adjustment(GTK_RANGE(simplifySizeSpin), adjustment);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_SIMPLIFY;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(simplifySizeSpin), "custom-data", custom_data);
              gtk_widget_set_name(simplifySizeSpin,"vbox-item");
              gtk_widget_set_tooltip_text(simplifySizeSpin,"simplify\namount");
              //g_signal_connect(simplifySizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
              g_signal_connect(simplifySizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
              gtk_box_pack_start(vbox,simplifySizeSpin,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }
            }
            else if (attr == GROMIT_RADIUS)
            {
              g_print("radius");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_RADIUS));
              if (TRUE) {
              GtkWidget * radiusSizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
              GtkAdjustment *adjustment = gtk_adjustment_new(context->radius, scale_min, scale_max, scale_step, scale_p_step, 0);

              // Set the GtkAdjustment for the scale, which will set the initial value
              gtk_range_set_adjustment(GTK_RANGE(radiusSizeSpin), adjustment);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_RADIUS;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(radiusSizeSpin), "custom-data", custom_data);
              gtk_widget_set_name(radiusSizeSpin,"vbox-item");
              gtk_widget_set_tooltip_text(radiusSizeSpin,"radius\namount");
              //g_signal_connect(radiusSizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
              g_signal_connect(radiusSizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
              gtk_box_pack_start(vbox,radiusSizeSpin,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }
            }
            else if (attr == GROMIT_MINLEN)
            {
              g_print("minlen");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_MINLEN));
              if (TRUE) {
              GtkWidget * minLenSizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
              GtkAdjustment *adjustment = gtk_adjustment_new(context->minlen, scale_min, scale_max, scale_step, scale_p_step, 0);

              // Set the GtkAdjustment for the scale, which will set the initial value
              gtk_range_set_adjustment(GTK_RANGE(minLenSizeSpin), adjustment);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_MINLEN;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(minLenSizeSpin), "custom-data", custom_data);
              gtk_widget_set_name(minLenSizeSpin,"vbox-item");
              gtk_widget_set_tooltip_text(minLenSizeSpin,"minimum\nlength");
              //g_signal_connect(minLenSizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
              g_signal_connect(minLenSizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
              gtk_box_pack_start(vbox,minLenSizeSpin,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }
            }
            else if (attr == GROMIT_SNAPDIST)
            {
              g_print("snap");
              GList *found_node = g_list_find(items, GINT_TO_POINTER(GROMIT_SNAPDIST));
              if (TRUE) {
              GtkWidget * snapSizeSpin = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,scale_min,scale_max,1);
              GtkAdjustment *adjustment = gtk_adjustment_new(context->snapdist, scale_min, scale_max, scale_step, scale_p_step, 0);

              // Set the GtkAdjustment for the scale, which will set the initial value
              gtk_range_set_adjustment(GTK_RANGE(snapSizeSpin), adjustment);
              CustomData *custom_data = g_new(CustomData, 1);
              custom_data->widget_id = GROMIT_SNAPDIST;
              custom_data->radio_nb = index;
              // Store the custom data in the GtkBox using a unique key
              g_object_set_data(G_OBJECT(snapSizeSpin), "custom-data", custom_data);
              gtk_widget_set_name(snapSizeSpin,"vbox-item");
              gtk_widget_set_tooltip_text(snapSizeSpin,"snap\namount");
              //g_signal_connect(snapSizeSpin, "query-tooltip", G_CALLBACK(on_query_tooltip), NULL);
              g_signal_connect(snapSizeSpin, "value-changed", G_CALLBACK(on_vbox_changed), data);
              gtk_box_pack_start(vbox,snapSizeSpin,FALSE,FALSE,0);
              }
              else
              {
                  int idx = g_list_position(items, found_node);
                  add_widget_to_other_box_by_idx(vboxOld,vbox,idx,&items);
              }
            }
            show_hide_additional(overflow_button,NULL);
        }
  }
  check_saved(vbox);
  return vbox;
}
GtkBox * set_appropriate_tool_options(GtkBox * vbox,gint index,GromitData * data)
{
  g_print("set_appropriate_tool_options");

  GList *pen_list = NULL;
  pen_list = g_list_append(pen_list, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  pen_list = g_list_append(pen_list, GINT_TO_POINTER(GROMIT_WIDTH));
  pen_list = g_list_append(pen_list, GINT_TO_POINTER(GROMIT_ARROW_TYPE));
  pen_list = g_list_append(pen_list, GINT_TO_POINTER(GROMIT_ARROWSIZE));

  GList *line_list = NULL;
  line_list = g_list_append(line_list, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  line_list = g_list_append(line_list, GINT_TO_POINTER(GROMIT_WIDTH));
  line_list = g_list_append(line_list, GINT_TO_POINTER(GROMIT_ARROW_TYPE));
  line_list = g_list_append(line_list, GINT_TO_POINTER(GROMIT_ARROWSIZE));

  GList *rect_list = NULL;
  rect_list = g_list_append(rect_list, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  rect_list = g_list_append(rect_list, GINT_TO_POINTER(GROMIT_WIDTH));

  GList *smooth_list = NULL;
  smooth_list = g_list_append(smooth_list, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  smooth_list = g_list_append(smooth_list, GINT_TO_POINTER(GROMIT_WIDTH));
  smooth_list = g_list_append(smooth_list, GINT_TO_POINTER(GROMIT_SIMPLIFY));
  smooth_list = g_list_append(smooth_list, GINT_TO_POINTER(GROMIT_SNAPDIST));

  GList *ortho_list = NULL;
  ortho_list = g_list_append(ortho_list,GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  ortho_list = g_list_append(ortho_list,GINT_TO_POINTER(GROMIT_WIDTH));
  ortho_list = g_list_append(ortho_list, GINT_TO_POINTER(GROMIT_SIMPLIFY));
  ortho_list = g_list_append(ortho_list, GINT_TO_POINTER(GROMIT_RADIUS));
  ortho_list = g_list_append(ortho_list, GINT_TO_POINTER(GROMIT_MINLEN));
  ortho_list = g_list_append(ortho_list, GINT_TO_POINTER(GROMIT_SNAPDIST));

  GList *retool_list = NULL;
  retool_list = g_list_append(retool_list, GINT_TO_POINTER(GROMIT_PAINT_COLOR));
  retool_list = g_list_append(retool_list, GINT_TO_POINTER(GROMIT_WIDTH));

  GList *eraser_list = NULL;
  eraser_list = g_list_append(eraser_list, GINT_TO_POINTER(GROMIT_WIDTH));





  GList *iter = gtk_container_get_children(GTK_CONTAINER(vbox)); // Get the list of child widgets
  GtkWidget *child = GTK_WIDGET(iter->data);  // Get the current child widget
  //the first item should be a tool combo box
  if (GTK_IS_COMBO_BOX(child))
  {
    g_print("960************************");
    gint combo_index = gtk_combo_box_get_active((GtkComboBox*)child);
    switch (combo_index)
    {
      case GROMIT_PEN:   vbox = create_vbox(vbox,pen_list,combo_index,index,data);
                break;
      case GROMIT_LINE:   vbox = create_vbox(vbox,line_list,combo_index,index,data);
                break;
      case GROMIT_RECT:   vbox = create_vbox(vbox,rect_list,combo_index,index,data);
                break;
      case GROMIT_SMOOTH:   vbox = create_vbox(vbox,smooth_list,combo_index,index,data);
                break;
      case GROMIT_ORTHOGONAL:   vbox = create_vbox(vbox,ortho_list,combo_index,index,data);
                break;
      case GROMIT_RECOLOR:   vbox = create_vbox(vbox,retool_list,combo_index,index,data);
                break;
      case GROMIT_ERASER:   vbox = create_vbox(vbox,eraser_list,combo_index,index,data);
                break;
    }

  }
  else
  {
    g_print("983 not combo box*****************************");
    return NULL;
  }



  g_list_free(iter);  // Free the list when done
  return vbox;
}
GtkWidget * create_arrow_combo(gint index,GromitData * data)
{
  g_print("create_arrow_combo");
  // Create a GtkListStore with two columns: one for GdkPixbuf and one for text
    GtkListStore *list_store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

    // Get the current icon theme


    // Add items to the list store
    GtkTreeIter iter;
    GdkPixbuf *none = gdk_pixbuf_new_from_file_at_size("/usr/local/share/icons/hicolor/24x24/actions/none_arrow.png", 24, 24, NULL);
    GdkPixbuf *start = gdk_pixbuf_new_from_file_at_size("/usr/local/share/icons/hicolor/24x24/actions/start_arrow.png", 24, 24, NULL);
    GdkPixbuf *end = gdk_pixbuf_new_from_file_at_size("/usr/local/share/icons/hicolor/24x24/actions/end_arrow.png", 24, 24, NULL);
    GdkPixbuf *start_end = gdk_pixbuf_new_from_file_at_size("/usr/local/share/icons/hicolor/24x24/actions/start_end_arrow.png", 24, 24, NULL);


    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, none, 1, "none", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, start, 1, "start", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, end, 1, "end", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, start_end, 1, "start & end", -1);


    // Create a GtkComboBox and set its model
    GtkWidget *arrow_combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_widget_set_name(arrow_combo_box,"vbox-item");

    // Add a cell renderer for the image
    GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(arrow_combo_box), pixbuf_renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(arrow_combo_box), pixbuf_renderer, "pixbuf", 0, NULL);

    // Add a cell renderer for the text
    /*GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(arrow_combo_box), text_renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(arrow_combo_box), text_renderer, "text", 1, NULL);
    */


    gtk_combo_box_set_active((GtkComboBox *) arrow_combo_box,data->current_graph_menu_type[index]);
    g_signal_connect(arrow_combo_box,"changed",G_CALLBACK(on_vbox_changed),data);
    g_signal_connect(arrow_combo_box, "query-tooltip", G_CALLBACK(on_combo_query_tooltip), NULL);
    gtk_widget_set_has_tooltip(arrow_combo_box, TRUE);


    CustomData *custom_data = g_new(CustomData, 1);
    custom_data->widget_id = GROMIT_ARROW_TYPE;
    custom_data->radio_nb = index;
    // Store the custom data in the GtkBox using a unique key
    g_object_set_data(G_OBJECT(arrow_combo_box), "custom-data", custom_data);
  // Cleanup
    g_object_unref(none);
    g_object_unref(start);
    g_object_unref(end);
    g_object_unref(start_end);
    g_object_unref(list_store);

    return arrow_combo_box;
}
void on_hide(GtkWidget *widget, gpointer user_data)
{
  GromitData * data = (GromitData *) user_data;
  toggle_visibility(data);
}
void on_gui_clear(GtkWidget *widget, gpointer user_data)
{
  GromitData * data = (GromitData *) user_data;
  clear_screen(data);
}
void on_toggle_paint_gui(GtkWidget *widget,
			 GdkEventButton  *ev,
			 gpointer   user_data)
{
  GromitData * data = (GromitData *) user_data;
  data->started_from_gui = TRUE;
  gtk_window_get_position(data->gui_window,&data->gui_window_position.x,&data->gui_window_position.y);
  gtk_widget_hide(data->gui_window);
  toggle_grab(data, ev->device);
}

void on_undo_button(GtkWidget *widget,gpointer user_data)
{
  GromitData * data = (GromitData *) user_data;
  undo_drawing(data);
}
void on_redo_button(GtkWidget *widget,gpointer user_data)
{
  GromitData * data = (GromitData *) user_data;
  redo_drawing(data);
}
void on_opacity_changed(GtkWidget *widget, gpointer user_data)
{
  GromitData *data = (GromitData *) user_data;
  data->opacity = gtk_range_get_value(GTK_RANGE(widget))/100;
  gtk_widget_set_opacity(data->win, data->opacity);
}

void on_tool_changed(GtkComboBox *tool_combo, gpointer user_data)
{
  g_print("on_tool_changed");
  GromitData * data = (GromitData *) user_data;
  g_print("1042 *****************");
  GtkBox *vbox = (GtkBox*)gtk_widget_get_parent(tool_combo);
  GtkBox *hbox = (GtkBox*)gtk_widget_get_parent(vbox);
  g_object_ref(vbox);
  gtk_container_remove(hbox,vbox);
  CustomData *custom_data = (CustomData *)g_object_get_data(G_OBJECT(tool_combo), "custom-data");
  data->current_graph_menu_type[custom_data->radio_nb]=gtk_combo_box_get_active((GtkComboBox*)tool_combo);
  vbox = set_appropriate_tool_options(vbox,custom_data->radio_nb,data);
  gtk_box_pack_start(hbox,vbox,FALSE,FALSE,0);
  gtk_widget_show_all(vbox);
  on_vbox_changed(tool_combo,data);


}

gboolean on_combo_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data) {
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *tooltip_text;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)) {
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
        gtk_tree_model_get(model, &iter, 1, &tooltip_text, -1); // Column 1 holds the tooltip
        gtk_tooltip_set_text(tooltip, tooltip_text);
        g_free(tooltip_text);
        return TRUE;
    }

    return FALSE;
}
GtkWidget * create_menu_tool_combo(gint index,GromitData * data)
{
  g_print("create_menu_tool_combo");
  // Create a GtkListStore with two columns: one for GdkPixbuf and one for text
    GtkListStore *list_store = gtk_list_store_new(2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

    // Get the current icon theme
    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();

    // Add items to the list store
    GtkTreeIter iter;

    GdkPixbuf *pen_icon = gtk_icon_theme_load_icon(icon_theme, "tool_pen", 24, 0, NULL);
    GdkPixbuf *line_icon = gtk_icon_theme_load_icon(icon_theme, "tool_line", 24, 0, NULL);
    GdkPixbuf *rect_icon = gtk_icon_theme_load_icon(icon_theme, "tool_rectangle", 24, 0, NULL);
    GdkPixbuf *smooth_icon = gtk_icon_theme_load_icon(icon_theme, "smooth", 24, 0, NULL);
    GdkPixbuf *ortho_icon = gtk_icon_theme_load_icon(icon_theme, "snap-orthogonal", 24, 0, NULL);
    GdkPixbuf *retool_icon = gtk_icon_theme_load_icon(icon_theme, "edit-select-symbolic", 24, 0, NULL);
    GdkPixbuf *eraser_icon = gtk_icon_theme_load_icon(icon_theme, "tool_eraser", 24, 0, NULL);

    gtk_list_store_append(list_store, &iter);

    gtk_list_store_set(list_store, &iter, 0, pen_icon, 1, "Pen", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, line_icon, 1, "Line", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, rect_icon, 1, "Rectangle", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, smooth_icon, 1, "Smooth", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, ortho_icon, 1, "Orthogonal", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, retool_icon, 1, "Retool", -1);
    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, 0, eraser_icon, 1, "Eraser", -1);
    //tooltip stuff
    GtkTreeView *tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store)));
    gtk_tree_view_set_tooltip_column(tree_view, 1);

    // Create a GtkComboBox and set its model
    GtkWidget *tool_combo_box = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list_store));
    gtk_widget_set_name(tool_combo_box,"vbox-item");

    // Add a cell renderer for the image
    GtkCellRenderer *pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tool_combo_box), pixbuf_renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tool_combo_box), pixbuf_renderer, "pixbuf", 0, NULL);

    // Add a cell renderer for the text
    /*GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tool_combo_box), text_renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tool_combo_box), text_renderer, "text", 1, NULL);
    */
    gtk_combo_box_set_active((GtkComboBox*)tool_combo_box,data->current_graph_menu_type[index]);

    gtk_widget_set_name(tool_combo_box,"vbox-item");
    //tooltip stuff
    g_signal_connect(tool_combo_box, "query-tooltip", G_CALLBACK(on_combo_query_tooltip), NULL);
    gtk_widget_set_has_tooltip(tool_combo_box, TRUE);


    CustomData *custom_data = g_new(CustomData, 1);
    custom_data->widget_id = 100;
    custom_data->radio_nb = index;
    // Store the custom data in the GtkBox using a unique key
    g_object_set_data(G_OBJECT(tool_combo_box), "custom-data", custom_data);

    g_signal_connect(tool_combo_box,"changed",G_CALLBACK(on_tool_changed),data);
  // Cleanup
    g_object_unref(pen_icon);
    g_object_unref(line_icon);
    g_object_unref(rect_icon);
    g_object_unref(smooth_icon);
    g_object_unref(ortho_icon);
    g_object_unref(retool_icon);
    g_object_unref(eraser_icon);
    g_object_unref(list_store);

    return tool_combo_box;
}
void radio_toggled(GtkWidget *widget,gpointer user_data)
{
  g_print("radio_toggled");
  GromitData * data = (GromitData *) user_data;
  CustomData * custom_data = g_object_get_data(widget,"custom-data");
  data->current_graph_menu_tool = custom_data->radio_nb;
  //save_values(data);
  //GromitPaintContext *tool = data->graph_menu_tools[custom_data->radio_nb][data->current_graph_menu_type[custom_data->radio_nb]];


  //data->current_graph_menu_tool = tool;
}
// Function to create a radio button
GtkWidget* create_radio_button(GtkRadioButton **group, const gchar *label,gint index,GromitData * data) {
    g_print("create_radio_button");
    GtkWidget *radio_button = gtk_radio_button_new_with_label_from_widget(*group, label);
    g_signal_connect(radio_button,"toggled",G_CALLBACK(radio_toggled),data);
    CustomData *custom_data = g_new(CustomData,1);
    custom_data->radio_nb = index;
    custom_data->widget_id = 1000;
    g_object_set_data(G_OBJECT(radio_button),"custom-data",custom_data);
    if (*group == NULL) {
      g_print("was null");
        *group = radio_button;
    }
    return radio_button;
}

// Function to create the GTK layout with radio buttons and boxes
GtkWidget* create_radio_box_pair(GtkRadioButton **group, const gchar *radio_label,gint index,GromitData *data) {
    g_print("create_radio_box_pair");
    // Create a horizontal box to hold the radio button and GtkBox
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

    // Create the radio button (attached to the group)
    GtkWidget *radio_button = create_radio_button(group, radio_label,index,data);
    if (data->current_graph_menu_tool == index)
      gtk_toggle_button_set_active(radio_button,TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), radio_button, FALSE, FALSE, 0);

    // Create a GtkBox on the right to place other widgets (e.g., label or buttons)
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *tool_combo = create_menu_tool_combo(index,data);

    gtk_box_pack_start(GTK_BOX(vbox),tool_combo,FALSE,FALSE,0);
    g_print("1136**************");
    vbox = set_appropriate_tool_options(vbox,index,data);
    g_print("1138****************");
    // Pack the GtkBox on the right side of the hbox
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
    gtk_widget_set_name(hbox,"hbox");
    return hbox;
}
void read_tool_from_key_file(GKeyFile *key_file,gchar* tool_str,
                          GromitPaintContext *tool_type,GromitData *data){
      g_print("read_tool_from_key_file...%s",tool_str);
      GError *error = NULL;
      tool_type->type = g_key_file_get_integer(key_file,tool_str,"type",&error);

      tool_type->width = g_key_file_get_integer(key_file,tool_str,"width",&error);
      tool_type->arrowsize = g_key_file_get_double(key_file,tool_str,"arrow_size",&error);
      tool_type->arrow_type = g_key_file_get_integer(key_file,tool_str,"arrow_type",&error);
      tool_type->minwidth = g_key_file_get_integer(key_file,tool_str,"minwidth",&error);
      tool_type->maxwidth = g_key_file_get_integer(key_file,tool_str,"maxwidth",&error);
      tool_type->radius = g_key_file_get_integer(key_file,tool_str,"radius",&error);
      tool_type->minlen = g_key_file_get_integer(key_file,tool_str,"minlen",&error);
      tool_type->maxangle = g_key_file_get_integer(key_file,tool_str,"maxangle",&error);
      tool_type->simplify = g_key_file_get_integer(key_file,tool_str,"simplify",&error);
      tool_type->snapdist = g_key_file_get_integer(key_file,tool_str,"snapdist",&error);
      g_print("here10");
      guint length;
      g_print("here20");
      gdouble *colors = g_key_file_get_double_list(key_file,tool_str,"paint_color",&length,&error);
      g_print("here30");
      GdkRGBA *color = g_malloc(sizeof(GdkRGBA));
      color->red = colors[0];
      g_print("here40");
      color->green = colors[1];
      color->blue = colors[2];
      color->alpha = colors[3];
      tool_type->paint_color = color;
      g_print("here50");
      tool_type->pressure = g_key_file_get_double(key_file,tool_str,"pressure",&error);
      make_paint_ctx(tool_type,data);


}
void make_paint_ctx(GromitPaintContext *tool_type,GromitData *data)
{
  tool_type->paint_ctx = cairo_create (data->backbuffer);

  gdk_cairo_set_source_rgba(tool_type->paint_ctx, tool_type->paint_color);
  if(!data->composited)
    cairo_set_antialias(tool_type->paint_ctx, CAIRO_ANTIALIAS_NONE);
  cairo_set_line_width(tool_type->paint_ctx, tool_type->width);
  cairo_set_line_cap(tool_type->paint_ctx, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(tool_type->paint_ctx, CAIRO_LINE_JOIN_ROUND);

  if (tool_type->type == GROMIT_ERASER)
    cairo_set_operator(tool_type->paint_ctx, CAIRO_OPERATOR_CLEAR);
  else
    if (tool_type->type == GROMIT_RECOLOR)
      cairo_set_operator(tool_type->paint_ctx, CAIRO_OPERATOR_ATOP);
    else /* GROMIT_PEN */
      cairo_set_operator(tool_type->paint_ctx, CAIRO_OPERATOR_OVER);
}

void load_tool_default(GromitPaintContext ** tool_type,GromitPaintType type,int tool_nb,GromitData * data)
{

    g_print("load_tool_default");
    GList * color_list = NULL;
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){255/255, 0.0, 0.0, 1.0}, sizeof(GdkRGBA))); // Red
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){0.0, 255/255, 0.0, 1.0}, sizeof(GdkRGBA))); // Green
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){0.0, 0.0, 255/255, 1.0}, sizeof(GdkRGBA))); // Blue
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){255/255, 255/255, 0.0, 1.0}, sizeof(GdkRGBA))); // Yellow
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){0.0, 255/255, 255/255, 1.0}, sizeof(GdkRGBA))); // Cyan
    color_list = g_list_append(color_list, g_memdup(&(GdkRGBA){255/255,0,255/255,1.0},sizeof(GdkRGBA))); //magenta

    *tool_type = g_malloc0(sizeof(GromitPaintContext));
    (*tool_type)->type = type;
    (*tool_type)->width = 7;
    (*tool_type)->arrowsize = 0;
    (*tool_type)->arrow_type = GROMIT_ARROW_NONE;
    (*tool_type)->minwidth = 1;
    (*tool_type)->maxwidth = G_MAXUINT;
    (*tool_type)->radius = 10;
    (*tool_type)->minlen = 2 * 10 + 10 /2;
    (*tool_type)->maxangle = 15;
    (*tool_type)->simplify = 10;
    (*tool_type)->snapdist = 0;
    (*tool_type)->paint_color = g_list_nth_data(color_list,tool_nb%g_list_length(color_list));
    make_paint_ctx(*tool_type,data);
}
void load_tool_defaults(GromitData * data)
{
      g_print("load_tool_defaults");
      data->current_graph_menu_tool = 0;
      for(int tool_nb=0;tool_nb<GROMIT_NUMBER_OF_GUI_TOOLS;tool_nb++)
      {
        for(int type=GROMIT_PEN;type<GROMIT_NUMBER_OF_PAINT_TYPES;type++)
        {
          g_print("tool_nb: %d  type: %d",tool_nb+1,type);
          load_tool_default(&(data->graph_menu_tools[tool_nb][type]),type,tool_nb,data);
        }
        data->current_graph_menu_type[tool_nb]=GROMIT_PEN;
      }
}
void setup_tools(GromitData * data)
{
  g_print("setup_tools");
  GKeyFile *key_file = g_key_file_new();
  GError *error = NULL;
  gchar * tool_str;
  load_tool_defaults(data);
  gchar *filename = g_strdup_printf("%s/.gromit_config",g_get_home_dir());
  if (!g_key_file_load_from_file(key_file, filename, G_KEY_FILE_NONE, &error)) {
        g_printerr("Error loading config file: %s\n", error->message);
        g_clear_error(&error);

    }
  else {
    for(int tool_nb=0;tool_nb<GROMIT_NUMBER_OF_GUI_TOOLS;tool_nb++)
    {
        for(int type=GROMIT_PEN;type<GROMIT_NUMBER_OF_PAINT_TYPES;type++)
        {
          tool_str = g_strdup_printf("Tool%d_%s",tool_nb,data->paint_types_str[type]); // keys have format "Tool<nb>__<type>"
          read_tool_from_key_file(key_file,tool_str,(data->graph_menu_tools[tool_nb][type]),data);
        }
    }
    //now setup based on General settings
    data->current_graph_menu_tool = g_key_file_get_integer(key_file,"General","current_graph_menu_tool",&error);
    guint length;
    gint *menu_types = g_key_file_get_integer_list(key_file,"General","current_graph_menu_type",&length,&error);
    for (int i=0;i<length;i++)
      data->current_graph_menu_type[i]=menu_types[i];
  if (!error)
    g_print("Successfully read in file");
  }
}

gboolean on_menu_toggle(GtkMenuItem *menuitem, gpointer user_data)
{
  g_print("on_menu_toggle");
  GromitData * data = (GromitData *) user_data;
  if(!data->gui_menu_toggle)
  {
    data->gui_menu_toggle = TRUE;
  }
 
  setup_tools(data);
  data->use_graphical_menu_items = TRUE;
  //g_print("world");
  int MENU_WIDTH=25;
  int MENU_HEIGHT=600;
  GtkWindow * menuWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_keep_above(GTK_WINDOW(menuWindow), TRUE);
  //not working  g_signal_connect(menuWindow, "destroy", G_CALLBACK(on_window_close), data);
  data->gui_window = menuWindow;
  gtk_window_set_default_size (GTK_WINDOW (menuWindow),MENU_WIDTH,MENU_HEIGHT);
  gtk_window_set_deletable(GTK_WINDOW(menuWindow), TRUE); // Close button enabled
  gtk_window_set_resizable(GTK_WINDOW(menuWindow), FALSE); // Disable resizing
  gtk_window_set_decorated(GTK_WINDOW(menuWindow),FALSE);
  GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(menuWindow));
  int screen_width = gdk_screen_get_width(screen);
  int screen_height = gdk_screen_get_height(screen);

  // Get the size of the window, didn't work
  //int window_width, window_height;
  //gtk_window_get_size(GTK_WINDOW(menuWindow), &window_width, &window_height);

  // Calculate the position for the window (on the far right)
  int x_position = screen_width - MENU_WIDTH - 30; // Position it at the far right
  int y_position = (screen_height-MENU_HEIGHT) / 2; // Vertically center the window

  // Move the window to the calculated position
  gtk_window_move(GTK_WINDOW(menuWindow), x_position, y_position);
  GtkBox * mainVBox = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);





   GtkCssProvider * css_provider = gtk_css_provider_new();
   const gchar *css_data =
        "/* Apply a border to the window */\n"
        "window {\n"
        "    border: 1px solid black;\n"
        "}\n"
        "\n"
        "/* Apply padding to the GtkBox */\n"
        "#hbox {\n"
        "    padding: 10px 10px 10px 10px;\n" //top right bottom left

        "    border: 1px solid black;\n"
        "}\n"
        "#vbox {\n"

        "}\n"
        "#vbox-item {\n"


        "}\n"
        ".tooltip {"
        "   background-color: #333333;"  /* Dark background */
        "   color: #ffffff;"              /* White text */
        "   border-radius: 5px;"          /* Rounded corners */
        "   padding: 5px;"                /* Padding inside the tooltip */
        "   font-size: 12px;"             /* Optional: Adjust font size */
        "}";

    // Load the CSS into the provider
    gtk_css_provider_load_from_data(css_provider, css_data, -1, NULL);

    // Get the style context for the window and box and apply the CSS
    GtkStyleContext *window_style_context = gtk_widget_get_style_context(menuWindow);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );


  GtkRadioButton *group = NULL;
  for (int i=0;i<GROMIT_NUMBER_OF_GUI_TOOLS;i++)
  {
    GtkWidget *radio_box = create_radio_box_pair(&group,g_strdup_printf("%d",i+1),i,data);

    gtk_box_pack_start(GTK_BOX(mainVBox),radio_box,FALSE,FALSE,0);
  }
  GtkWidget * scaleOpacity = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,1,100,1);
  g_signal_connect(scaleOpacity,"value-changed",G_CALLBACK(on_opacity_changed),data);
  GtkAdjustment *adjustment = gtk_adjustment_new(100, 1, 100, 1, 1, 0);
  gtk_range_set_adjustment(GTK_RANGE(scaleOpacity), adjustment);
  gtk_widget_set_tooltip_text(scaleOpacity,"opacity");
  gtk_widget_set_has_tooltip(scaleOpacity,TRUE);
  GtkWidget * hideButton = gtk_toggle_button_new();
  // Create an icon image using the icon theme
  GtkWidget *icon = gtk_image_new_from_icon_name("gnumeric-column-hide", GTK_ICON_SIZE_BUTTON);
  // Set the icon as the image for the toggle button
  gtk_button_set_image(GTK_BUTTON(hideButton), icon);
  // Connect the toggled signal to a callback function
  g_signal_connect(hideButton, "toggled", G_CALLBACK(on_hide), data);
  gtk_widget_set_tooltip_text(hideButton,"hide");
  gtk_widget_set_has_tooltip(hideButton,TRUE);
  GtkWidget *buttonClear = gtk_button_new_from_icon_name("delete",16);
  g_signal_connect(buttonClear,"clicked",G_CALLBACK(on_gui_clear),data);
  gtk_widget_set_tooltip_text(buttonClear,"clear all");
  gtk_widget_set_has_tooltip(buttonClear,TRUE);
  GtkWidget * drawButton = gtk_toggle_button_new();
  // Create an icon image using the icon theme
  icon = gtk_image_new_from_icon_name("preferences-desktop-display-color", GTK_ICON_SIZE_BUTTON);
  // Set the icon as the image for the toggle button
  gtk_button_set_image(GTK_BUTTON(drawButton), icon);
  g_signal_connect(drawButton, "button-press-event",G_CALLBACK(on_toggle_paint_gui), data);
  gtk_widget_set_tooltip_text(drawButton,"start\ndrawing");
  gtk_widget_set_has_tooltip(drawButton,TRUE);

  GtkBox *undo_redo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  GtkWidget *undo_button = gtk_button_new_from_icon_name("edit-undo",16);
  g_signal_connect(undo_button,"clicked",G_CALLBACK(on_undo_button),data);
  gtk_widget_set_tooltip_text(undo_button,"undo");
  gtk_widget_set_has_tooltip(undo_button,TRUE);
  GtkWidget *redo_button = gtk_button_new_from_icon_name("edit-redo",16);
  g_signal_connect(redo_button,"clicked",G_CALLBACK(on_redo_button),data);
  gtk_widget_set_tooltip_text(redo_button,"redo");
  gtk_widget_set_has_tooltip(redo_button,TRUE);
  gtk_box_pack_start(undo_redo,undo_button,TRUE,TRUE,0);
  gtk_box_pack_start(undo_redo,redo_button,TRUE,TRUE,0);

  GtkBox *move_close = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  GtkWidget * button_move = gtk_button_new_from_icon_name("transform-move",16);
  g_signal_connect(button_move,"button-press-event",G_CALLBACK(on_move_button_pressed),NULL);
  gtk_widget_set_tooltip_text(button_move,"move\nmenu");
  gtk_widget_set_has_tooltip(button_move,TRUE);
  GtkWidget * button_close = gtk_button_new_from_icon_name("gtk-close",16);
  g_signal_connect(button_close, "clicked", G_CALLBACK(on_window_close), data);
  gtk_widget_set_tooltip_text(button_close,"close\nmenu");
  gtk_widget_set_has_tooltip(button_close,TRUE);
  gtk_box_pack_start(move_close,button_move,TRUE,TRUE,0);
  gtk_box_pack_start(move_close,button_close,TRUE,TRUE,0);
  GtkBox * hideButtonDrawBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);

  gtk_box_pack_start(GTK_BOX(hideButtonDrawBox),hideButton,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(hideButtonDrawBox),buttonClear,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(hideButtonDrawBox),drawButton,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(mainVBox),scaleOpacity,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mainVBox),hideButtonDrawBox,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mainVBox),undo_redo,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mainVBox),move_close,FALSE,FALSE,0);
  gtk_container_add(GTK_CONTAINER(menuWindow),mainVBox);
  gtk_widget_show_all(menuWindow);


  return TRUE;
}

gboolean on_toggle_paint(GtkWidget *widget,
			 GdkEventButton  *ev,
			 gpointer   user_data)
{
    GromitData *data = (GromitData *) user_data;

    if(data->debug)
	g_printerr("DEBUG: Device '%s': Button %i on_toggle_paint at (x,y)=(%.2f : %.2f)\n",
		   gdk_device_get_name(ev->device), ev->button, ev->x, ev->y);

    toggle_grab(data, ev->device);

    return TRUE;
}

void on_toggle_paint_all (GtkMenuItem *menuitem,
			  gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;

  /*
    on_toggle_paint_all() is called when toggle_paint_item in the menu
    is clicked on KDE-like platforms. Under X11, at least
    https://github.com/ubuntu/gnome-shell-extension-appindicator seems to
    grab the pointer, preventing grabbing by Gromit-MPX.
    Simply work around this by waiting :-/
   */
  char *xdg_session_type = getenv("XDG_SESSION_TYPE");
  if (xdg_session_type && strcmp(xdg_session_type, "x11") == 0)
      g_usleep(333*1000);

  toggle_grab(data, NULL);
}


void on_clear (GtkMenuItem *menuitem,
	       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  clear_screen(data);
}


void on_toggle_vis(GtkMenuItem *menuitem,
		   gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  toggle_visibility(data);
}


void on_thicker_lines(GtkMenuItem *menuitem,
		      gpointer     user_data)
{
  line_thickener += 0.1;
}

void on_thinner_lines(GtkMenuItem *menuitem,
		      gpointer     user_data)
{
  line_thickener -= 0.1;
  if (line_thickener < -1)
    line_thickener = -1;
}


void on_opacity_bigger(GtkMenuItem *menuitem,
		       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  data->opacity += 0.1;
  if(data->opacity>1.0)
    data->opacity = 1.0;
  gtk_widget_set_opacity(data->win, data->opacity);
}

void on_opacity_lesser(GtkMenuItem *menuitem,
		       gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  data->opacity -= 0.1;
  if(data->opacity<0.0)
    data->opacity = 0.0;
  gtk_widget_set_opacity(data->win, data->opacity);
}


void on_undo(GtkMenuItem *menuitem,
	     gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  undo_drawing (data);
}

void on_redo(GtkMenuItem *menuitem,
	     gpointer     user_data)
{
  GromitData *data = (GromitData *) user_data;
  redo_drawing (data);
}


void on_about(GtkMenuItem *menuitem,
	      gpointer     user_data)
{
    const gchar *authors [] = { "Christian Beier <info@christianbeier.net>",
                                "Simon Budig <Simon.Budig@unix-ag.org>",
                                "Barak A. Pearlmutter <barak+git@pearlmutter.net>",
                                "Nathan Whitehead <nwhitehe@gmail.com>",
                                "Lukáš Hermann <tuxilero@gmail.com>",
                                "Katie Holly <git@meo.ws>",
                                "Monty Montgomery <xiphmont@gmail.com>",
                                "AlisterH <alister.hood@gmail.com>",
                                "Mehmet Atif Ergun <mehmetaergun@users.noreply.github.com>",
                                "Russel Winder <russel@winder.org.uk>",
                                "Tao Klerks <tao@klerks.biz>",
                                "Tobias Schönberg <tobias47n9e@gmail.com>",
                                "Yuri D'Elia <yuri.delia@eurac.edu>",
				"Julián Unrrein <junrrein@gmail.com>",
				"Eshant Gupta <guptaeshant@gmail.com>",
				"marput <frayedultrasonicaligator@disroot.org>",
				"albanobattistella <34811668+albanobattistella@users.noreply.github.com>",
				"Renato Candido <renatocan@gmail.com>",
				"Komeil Parseh <ahmdparsh129@gmail.com>",
				"Adam Chyła <adam@chyla.org>",
				"bbhtt <62639087+bbhtt@users.noreply.github.com>",
				"avma <avi.markovitz@gmail.com>",
				"godmar <godmar@gmail.com>",
				"Ashwin Rajesh <46510831+VanillaViking@users.noreply.github.com>",
                                "Pascal Niklaus <pascal.niklaus@ieu.uzh.ch>",
				NULL };
    gtk_show_about_dialog (NULL,
			   "program-name", "Gromit-MPX",
			   "logo-icon-name", "net.christianbeier.Gromit-MPX",
			   "title", _("About Gromit-MPX"),
			   "comments", _("Gromit-MPX (GRaphics Over MIscellaneous Things - Multi-Pointer-EXtension) is an on-screen annotation tool that works with any Unix desktop environment under X11 as well as Wayland."),
			   "version", PACKAGE_VERSION,
			   "website", PACKAGE_URL,
			   "authors", authors,
			   "copyright", "2009-2024 Christian Beier, Copyright 2000 Simon Budig",
			   "license-type", GTK_LICENSE_GPL_2_0,
			   NULL);
}


static void on_intro_show_again_button_toggled(GtkCheckButton *toggle, GromitData *data)
{
  data->show_intro_on_startup = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle));
}

void on_intro(GtkMenuItem *menuitem,
	      gpointer user_data)
{
    GromitData *data = (GromitData *) user_data;

    // Create a new assistant widget with no pages.
    GtkWidget *assistant = gtk_assistant_new ();
    gtk_window_set_position (GTK_WINDOW(assistant), GTK_WIN_POS_CENTER);

    // set page one
    GtkWidget *widgetOne = gtk_label_new(_("Gromit-MPX (GRaphics Over MIscellaneous Things) is a small tool to make\n"
					  "annotations on the screen.\n\n"
					  "Its main use is for making presentations of some application. Normally,\n"
					  "you would have to move the mouse pointer around the point of interest\n"
					  "until hopefully everybody noticed it.  With Gromit-MPX, you can draw\n"
					  "everywhere onto the screen, highlighting some button or area.\n\n"
                                          "If you happen to enjoy using Gromit-MPX, please consider supporting\n"
					  "its development by using one of the donation options on the project's\n"
					  "website or directly via the support options available from the tray menu.\n"));
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetOne);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetOne, _("Gromit-MPX - What is it?"));
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetOne, GTK_ASSISTANT_PAGE_INTRO);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetOne, TRUE);

    // set page two
    GtkWidget *widgetTwo = gtk_label_new (NULL);
    char widgetTwoBuf[4096];
    snprintf(widgetTwoBuf, sizeof(widgetTwoBuf),
	     _("You can operate Gromit-MPX using its tray icon (if your desktop environment\n"
	     "provides a sys tray), but since you typically want to use the program you are\n"
	     "demonstrating and highlighting something is a short interruption of your\n"
	     "workflow, Gromit-MPX can be toggled on and off on the fly via a hotkey:\n\n"
	     "It grabs the `%s` and `%s` keys, so that no other application can use them\n"
	     "and they are available to Gromit-MPX only.  The available commands are:\n\n<tt><b>"
	     "   toggle painting:         %s\n"
       "   toggle gui menu          ALT-%s\n"
	     "   clear screen:            SHIFT-%s\n"
	     "   toggle visibility:       CTRL-%s\n"
	     "   quit:                    ALT-%s\n"
	     "   undo last stroke:        %s\n"
	     "   redo last undone stroke: SHIFT-%s</b></tt>"),
	     data->hot_keyval, data->undo_keyval,
	     data->hot_keyval,data->menu_keyval, data->hot_keyval, data->hot_keyval, data->hot_keyval, data->undo_keyval, data->undo_keyval);
    gtk_label_set_markup (GTK_LABEL (widgetTwo), widgetTwoBuf);
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetTwo);
    gtk_assistant_set_page_title (GTK_ASSISTANT (assistant), widgetTwo, _("Gromit-MPX - How to use it"));
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetTwo, GTK_ASSISTANT_PAGE_CONTENT);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetTwo, TRUE);

    // set page three
    GtkWidget *widgetThree = gtk_grid_new ();
    GtkWidget *widgetThreeText = gtk_label_new (_("Do you want to show this introduction again on the next start of Gromit-MPX?\n"
						  "You can always access it again via the sys tray menu.\n"));
    GtkWidget *widgetThreeButton = gtk_check_button_new_with_label (_("Show again on startup"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgetThreeButton), data->show_intro_on_startup);
    gtk_grid_attach (GTK_GRID (widgetThree), widgetThreeText, 0, 0, 1, 1);
    gtk_grid_attach_next_to (GTK_GRID (widgetThree), widgetThreeButton, widgetThreeText, GTK_POS_BOTTOM, 1, 1);
    g_signal_connect (G_OBJECT (widgetThreeButton), "toggled",
		      G_CALLBACK (on_intro_show_again_button_toggled), data);
    gtk_assistant_append_page (GTK_ASSISTANT (assistant), widgetThree);
    gtk_assistant_set_page_type (GTK_ASSISTANT (assistant), widgetThree, GTK_ASSISTANT_PAGE_CONFIRM);
    gtk_assistant_set_page_complete (GTK_ASSISTANT (assistant), widgetThree, TRUE);

    // connect the close buttons
    g_signal_connect (G_OBJECT (assistant), "cancel",
		      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (G_OBJECT (assistant), "close",
		      G_CALLBACK (gtk_widget_destroy), NULL);

    // show
    gtk_widget_show_all (assistant);
}

void on_edit_config(GtkMenuItem *menuitem,
                    gpointer user_data)
{
    /*
      Check if user config does not exist or is empty.
      If so, copy system config to user config.
    */
    gchar *user_config_path = g_strjoin (G_DIR_SEPARATOR_S, g_get_user_config_dir(), "gromit-mpx.cfg", NULL);
    GFile *user_config_file = g_file_new_for_path(user_config_path);

    guint64 user_config_size = 0;
    GFileInfo *user_config_info = g_file_query_info(user_config_file, G_FILE_ATTRIBUTE_STANDARD_SIZE, 0, NULL, NULL);
    if (user_config_info != NULL) {
        user_config_size = g_file_info_get_size(user_config_info);
        g_object_unref(user_config_info);
    }

    if (!g_file_query_exists(user_config_file, NULL) || user_config_size == 0) {
        g_print("User config does not exist or is empty, copying system config\n");

        gchar *system_config_path = g_strjoin (G_DIR_SEPARATOR_S, SYSCONFDIR, "gromit-mpx", "gromit-mpx.cfg", NULL);
        GFile *system_config_file = g_file_new_for_path(system_config_path);

        GError *error = NULL;
        gboolean result = g_file_copy(system_config_file, user_config_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
        if (!result) {
            g_printerr("Error copying system config to user config: %s\n", error->message);
            g_error_free(error);
        }

        g_object_unref(system_config_file);
        g_free(system_config_path);
    }


    /*
      Open user config for editing.
     */
    gchar *user_config_uri = g_strjoin (G_DIR_SEPARATOR_S, "file://", user_config_path, NULL);

    gtk_show_uri_on_window (NULL,
			    user_config_uri,
			    GDK_CURRENT_TIME,
			    NULL);

    /*
      Clean up
     */
    g_object_unref(user_config_file);
    g_free(user_config_path);
    g_free(user_config_uri);
}


void on_issues(GtkMenuItem *menuitem,
               gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://github.com/bk138/gromit-mpx/issues",
			    GDK_CURRENT_TIME,
			    NULL);
}


void on_support_liberapay(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://liberapay.com/bk138",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_support_patreon(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://patreon.com/bk138",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_support_paypal(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri_on_window (NULL,
			    "https://www.paypal.com/donate?hosted_button_id=N7GSSPRPUSTPU",
			    GDK_CURRENT_TIME,
			    NULL);

}

void on_signal(int signum) {
    // for now only SIGINT and SIGTERM
    gtk_main_quit();
}

