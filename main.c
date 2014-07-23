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

#include "keylogger.h"

typedef struct {
  Display *xdisplay;
  Keylogger *keylogger;
  GtkTextBuffer *buf;
} Application;

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
record_toggled (GtkToggleButton *toggle,
                gpointer         data)
{
  Application *app = data;

  if (gtk_toggle_button_get_active (toggle))
    {
      /* Clear the text buffer when starting to record */
      gtk_text_buffer_set_text (app->buf, "", 0);
      keylogger_start (app->keylogger);
    }
  else
    keylogger_stop (app->keylogger);
}

static void
app_init (Application *app)
{
  GtkWidget *window, *vbox, *record, *scroll, *textview;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 700, 500);
  gtk_window_set_title (GTK_WINDOW (window), "Simple Keylogger");
  gtk_container_set_border_width (GTK_CONTAINER (window), 6);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  record = gtk_toggle_button_new_with_label ("Record");
  gtk_container_add (GTK_CONTAINER (vbox), record);

  g_signal_connect (record, "toggled",
                    G_CALLBACK (record_toggled), app);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_widget_set_vexpand (scroll, TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), scroll);

  textview = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_container_add (GTK_CONTAINER (scroll), textview);

  app->buf = gtk_text_buffer_new (NULL);
  gtk_text_buffer_create_tag (app->buf, "monospace",
                              "family", "monospace",
                              "size", 24 * PANGO_SCALE,
                              NULL);
  gtk_text_view_set_buffer (GTK_TEXT_VIEW (textview), app->buf);

  GdkDisplay *display = gtk_widget_get_display (window);
  app->xdisplay = GDK_DISPLAY_XDISPLAY (display);

  app->keylogger = keylogger_new (app->xdisplay, app_key_event, app);

  gtk_widget_show_all (window);
}

int main(int argc, char *argv[])
{
  Application app = { 0 };

  gtk_init (&argc, &argv);

  app_init (&app);

  gtk_main ();
  return 0;
}
