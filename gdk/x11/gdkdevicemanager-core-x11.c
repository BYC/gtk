/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkx11devicemanager-core.h"
#include "gdkx11device-core.h"

#include "gdkdevicemanagerprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventtranslator.h"
#include "gdkprivate-x11.h"
#include "gdkkeysyms.h"

#ifdef HAVE_XKB
#include <X11/XKBlib.h>
#endif


#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    gdk_x11_device_manager_core_finalize    (GObject *object);
static void    gdk_x11_device_manager_core_constructed (GObject *object);

static GList * gdk_x11_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                                         GdkDeviceType     type);
static GdkDevice * gdk_x11_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager);

static void     gdk_x11_device_manager_event_translator_init (GdkEventTranslatorIface *iface);

static gboolean gdk_x11_device_manager_core_translate_event  (GdkEventTranslator *translator,
                                                              GdkDisplay         *display,
                                                              GdkEvent           *event,
                                                              XEvent             *xevent);


G_DEFINE_TYPE_WITH_CODE (GdkX11DeviceManagerCore, gdk_x11_device_manager_core, GDK_TYPE_DEVICE_MANAGER,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_device_manager_event_translator_init))

static void
gdk_x11_device_manager_core_class_init (GdkX11DeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_x11_device_manager_core_finalize;
  object_class->constructed = gdk_x11_device_manager_core_constructed;
  device_manager_class->list_devices = gdk_x11_device_manager_core_list_devices;
  device_manager_class->get_client_pointer = gdk_x11_device_manager_core_get_client_pointer;
}

static void
gdk_x11_device_manager_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_x11_device_manager_core_translate_event;
}

static GdkDevice *
create_core_pointer (GdkDeviceManager *device_manager,
                     GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static GdkDevice *
create_core_keyboard (GdkDeviceManager *device_manager,
                      GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_X11_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_x11_device_manager_core_init (GdkX11DeviceManagerCore *device_manager)
{
}

static void
gdk_x11_device_manager_core_finalize (GObject *object)
{
  GdkX11DeviceManagerCore *device_manager_core;

  device_manager_core = GDK_X11_DEVICE_MANAGER_CORE (object);

  g_object_unref (device_manager_core->core_pointer);
  g_object_unref (device_manager_core->core_keyboard);

  G_OBJECT_CLASS (gdk_x11_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_x11_device_manager_core_constructed (GObject *object)
{
  GdkX11DeviceManagerCore *device_manager;
  GdkDisplay *display;

  device_manager = GDK_X11_DEVICE_MANAGER_CORE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);
}

static void
translate_key_event (GdkDisplay              *display,
                     GdkX11DeviceManagerCore *device_manager,
                     GdkEvent                *event,
                     XEvent                  *xevent)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (display);
  GdkModifierType consumed, state;
  gunichar c = 0;
  gchar buf[7];

  event->key.type = xevent->xany.type == KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;
  event->key.time = xevent->xkey.time;
  gdk_event_set_device (event, device_manager->core_keyboard);

  event->key.state = (GdkModifierType) xevent->xkey.state;
  event->key.group = _gdk_x11_get_group_for_state (display, xevent->xkey.state);
  event->key.hardware_keycode = xevent->xkey.keycode;

  event->key.keyval = GDK_KEY_VoidSymbol;

  gdk_keymap_translate_keyboard_state (keymap,
                                       event->key.hardware_keycode,
                                       event->key.state,
                                       event->key.group,
                                       &event->key.keyval,
                                       NULL, NULL, &consumed);

  state = event->key.state & ~consumed;
  _gdk_x11_keymap_add_virt_mods (keymap, &state);
  event->key.state |= state;

  event->key.is_modifier = _gdk_x11_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->key.string = NULL;

  if (event->key.keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->key.keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib
       */
      if (event->key.state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
          else if (c == '2')
            {
              event->key.string = g_memdup ("\0\0", 2);
              event->key.length = 1;
              buf[0] = '\0';
              goto out;
            }
          else if (c >= '3' && c <= '7') c -= ('3' - '\033');
          else if (c == '8') c = '\177';
          else if (c == '/') c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->key.string = g_locale_from_utf8 (buf, len,
                                              NULL, &bytes_written,
                                              NULL);
      if (event->key.string)
        event->key.length = bytes_written;
    }
  else if (event->key.keyval == GDK_KEY_Escape)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\033");
    }
  else if (event->key.keyval == GDK_KEY_Return ||
          event->key.keyval == GDK_KEY_KP_Enter)
    {
      event->key.length = 1;
      event->key.string = g_strdup ("\r");
    }

  if (!event->key.string)
    {
      event->key.length = 0;
      event->key.string = g_strdup ("");
    }

 out:
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      g_message ("%s:\t\twindow: %ld     key: %12s  %d",
                 event->type == GDK_KEY_PRESS ? "key press  " : "key release",
                 xevent->xkey.window,
                 event->key.keyval ? gdk_keyval_name (event->key.keyval) : "(none)",
                 event->key.keyval);

      if (event->key.length > 0)
        g_message ("\t\tlength: %4d string: \"%s\"",
                   event->key.length, buf);
    }
#endif /* G_ENABLE_DEBUG */
  return;
}

#ifdef G_ENABLE_DEBUG
static const char notify_modes[][19] = {
  "NotifyNormal",
  "NotifyGrab",
  "NotifyUngrab",
  "NotifyWhileGrabbed"
};

static const char notify_details[][23] = {
  "NotifyAncestor",
  "NotifyVirtual",
  "NotifyInferior",
  "NotifyNonlinear",
  "NotifyNonlinearVirtual",
  "NotifyPointer",
  "NotifyPointerRoot",
  "NotifyDetailNone"
};
#endif

static void
set_user_time (GdkWindow *window,
               GdkEvent  *event)
{
  g_return_if_fail (event != NULL);

  window = gdk_window_get_toplevel (event->client.window);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
    gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
                                  gdk_event_get_time (event));
}

