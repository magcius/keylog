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

#pragma once

#include <X11/Xlib.h>
#include <cairo.h>

typedef struct _WindowDrawer WindowDrawer;
typedef void (*WindowDrawerFunc) (WindowDrawer *drawer, cairo_t *cr, void *data);

WindowDrawer * window_drawer_new (Display          *xdisplay,
                                  Window            xwindow,
                                  WindowDrawerFunc  draw_func,
                                  void             *draw_func_data);

void window_drawer_free (WindowDrawer *drawer);
