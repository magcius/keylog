/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright (C) 2014 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <cairo-xlib.h>

#include "keylogger.h"
#include "drawer.h"
#include "utils.h"

typedef struct {
  GdkPixbuf *logo_pixbuf;

  Display *xdisplay;
  GtkWidget *window;

  Keylogger *keylogger;
  GtkTextBuffer *buf;

  WindowDrawer *firefox_drawer;
} Application;

static void
on_focus_out (GtkWidget     *widget,
              GdkEventFocus *event,
              gpointer       data)
{
  Application *app = data;
  GdkWindow *window = gtk_widget_get_window (widget);
  Window xwindow = GDK_WINDOW_XID (window);

  XSetInputFocus (app->xdisplay, xwindow, RevertToParent, CurrentTime);
  XRaiseWindow (app->xdisplay, xwindow);
}

static void
steal_focus_toggled (GtkToggleButton *toggle,
                     gpointer         data)
{
  Application *app = data;

  if (gtk_toggle_button_get_active (toggle))
    {
      g_signal_connect (app->window, "focus-out-event",
                        G_CALLBACK (on_focus_out), app);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (app->window,
                                            G_CALLBACK (on_focus_out), app);
    }
}

static char
sym_to_char (KeySym sym)
{
  /* ascii symbols */
  if (sym >= 0x20 && sym <= 0x7E)
    return sym;

  /* hack for return */
  if (sym == XK_Return)
    return 0x0D;

  /* invalid */
  return 0;
}

static void
app_key_event (Keylogger *keylogger,
               XKeyEvent *xkey,
               void      *data)
{
  /* Don't do anything for KeyRelease events. */
  if (xkey->type == KeyRelease)
    return;

  Application *app = data;

  /* Yes, I know it's deprecated. Shut up. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  KeySym sym = XKeycodeToKeysym (app->xdisplay, xkey->keycode, 0);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (sym == XK_BackSpace)
    {
      GtkTextIter iter;
      gtk_text_buffer_get_end_iter (app->buf, &iter);
      gtk_text_buffer_backspace (app->buf, &iter, TRUE, TRUE);
    }

  /* Gross dumb hack. */
  char chr = sym_to_char (sym);

  if (chr > 0)
    {
      char str[1] = { chr };
      GtkTextIter iter;
      gtk_text_buffer_get_end_iter (app->buf, &iter);
      gtk_text_buffer_insert_with_tags_by_name (app->buf, &iter,
                                                str, sizeof (str),
                                                "monospace", NULL);
    }
}

static void
keylog_toggled (GtkToggleButton *toggle,
                gpointer         data)
{
  Application *app = data;

  if (gtk_toggle_button_get_active (toggle))
    {
      /* Clear the text buffer when starting to keylog */
      gtk_text_buffer_set_text (app->buf, "", 0);
      keylogger_start (app->keylogger);
    }
  else
    keylogger_stop (app->keylogger);
}

static void
draw_spam (WindowDrawer *drawer,
           cairo_t      *cr,
           gpointer      data)
{
  Application *app = data;
  int win_height = cairo_xlib_surface_get_height (cairo_get_target (cr));

  int i;
  for (i = 0; i < 80; i++)
    {
      cairo_save (cr);
      int x = (i * 70);
      int y = (i * 230) % win_height;

      cairo_translate (cr, x, y);
      gdk_cairo_set_source_pixbuf (cr, app->logo_pixbuf, 0, 0);
      cairo_rectangle (cr, 0, 0, 256, 256);
      cairo_fill (cr);
      cairo_restore (cr);
    }
}

static void
draw_on_firefox_toggled (GtkToggleButton *toggle,
                         gpointer         data)
{
  Application *app = data;

  if (gtk_toggle_button_get_active (toggle))
    {
      Window firefox_window = find_window_with_class (app->xdisplay, "Firefox");
      app->firefox_drawer = window_drawer_new (app->xdisplay, firefox_window,
                                               draw_spam, app);
    }
  else
    {
      window_drawer_free (app->firefox_drawer);
    }
}

#define APP_TITLE "RealPlayer 10.4 Special Deluxe Freemium Edition (Unregistered Trial)"

static void
app_init (Application *app)
{
  app->logo_pixbuf = gdk_pixbuf_new_from_file ("real.png", NULL);
  gtk_window_set_default_icon (app->logo_pixbuf);

  app->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (app->window, 700, 500);
  gtk_window_set_title (GTK_WINDOW (app->window), APP_TITLE);
  gtk_container_set_border_width (GTK_CONTAINER (app->window), 6);

  GdkDisplay *display = gtk_widget_get_display (app->window);
  app->xdisplay = GDK_DISPLAY_XDISPLAY (display);

  GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (app->window), vbox);

  GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (vbox), hbox);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);

  GdkPixbuf *logo_icon = gdk_pixbuf_scale_simple (app->logo_pixbuf, 64, 64, GDK_INTERP_BILINEAR);
  GtkWidget *logo = gtk_image_new_from_pixbuf (logo_icon);
  g_object_unref (logo_icon);
  gtk_container_add (GTK_CONTAINER (hbox), logo);

  GtkWidget *title = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (title), "<span size='x-large' weight='bold'>" APP_TITLE "</span>");
  gtk_container_add (GTK_CONTAINER (hbox), title);

  GtkWidget *steal_focus = gtk_toggle_button_new_with_label ("Steal Focus");
  gtk_container_add (GTK_CONTAINER (vbox), steal_focus);
  g_signal_connect (steal_focus, "toggled",
                    G_CALLBACK (steal_focus_toggled), app);

  GtkWidget *keylog = gtk_toggle_button_new_with_label ("Keylog");
  gtk_container_add (GTK_CONTAINER (vbox), keylog);
  g_signal_connect (keylog, "toggled",
                    G_CALLBACK (keylog_toggled), app);

  GtkWidget *scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), scroll);

  GtkWidget *textview = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_container_add (GTK_CONTAINER (scroll), textview);

  app->buf = gtk_text_buffer_new (NULL);
  gtk_text_buffer_create_tag (app->buf, "monospace",
                              "family", "monospace",
                              "size", 24 * PANGO_SCALE,
                              NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), app->buf);

  app->keylogger = keylogger_new (app->xdisplay, app_key_event, app);

  GtkWidget *draw_on_firefox = gtk_toggle_button_new_with_label ("Draw on Firefox");
  gtk_container_add (GTK_CONTAINER (vbox), draw_on_firefox);
  g_signal_connect (draw_on_firefox, "toggled",
                    G_CALLBACK (draw_on_firefox_toggled), app);

  gtk_widget_show_all (app->window);
}

int main(int argc, char *argv[])
{
  Application app = { 0 };

  gtk_init (&argc, &argv);

  app_init (&app);

  gtk_main ();
  return 0;
}