static void
generate_focus_event (GdkX11DeviceManagerCore *device_manager,
                      GdkWindow               *window,
                      gboolean                 in)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = in;
  gdk_event_set_device (event, device_manager->core_keyboard);

  gdk_event_put (event);
  gdk_event_free (event);
}

static gboolean
set_screen_from_root (GdkDisplay *display,
                      GdkEvent   *event,
                      Window      xrootwin)
{
  GdkScreen *screen;

  screen = _gdk_x11_display_screen_for_xrootwin (display, xrootwin);

  if (screen)
    {
      gdk_event_set_screen (event, screen);

      return TRUE;
    }

  return FALSE;
}

static GdkCrossingMode
translate_crossing_mode (int mode)
{
  switch (mode)
    {
    case NotifyNormal:
      return GDK_CROSSING_NORMAL;
    case NotifyGrab:
      return GDK_CROSSING_GRAB;
    case NotifyUngrab:
      return GDK_CROSSING_UNGRAB;
    default:
      g_assert_not_reached ();
    }
}

static GdkNotifyType
translate_notify_type (int detail)
{
  switch (detail)
    {
    case NotifyInferior:
      return GDK_NOTIFY_INFERIOR;
    case NotifyAncestor:
      return GDK_NOTIFY_ANCESTOR;
    case NotifyVirtual:
      return GDK_NOTIFY_VIRTUAL;
    case NotifyNonlinear:
      return GDK_NOTIFY_NONLINEAR;
    case NotifyNonlinearVirtual:
      return GDK_NOTIFY_NONLINEAR_VIRTUAL;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
is_parent_of (GdkWindow *parent,
              GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
        return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static GdkWindow *
get_event_window (GdkEventTranslator *translator,
                  XEvent             *xevent)
{
  GdkDeviceManager *device_manager;
  GdkDisplay *display;
  GdkWindow *window;

  device_manager = GDK_DEVICE_MANAGER (translator);
  display = gdk_device_manager_get_display (device_manager);
  window = gdk_x11_window_lookup_for_display (display, xevent->xany.window);

  /* Apply keyboard grabs to non-native windows */
  if (xevent->type == KeyPress || xevent->type == KeyRelease)
    {
      GdkDeviceGrabInfo *info;
      gulong serial;

      serial = _gdk_display_get_next_serial (display);
      info = _gdk_display_has_device_grab (display,
                                           GDK_X11_DEVICE_MANAGER_CORE (device_manager)->core_keyboard,
                                           serial);
      if (info &&
          (!is_parent_of (info->window, window) ||
           !info->owner_events))
        {
          /* Report key event against grab window */
          window = info->window;
        }
    }

  return window;
}

static gboolean
gdk_x11_device_manager_core_translate_event (GdkEventTranslator *translator,
                                             GdkDisplay         *display,
                                             GdkEvent           *event,
                                             XEvent             *xevent)
{
  GdkX11DeviceManagerCore *device_manager;
  GdkWindow *window;
  GdkWindowImplX11 *window_impl = NULL;
  gboolean return_val;
  GdkToplevelX11 *toplevel = NULL;
  GdkX11Display *display_x11 = GDK_X11_DISPLAY (display);

  device_manager = GDK_X11_DEVICE_MANAGER_CORE (translator);
  return_val = FALSE;

  window = get_event_window (translator, xevent);

  if (window)
    {
      if (GDK_WINDOW_DESTROYED (window) || !GDK_IS_WINDOW (window))
        return FALSE;

      toplevel = _gdk_x11_window_get_toplevel (window);
      window_impl = GDK_WINDOW_IMPL_X11 (window->impl);
      g_object_ref (window);
    }

  event->any.window = window;
  event->any.send_event = xevent->xany.send_event ? TRUE : FALSE;

  if (window && GDK_WINDOW_DESTROYED (window))
    {
      if (xevent->type != DestroyNotify)
        {
          return_val = FALSE;
          goto done;
        }
    }

  if (window &&
      (xevent->type == MotionNotify ||
       xevent->type == ButtonRelease))
    {
      if (_gdk_x11_moveresize_handle_event (xevent))
        {
          return_val = FALSE;
          goto done;
        }
    }

  /* We do a "manual" conversion of the XEvent to a
   *  GdkEvent. The structures are mostly the same so
   *  the conversion is fairly straightforward. We also
   *  optionally print debugging info regarding events
   *  received.
   */

  return_val = TRUE;

  switch (xevent->type)
    {
    case KeyPress:
      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }
      translate_key_event (display, device_manager, event, xevent);
      set_user_time (window, event);
      break;

    case KeyRelease:
      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* Emulate detectable auto-repeat by checking to see
       * if the next event is a key press with the same
       * keycode and timestamp, and if so, ignoring the event.
       */

      if (!display_x11->have_xkb_autorepeat && XPending (xevent->xkey.display))
        {
          XEvent next_event;

          XPeekEvent (xevent->xkey.display, &next_event);

          if (next_event.type == KeyPress &&
              next_event.xkey.keycode == xevent->xkey.keycode &&
              next_event.xkey.time == xevent->xkey.time)
            {
              return_val = FALSE;
              break;
            }
        }

      translate_key_event (display, device_manager, event, xevent);
      break;

    case ButtonPress:
      GDK_NOTE (EVENTS,
                g_message ("button press:\t\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* If we get a ButtonPress event where the button is 4 or 5,
         it's a Scroll event */
      switch (xevent->xbutton.button)
        {
        case 4: /* up */
        case 5: /* down */
        case 6: /* left */
        case 7: /* right */
          event->scroll.type = GDK_SCROLL;

          if (xevent->xbutton.button == 4)
            event->scroll.direction = GDK_SCROLL_UP;
          else if (xevent->xbutton.button == 5)
            event->scroll.direction = GDK_SCROLL_DOWN;
          else if (xevent->xbutton.button == 6)
            event->scroll.direction = GDK_SCROLL_LEFT;
          else
            event->scroll.direction = GDK_SCROLL_RIGHT;

          event->scroll.window = window;
          event->scroll.time = xevent->xbutton.time;
          event->scroll.x = (gdouble) xevent->xbutton.x;
          event->scroll.y = (gdouble) xevent->xbutton.y;
          event->scroll.x_root = (gdouble) xevent->xbutton.x_root;
          event->scroll.y_root = (gdouble) xevent->xbutton.y_root;
          event->scroll.state = (GdkModifierType) xevent->xbutton.state;
          event->scroll.device = device_manager->core_pointer;

          if (!set_screen_from_root (display, event, xevent->xbutton.root))
            {
              return_val = FALSE;
              break;
            }

          break;

        default:
          event->button.type = GDK_BUTTON_PRESS;
          event->button.window = window;
          event->button.time = xevent->xbutton.time;
          event->button.x = (gdouble) xevent->xbutton.x;
          event->button.y = (gdouble) xevent->xbutton.y;
          event->button.x_root = (gdouble) xevent->xbutton.x_root;
          event->button.y_root = (gdouble) xevent->xbutton.y_root;
          event->button.axes = NULL;
          event->button.state = (GdkModifierType) xevent->xbutton.state;
          event->button.button = xevent->xbutton.button;
          event->button.device = device_manager->core_pointer;

          if (!set_screen_from_root (display, event, xevent->xbutton.root))
            return_val = FALSE;

          break;
        }

      set_user_time (window, event);

      break;

    case ButtonRelease:
      GDK_NOTE (EVENTS,
                g_message ("button release:\twindow: %ld  x,y: %d %d  button: %d",
                           xevent->xbutton.window,
                           xevent->xbutton.x, xevent->xbutton.y,
                           xevent->xbutton.button));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      /* We treat button presses as scroll wheel events, so ignore the release */
      if (xevent->xbutton.button == 4 || xevent->xbutton.button == 5 ||
          xevent->xbutton.button == 6 || xevent->xbutton.button == 7)
        {
          return_val = FALSE;
          break;
        }

      event->button.type = GDK_BUTTON_RELEASE;
      event->button.window = window;
      event->button.time = xevent->xbutton.time;
      event->button.x = (gdouble) xevent->xbutton.x;
      event->button.y = (gdouble) xevent->xbutton.y;
      event->button.x_root = (gdouble) xevent->xbutton.x_root;
      event->button.y_root = (gdouble) xevent->xbutton.y_root;
      event->button.axes = NULL;
      event->button.state = (GdkModifierType) xevent->xbutton.state;
      event->button.button = xevent->xbutton.button;
      event->button.device = device_manager->core_pointer;

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        return_val = FALSE;

      break;

    case MotionNotify:
      GDK_NOTE (EVENTS,
                g_message ("motion notify:\t\twindow: %ld  x,y: %d %d  hint: %s",
                           xevent->xmotion.window,
                           xevent->xmotion.x, xevent->xmotion.y,
                           (xevent->xmotion.is_hint) ? "true" : "false"));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xevent->xmotion.time;
      event->motion.x = (gdouble) xevent->xmotion.x;
      event->motion.y = (gdouble) xevent->xmotion.y;
      event->motion.x_root = (gdouble) xevent->xmotion.x_root;
      event->motion.y_root = (gdouble) xevent->xmotion.y_root;
      event->motion.axes = NULL;
      event->motion.state = (GdkModifierType) xevent->xmotion.state;
      event->motion.is_hint = xevent->xmotion.is_hint;
      event->motion.device = device_manager->core_pointer;

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      break;

    case EnterNotify:
      GDK_NOTE (EVENTS,
                g_message ("enter notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail,
                           xevent->xcrossing.subwindow));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      event->crossing.type = GDK_ENTER_NOTIFY;
      event->crossing.window = window;
      gdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.subwindow = gdk_x11_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.subwindow = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x;
      event->crossing.y = (gdouble) xevent->xcrossing.y;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

    case LeaveNotify:
      GDK_NOTE (EVENTS,
                g_message ("leave notify:\t\twindow: %ld  detail: %d subwin: %ld",
                           xevent->xcrossing.window,
                           xevent->xcrossing.detail, xevent->xcrossing.subwindow));

      if (window == NULL)
        {
          return_val = FALSE;
          break;
        }

      if (!set_screen_from_root (display, event, xevent->xbutton.root))
        {
          return_val = FALSE;
          break;
        }

      event->crossing.type = GDK_LEAVE_NOTIFY;
      event->crossing.window = window;
      gdk_event_set_device (event, device_manager->core_pointer);

      /* If the subwindow field of the XEvent is non-NULL, then
       *  lookup the corresponding GdkWindow.
       */
      if (xevent->xcrossing.subwindow != None)
        event->crossing.subwindow = gdk_x11_window_lookup_for_display (display, xevent->xcrossing.subwindow);
      else
        event->crossing.subwindow = NULL;

      event->crossing.time = xevent->xcrossing.time;
      event->crossing.x = (gdouble) xevent->xcrossing.x;
      event->crossing.y = (gdouble) xevent->xcrossing.y;
      event->crossing.x_root = (gdouble) xevent->xcrossing.x_root;
      event->crossing.y_root = (gdouble) xevent->xcrossing.y_root;

      event->crossing.mode = translate_crossing_mode (xevent->xcrossing.mode);
      event->crossing.detail = translate_notify_type (xevent->xcrossing.detail);

      event->crossing.focus = xevent->xcrossing.focus;
      event->crossing.state = xevent->xcrossing.state;

      break;

      /* We only care about focus events that indicate that _this_
       * window (not a ancestor or child) got or lost the focus
       */
    case FocusIn:
      GDK_NOTE (EVENTS,
                g_message ("focus in:\t\twindow: %ld, detail: %s, mode: %s",
                           xevent->xfocus.window,
                           notify_details[xevent->xfocus.detail],
                           notify_modes[xevent->xfocus.mode]));

      if (toplevel)
        {
          gboolean had_focus = HAS_FOCUS (toplevel);

          switch (xevent->xfocus.detail)
            {
            case NotifyAncestor:
            case NotifyVirtual:
              /* When the focus moves from an ancestor of the window to
               * the window or a descendent of the window, *and* the
               * pointer is inside the window, then we were previously
               * receiving keystroke events in the has_pointer_focus
               * case and are now receiving them in the
               * has_focus_window case.
               */
              if (toplevel->has_pointer &&
                  xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_pointer_focus = FALSE;

              /* fall through */
            case NotifyNonlinear:
            case NotifyNonlinearVirtual:
              if (xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_focus_window = TRUE;
              /* We pretend that the focus moves to the grab
               * window, so we pay attention to NotifyGrab
               * NotifyUngrab, and ignore NotifyWhileGrabbed
               */
              if (xevent->xfocus.mode != NotifyWhileGrabbed)
                toplevel->has_focus = TRUE;
              break;
            case NotifyPointer:
              /* The X server sends NotifyPointer/NotifyGrab,
               * but the pointer focus is ignored while a
               * grab is in effect
               */
              if (xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_pointer_focus = TRUE;
              break;
            case NotifyInferior:
            case NotifyPointerRoot:
            case NotifyDetailNone:
              break;
            }

          if (HAS_FOCUS (toplevel) != had_focus)
            generate_focus_event (device_manager, window, TRUE);
        }
      break;
    case FocusOut:
      GDK_NOTE (EVENTS,
                g_message ("focus out:\t\twindow: %ld, detail: %s, mode: %s",
                           xevent->xfocus.window,
                           notify_details[xevent->xfocus.detail],
                           notify_modes[xevent->xfocus.mode]));

      if (toplevel)
        {
          gboolean had_focus = HAS_FOCUS (toplevel);

          switch (xevent->xfocus.detail)
            {
            case NotifyAncestor:
            case NotifyVirtual:
              /* When the focus moves from the window or a descendent
               * of the window to an ancestor of the window, *and* the
               * pointer is inside the window, then we were previously
               * receiving keystroke events in the has_focus_window
               * case and are now receiving them in the
               * has_pointer_focus case.
               */
              if (toplevel->has_pointer &&
                  xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_pointer_focus = TRUE;

              /* fall through */
            case NotifyNonlinear:
            case NotifyNonlinearVirtual:
              if (xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_focus_window = FALSE;
              if (xevent->xfocus.mode != NotifyWhileGrabbed)
                toplevel->has_focus = FALSE;
              break;
            case NotifyPointer:
              if (xevent->xfocus.mode != NotifyGrab &&
                  xevent->xfocus.mode != NotifyUngrab)
                toplevel->has_pointer_focus = FALSE;
            break;
            case NotifyInferior:
            case NotifyPointerRoot:
            case NotifyDetailNone:
              break;
            }

          if (HAS_FOCUS (toplevel) != had_focus)
            generate_focus_event (device_manager, window, FALSE);
        }
      break;

    default:
        return_val = FALSE;
    }

 done:
  if (return_val)
    {
      if (event->any.window)
        g_object_ref (event->any.window);

      if (((event->any.type == GDK_ENTER_NOTIFY) ||
           (event->any.type == GDK_LEAVE_NOTIFY)) &&
          (event->crossing.subwindow != NULL))
        g_object_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }

  if (window)
    g_object_unref (window);

  return return_val;
}

static GList *
gdk_x11_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                          GdkDeviceType     type)
{
  GdkX11DeviceManagerCore *device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      device_manager_core = (GdkX11DeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, device_manager_core->core_pointer);
    }

  return devices;
}

static GdkDevice *
gdk_x11_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkX11DeviceManagerCore *device_manager_core;

  device_manager_core = (GdkX11DeviceManagerCore *) device_manager;
  return device_manager_core->core_pointer;
}