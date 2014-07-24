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

#include "utils.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <glib.h>

static Window
find_window_recursively (Display *xdisplay,
                         Window   xwindow,
                         char    *class)
{
  Window found = None;
  XWindowAttributes attrs;
  XGetWindowAttributes (xdisplay, xwindow, &attrs);
  if (attrs.class != InputOutput)
    goto out;
  if (attrs.map_state != IsViewable)
    goto out;

  {
    XClassHint class_hint = { 0 };
    XGetClassHint (xdisplay, xwindow, &class_hint);

    if (g_strcmp0 (class_hint.res_class, class) == 0)
      found = xwindow;

    XFree (class_hint.res_name);
    XFree (class_hint.res_class);

    if (found)
      goto out;
  }

  {
    Window root_dummy, parent_dummy;
    Window *children;
    unsigned int i, n_children;
    XQueryTree (xdisplay, xwindow,
                &root_dummy, &parent_dummy,
                &children, &n_children);

    for (i = 0; i < n_children; i++)
      {
        Window child = children[i];
        found = find_window_recursively (xdisplay, child, class);
        if (found)
          break;
      }

    XFree (children);

    if (found)
      goto out;
  }

 out:
  return found;
}

Window
find_window_with_class (Display *xdisplay, char *class)
{
  return find_window_recursively (xdisplay, DefaultRootWindow (xdisplay), class);
}
