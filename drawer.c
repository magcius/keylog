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

#include "drawer.h"

#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <cairo-xlib.h>

struct _WindowDrawer {
  int damage_event_base;

  Display *xdisplay;
  Window xwindow;
  Damage xdamage;

  WindowDrawerFunc draw_func;
  void *draw_func_data;
};

static cairo_surface_t *
make_surface (WindowDrawer *drawer)
{
  XWindowAttributes attrs;
  XGetWindowAttributes (drawer->xdisplay, drawer->xwindow, &attrs);
  return cairo_xlib_surface_create (drawer->xdisplay, drawer->xwindow,
                                    attrs.visual, attrs.width, attrs.height);
}

static void
fire_redraw (WindowDrawer *drawer)
{
  cairo_surface_t *surface = make_surface (drawer);
  cairo_t *cr = cairo_create (surface);
  drawer->draw_func (drawer, cr, drawer->draw_func_data);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

static GdkFilterReturn
event_filter (GdkXEvent *gdk_xevent,
              GdkEvent  *gdk_event,
              gpointer   data)
{
  WindowDrawer *drawer = data;
  XEvent *xevent = gdk_xevent;

  if (xevent->type == (drawer->damage_event_base) + XDamageNotify &&
      ((XDamageNotifyEvent *) xevent)->damage == drawer->xdamage)
    {
      fire_redraw (drawer);

      /* Subtract damage *after* we redraw, so we don't get in a loop. */
      XDamageSubtract (drawer->xdisplay, drawer->xdamage, None, None);

      return GDK_FILTER_REMOVE;
    }

  return GDK_FILTER_CONTINUE;
}

WindowDrawer *
window_drawer_new (Display          *xdisplay,
                   Window            xwindow,
                   WindowDrawerFunc  draw_func,
                   void             *draw_func_data)
{
  WindowDrawer *drawer = g_slice_new0 (WindowDrawer);
  int damage_error_base;

  drawer->xdisplay = xdisplay;
  drawer->xwindow = xwindow;

  XDamageQueryExtension (drawer->xdisplay,
                         &drawer->damage_event_base,
                         &damage_error_base);

  drawer->xdamage = XDamageCreate (drawer->xdisplay, drawer->xwindow, XDamageReportNonEmpty);

  gdk_window_add_filter (NULL, event_filter, drawer);

  drawer->draw_func = draw_func;
  drawer->draw_func_data = draw_func_data;

  return drawer;
}

void
window_drawer_free (WindowDrawer *drawer)
{
  XDamageDestroy (drawer->xdisplay, drawer->xdamage);
  gdk_window_remove_filter (NULL, event_filter, drawer);
  g_slice_free (WindowDrawer, drawer);
}
