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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "main.h"


gboolean on_expose (GtkWidget *widget,
		    cairo_t* cr,
		    gpointer user_data);

gboolean on_configure (GtkWidget *widget,
		       GdkEventExpose *event,
		       gpointer user_data);


void on_screen_changed(GtkWidget *widget,
		       GdkScreen *previous_screen,
		       gpointer   user_data);

void on_monitors_changed(GdkScreen *screen,
			 gpointer   user_data);

void on_composited_changed(GdkScreen *screen,
			   gpointer   user_data);

void on_clientapp_selection_get (GtkWidget          *widget,
				 GtkSelectionData   *selection_data,
				 guint               info,
				 guint               time,
				 gpointer            user_data);

void on_clientapp_selection_received (GtkWidget *widget,
				      GtkSelectionData *selection_data,
				      guint time,
				      gpointer user_data);

gboolean on_buttonpress (GtkWidget *win, GdkEventButton *ev, gpointer user_data);

gboolean on_motion (GtkWidget *win, GdkEventMotion *ev, gpointer user_data);

gboolean on_buttonrelease (GtkWidget *win, GdkEventButton *ev, gpointer user_data);

void on_mainapp_selection_get (GtkWidget          *widget,
			       GtkSelectionData   *selection_data,
			       guint               info,
			       guint               time,
			       gpointer            user_data);


void on_mainapp_selection_received (GtkWidget *widget,
				    GtkSelectionData *selection_data,
				    guint time,
				    gpointer user_data);


void on_device_removed (GdkDeviceManager *device_manager,
			GdkDevice        *device,
			gpointer          user_data);

void on_device_added (GdkDeviceManager *device_manager,
		      GdkDevice        *device,
		      gpointer          user_data);

void on_signal(int signum);

/*
  menu callbacks
 */
gboolean on_toggle_paint(GtkWidget *widget,
			 GdkEventButton  *ev,
			 gpointer   user_data);
void print_list(GList * list,gchar *title);
void print_box_contents(GtkBox * box);
gboolean on_menu_toggle(GtkMenuItem *menuitem,gpointer user_data);
gboolean on_vbox_changed(GtkWidget *widget, gpointer userdata);
GtkWidget* create_radio_box_pair(GtkRadioButton **group, const gchar *radio_label,gint index,GromitData * data);
GtkWidget* create_radio_button(GtkRadioButton **group, const gchar *label,gint index,GromitData * data);
gboolean on_combo_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, gpointer user_data);
GtkWidget * create_menu_tool_combo(gint index,GromitData * data);
void on_tool_changed(GtkComboBox *tool_combo, gpointer user_data);
void on_opacity_changed(GtkWidget *widget, gpointer user_data);
void on_redo_button(GtkWidget *widget,gpointer user_data);
void on_undo_button(GtkWidget *widget,gpointer user_data);
void on_toggle_paint_gui(GtkWidget *widget, GdkEventButton  *ev, gpointer user_data);
void on_gui_clear(GtkWidget *widget, gpointer user_data);
void on_hide(GtkWidget *widget, gpointer user_data);
void add_general_data_to_key_file(GKeyFile *key_file,GromitData *data);
void save_values(GromitData * data);
void make_paint_ctx(GromitPaintContext *tool_type,GromitData *data);
GtkBox * set_appropriate_tool_options(GtkBox * vbox,gint index,GromitData *data);
void limit_size_vbox(GtkWidget *widget, GdkRectangle *allocation, gpointer data);
GtkBox * create_vbox(GtkBox *vbox,GList * capabilities ,GromitPaintType type, gint index,GromitData *data);
void on_window_close(GtkWidget *button, gpointer user_data);
gboolean on_move_button_pressed(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
GtkWidget * create_arrow_combo(gint index, GromitData * data);
void load_tool_defaults(GromitData * data);

void on_toggle_paint_all (GtkMenuItem *menuitem,
			  gpointer     user_data);

void on_clear (GtkMenuItem *menuitem,
	       gpointer     user_data);

void on_toggle_vis(GtkMenuItem *menuitem,
		   gpointer     user_data);

void on_thicker_lines(GtkMenuItem *menuitem,
		      gpointer     user_data);

void on_thinner_lines(GtkMenuItem *menuitem,
		      gpointer     user_data);

void on_opacity_bigger(GtkMenuItem *menuitem,
		       gpointer     user_data);

void on_opacity_lesser(GtkMenuItem *menuitem,
		       gpointer     user_data);

void on_undo(GtkMenuItem *menuitem,
	     gpointer     user_data);

void on_redo(GtkMenuItem *menuitem,
	     gpointer     user_data);

void on_about(GtkMenuItem *menuitem,
	      gpointer     user_data);

void on_intro(GtkMenuItem *menuitem,
	      gpointer user_data);

void on_edit_config(GtkMenuItem *menuitem,
                    gpointer user_data);

void on_issues(GtkMenuItem *menuitem,
               gpointer user_data);

void on_support_liberapay(GtkMenuItem *menuitem,
			  gpointer user_data);

void on_support_patreon(GtkMenuItem *menuitem,
			gpointer user_data);

void on_support_paypal(GtkMenuItem *menuitem,
		       gpointer user_data);

#endif
