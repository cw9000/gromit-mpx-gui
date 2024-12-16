/* 
 * Gromit -- a program for painting on the screen
 * Copyright (C) 2000 Simon Budig <Simon.Budig@unix-ag.org>
 *
 * MPX modifications Copyright (C) 2009 Christian Beier <dontmind@freeshell.org>
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

#ifndef GROMIT_MPX_MAIN_H
#define GROMIT_MPX_MAIN_H

#include "build-config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#ifdef APPINDICATOR_IS_LEGACY
#include <libappindicator/app-indicator.h>
#else
#include <libayatana-appindicator/app-indicator.h>
#endif

#define GROMIT_MOUSE_EVENTS ( GDK_BUTTON_MOTION_MASK | \
                              GDK_BUTTON_PRESS_MASK | \
                              GDK_BUTTON_RELEASE_MASK )

#define GROMIT_WINDOW_EVENTS ( GROMIT_MOUSE_EVENTS | GDK_EXPOSURE_MASK)

/* Atoms used to control Gromit */
#define GA_CONTROL    gdk_atom_intern ("Gromit/control", FALSE)
#define GA_STATUS     gdk_atom_intern ("Gromit/status", FALSE)
#define GA_QUIT       gdk_atom_intern ("Gromit/quit", FALSE)
#define GA_ACTIVATE   gdk_atom_intern ("Gromit/activate", FALSE)
#define GA_DEACTIVATE gdk_atom_intern ("Gromit/deactivate", FALSE)
#define GA_TOGGLE     gdk_atom_intern ("Gromit/toggle", FALSE)
#define GA_LINE       gdk_atom_intern ("Gromit/line", FALSE)
#define GA_VISIBILITY gdk_atom_intern ("Gromit/visibility", FALSE)
#define GA_CLEAR      gdk_atom_intern ("Gromit/clear", FALSE)
#define GA_RELOAD     gdk_atom_intern ("Gromit/reload", FALSE)
#define GA_UNDO       gdk_atom_intern ("Gromit/undo", FALSE)
#define GA_REDO       gdk_atom_intern ("Gromit/redo", FALSE)
#define GA_GUIMENU    gdk_atom_intern ("Gromit/menutoggle", FALSE)
#define GA_OPENTOGGLE gdk_atom_intern ("Gromit/opentoggle", FALSE)

#define GA_DATA       gdk_atom_intern ("Gromit/data", FALSE)
#define GA_TOGGLEDATA gdk_atom_intern ("Gromit/toggledata", FALSE)
#define GA_LINEDATA   gdk_atom_intern ("Gromit/linedata", FALSE)

#define GROMIT_MAX_UNDO 100
// GROMIT_NUMBER_OF_GUI_TOOLS can be edited to have how many tools you want.
// IF you change this, make sure you delete your .gromit_config or change its name
#define GROMIT_NUMBER_OF_GUI_TOOLS 6
#define NUMBER_OF_PAINT_TYPES 7
#define GROMIT_PAINT_TYPE_STR_LEN 15
#define GROMIT_PAINT_TYPES_STR "_Pen","_Line","_Rect","_Smooth","_Ortho","_Eraser","_Recolor"
#define GROMIT_TOOL_TYPE_ATTRIBUTES "width","arrowsize","arrow_type","minwidth","maxwidth","radius","minlen","maxangle","simplify","snapdist","paint_color","pressure"
typedef enum
{
  GROMIT_PEN,
  GROMIT_LINE,
  GROMIT_RECT,
  GROMIT_SMOOTH,
  GROMIT_ORTHOGONAL,
  GROMIT_RECOLOR,
  GROMIT_ERASER,
  GROMIT_NUMBER_OF_PAINT_TYPES, //7 as of now
  GROMIT_CURRENT_PAINT_TYPE //8 as of now
} GromitPaintType;
typedef enum
{
  GROMIT_WIDTH,
  GROMIT_ARROWSIZE,
  GROMIT_ARROW_TYPE,
  GROMIT_MINWIDTH,
  GROMIT_MAXWIDTH,
  GROMIT_RADIUS,
  GROMIT_MINLEN,
  GROMIT_MAXANGLE,
  GROMIT_SIMPLIFY,
  GROMIT_SNAPDIST,
  GROMIT_PAINT_COLOR,
  GROMIT_PRESSURE,
  GROMIT_NUMBER_OF_PAINT_TYPE_ATTRS
} GromitToolTypeAttributes;

typedef enum
{
  GROMIT_ARROW_NONE = 0,
  GROMIT_ARROW_END = 1,
  GROMIT_ARROW_START = 2,
  GROMIT_ARROW_DOUBLE =3
} GromitArrowType;
typedef struct {
    gint x;
    gint y;
} WindowPosition;

