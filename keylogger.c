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

#include "keylogger.h"

#include <glib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>

struct _Keylogger {
  Display *xdisplay;
  GSource *source;
  KeyloggerFunc event_func;
  void *event_func_data;

  int source_id;
};

typedef struct {
  GSource base;
  GPollFD event_poll_fd;
  Display *xdisplay;
} XEventSource;

static gboolean
x_event_source_prepare (GSource *source,
                        int     *timeout)
{
  XEventSource *x_source = (XEventSource *) source;

  *timeout = -1;

  return XPending (x_source->xdisplay);
}

static gboolean
x_event_source_check (GSource *source)
{
  XEventSource *x_source = (XEventSource *) source;

  return XPending (x_source->xdisplay);
}

static gboolean
x_event_source_dispatch (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  XEventSource *x_source = (XEventSource *) source;

  while (XPending (x_source->xdisplay))
    {
      /* Pump the events. This is enough to make the recording work. */
      XEvent event;
      XNextEvent (x_source->xdisplay, &event);
    }

  return TRUE;
}

static GSourceFuncs x_event_funcs = {
  x_event_source_prepare,
  x_event_source_check,
  x_event_source_dispatch,
};

static GSource *
x_event_source_new (Display *xdisplay)
{
  GSource *source;
  XEventSource *x_source;

  source = g_source_new (&x_event_funcs, sizeof (XEventSource));
  x_source = (XEventSource *) source;
  x_source->xdisplay = xdisplay;
  x_source->event_poll_fd.fd = ConnectionNumber (xdisplay);
  x_source->event_poll_fd.events = G_IO_IN;
  g_source_add_poll (source, &x_source->event_poll_fd);

  return source;
}

/* Stolen from XlibInt.c in libX11 */
static void
translate_wire_to_key_event (Display *dpy, XEvent *re, xEvent *event)
{
  re->type = event->u.u.type & 0x7f;
  /* ((XAnyEvent *)re)->serial = _XSetLastRequestRead(dpy, (xGenericReply *)event); */
  ((XAnyEvent *)re)->send_event = ((event->u.u.type & 0x80) != 0);
  ((XAnyEvent *)re)->display = dpy;

  XKeyEvent *ev   = (XKeyEvent*) re;

  ev->root        = event->u.keyButtonPointer.root;
  ev->window      = event->u.keyButtonPointer.event;
  ev->subwindow   = event->u.keyButtonPointer.child;
  ev->time        = event->u.keyButtonPointer.time;
  ev->x 	  = cvtINT16toInt(event->u.keyButtonPointer.eventX);
  ev->y 	  = cvtINT16toInt(event->u.keyButtonPointer.eventY);
  ev->x_root      = cvtINT16toInt(event->u.keyButtonPointer.rootX);
  ev->y_root      = cvtINT16toInt(event->u.keyButtonPointer.rootY);
  ev->state       = event->u.keyButtonPointer.state;
  ev->same_screen = event->u.keyButtonPointer.sameScreen;
  ev->keycode     = event->u.u.detail;
}

static void
keylogger_intercept_proc (XPointer closure, XRecordInterceptData *data)
{
  Keylogger *keylogger = (Keylogger *) closure;

  if (data->category != XRecordFromServer)
    return;

  xReply *reply = (xReply *) data->data;
  switch (reply->generic.type)
    {
    case KeyPress:
    case KeyRelease:
      {
        XEvent ev;

        translate_wire_to_key_event (keylogger->xdisplay, &ev, &reply->event);

        keylogger->event_func (keylogger, (XKeyEvent *) &ev, keylogger->event_func_data);

        break;
      }
    default:
      g_assert_not_reached ();
    }
}

static void
keylogger_init_xrecord (Keylogger *keylogger)
{
  int rec_maj = 1, rec_min = 1;
  XRecordQueryVersion (keylogger->xdisplay, &rec_maj, &rec_min);

  XRecordContext ctx;
  XRecordClientSpec spec = XRecordAllClients;

  XRecordRange *range = XRecordAllocRange ();
  range->device_events.first = KeyPress;
  range->device_events.last = KeyRelease;

  ctx = XRecordCreateContext (keylogger->xdisplay, 0, &spec, 1, &range, 1);
  XRecordEnableContextAsync (keylogger->xdisplay, ctx, keylogger_intercept_proc, (XPointer) keylogger);
}

static Display *
reopen_display (Display *xdisplay)
{
  return XOpenDisplay (DisplayString (xdisplay));
}

Keylogger *
keylogger_new (Display       *xdisplay,
               KeyloggerFunc  event_func,
               void          *event_func_data)
{
  Keylogger *keylogger = g_slice_new0 (Keylogger);

  keylogger->xdisplay = reopen_display (xdisplay);
  keylogger->event_func = event_func;
  keylogger->event_func_data = event_func_data;

  keylogger_init_xrecord (keylogger);

  return keylogger;
}

void
keylogger_start (Keylogger *keylogger)
{
  keylogger->source = x_event_source_new (keylogger->xdisplay);
  keylogger->source_id = g_source_attach (keylogger->source, NULL);
}

void
keylogger_stop (Keylogger *keylogger)
{
  g_source_remove (keylogger->source_id);
  keylogger->source_id = 0;

  g_source_unref (keylogger->source);
  keylogger->source = NULL;
}