typedef struct
{
  GromitPaintType type;
  guint           width;
  gfloat          arrowsize;
  GromitArrowType arrow_type;
  guint           minwidth;
  guint           maxwidth;
  guint           radius;
  guint           minlen;
  guint           maxangle;
  guint           simplify;
  guint           snapdist;
  GdkRGBA         *paint_color;
  cairo_t         *paint_ctx;
  gdouble         pressure;
} GromitPaintContext;

typedef struct
{
  gdouble      lastx;
  gdouble      lasty;
  guint32      motion_time;
  GList*       coordlist;
  GdkDevice*   device;
  guint        index;
  guint        state;
  GromitPaintContext *cur_context;
  gboolean     is_grabbed;
  gboolean     was_grabbed;
  GdkDevice*   lastslave;
} GromitDeviceData;


typedef struct
{
  GtkWidget   *win;
  AppIndicator *trayicon;

  GdkCursor   *paint_cursor;
  GdkCursor   *erase_cursor;

  GdkDisplay  *display;
  GdkScreen   *screen;
  gboolean     xinerama;
  gboolean     composited;
  GdkWindow   *root;
  gchar       *hot_keyval;
  guint        hot_keycode;
  gchar       *menu_keyval;
  guint        menu_keycode;
  gchar       *undo_keyval;
  guint        undo_keycode;
  gdouble      opacity;

  GdkRGBA     *white;
  GdkRGBA     *black;
  GdkRGBA     *red;
  GdkRGBA     *menu_color1;
  GdkRGBA     *menu_color2;
  GdkRGBA     *menu_color3;
  GdkRGBA     *menu_color4;
  GdkRGBA     *menu_color5;
  GdkRGBA     *menu_color6;


  GromitPaintContext *default_pen;
  GromitPaintContext *default_eraser;
 
  GHashTable  *tool_config;

  cairo_surface_t *backbuffer;
  /* Auxiliary backbuffer for tools like LINE or RECT */
  cairo_surface_t *aux_backbuffer;

  GHashTable  *devdatatable;

  guint        timeout_id;
  guint        modified;
  guint        delayed;
  guint        maxwidth;
  guint        width;
  guint        height;
  guint        client;
  guint        painted;
  gboolean     hidden;
  gboolean     debug;

  gchar       *clientdata;

  /* undo buffer */
  gchar  *undo_buffer[GROMIT_MAX_UNDO];
  size_t undo_buffer_size[GROMIT_MAX_UNDO];
  size_t undo_buffer_used[GROMIT_MAX_UNDO];
  gchar *undo_temp;
  size_t undo_temp_size;
  size_t undo_temp_used;
  gint   undo_head, undo_depth, redo_depth;
  gboolean started_from_gui;

  gboolean show_intro_on_startup;
  gboolean use_graphical_menu_items;
  GromitPaintContext * graph_menu_tools[GROMIT_NUMBER_OF_GUI_TOOLS][GROMIT_NUMBER_OF_PAINT_TYPES+1];
  const gchar *paint_types_str[GROMIT_NUMBER_OF_PAINT_TYPES];
  const gchar *paint_type_attributes_str[GROMIT_NUMBER_OF_PAINT_TYPE_ATTRS];
  int current_graph_menu_tool;
  int current_graph_menu_type[GROMIT_NUMBER_OF_GUI_TOOLS];
  GtkWindow *gui_window;
  gboolean gui_menu_toggle;
  WindowPosition gui_window_position;
  gboolean start_with_gui;
  gboolean open;

} GromitData;


void toggle_visibility (GromitData *data);
void hide_window (GromitData *data);
void show_window (GromitData *data);

void parse_print_help (gpointer key, gpointer value, gpointer user_data);

void select_tool (GromitData *data, GdkDevice *device, GdkDevice *slave_device, guint state);

void copy_surface (cairo_surface_t *dst, cairo_surface_t *src);
void snap_undo_state(GromitData *data);
void undo_drawing (GromitData *data);
void redo_drawing (GromitData *data);
void undo_compress(GromitData *data, cairo_surface_t *surface);
void undo_temp_buffer_to_slot(GromitData *data, gint undo_slot);
void undo_decompress(GromitData *data, gint undo_slot, cairo_surface_t *surface);

void clear_screen (GromitData *data);

GromitPaintContext *paint_context_new (GromitData *data, GromitPaintType type,
				       GdkRGBA *fg_color, guint width,
                                       guint arrowsize, GromitArrowType arrowtype,
                                       guint simpilfy, guint radius, guint maxangle, guint minlen, guint snapdist,
                                       guint minwidth, guint maxwidth);
void paint_context_free (GromitPaintContext *context);

void indicate_active(GromitData *data, gboolean YESNO);

#endif
