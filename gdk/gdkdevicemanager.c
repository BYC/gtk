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
#include "gdkdevicemanager.h"
#include "gdkintl.h"
#include "gdkinternals.h"


/**
 * SECTION:gdkdevicemanager
 * @Short_description: Functions for handling input devices
 * @Long_description: In addition to a single pointer and keyboard for user interface input, GDK
 *                    contains support for a variety of input devices, including graphics tablets,
 *                    touchscreens and multiple pointers/keyboards interacting simultaneously with
 *                    the user interface. Under X, the support for multiple input devices is done
 *                    through the <firstterm>XInput 2</firstterm> extension, which also supports
 *                    additional features such as sub-pixel positioning information and additional
 *                    device-dependent information.
 * @Title: GdkDeviceManager
 * @See_also: #GdkDevice, #GdkEvent, gdk_enable_multidevice()
 *
 * By default, GDK supports the traditional single keyboard/pointer input scheme (Plus additional
 * special input devices such as tablets. In short, backwards compatible with 2.X). Since version 3.0,
 * if gdk_enable_multidevice() is called before gdk_display_open() and the platform supports it, GDK
 * will be aware of multiple keyboard/pointer pairs interacting simultaneously with the user interface.
 *
 * Conceptually, in multidevice mode there are 2 device types, virtual devices (or master devices)
 * are represented by the pointer cursors and keyboard foci that are seen on the screen. physical
 * devices (or slave devices) represent the hardware that is controlling the virtual devices, and
 * thus has no visible cursor on the screen.
 *
 * Virtual devices are always paired, there is a keyboard device for every pointer device,
 * associations between devices may be inspected through gdk_device_get_associated_device().
 *
 * There may be several virtual devices, and several physical devices could be controlling each of
 * these virtual devices. Physical devices may also be "floating", which means they are not attached
 * to any virtual device.
 *
 * By default, GDK will automatically listen for events coming from all master devices, setting the
 * #GdkDevice for all events coming from input devices
 * <footnote>
 *   Events containing device information are #GDK_MOTION_NOTIFY, #GDK_BUTTON_PRESS, #GDK_2BUTTON_PRESS,
 *   #GDK_3BUTTON_PRESS, #GDK_BUTTON_RELEASE, #GDK_SCROLL, #GDK_KEY_PRESS, #GDK_KEY_RELEASE,
 *   #GDK_ENTER_NOTIFY, #GDK_LEAVE_NOTIFY, #GDK_FOCUS_CHANGE, #GDK_PROXIMITY_IN, #GDK_PROXIMITY_OUT,
 *   #GDK_DRAG_ENTER, #GDK_DRAG_LEAVE, #GDK_DRAG_MOTION, #GDK_DRAG_STATUS, #GDK_DROP_START,
 *   #GDK_DROP_FINISHED and #GDK_GRAB_BROKEN.
 * </footnote>
 * , although gdk_window_set_support_multidevice() has to be called on #GdkWindow<!-- --> in order to
 * support additional features of multiple pointer interaction, such as multiple, per-device enter/leave
 * events. The default setting will emit just one enter/leave event pair for all devices on the window.
 * See gdk_window_set_support_multidevice() documentation for more information.
 *
 * In order to listen for events coming from other than a virtual device, gdk_window_set_device_events()
 * must be called. Generally, this function can be used to modify the event mask for any given device.
 *
 * Input devices may also provide additional information besides X/Y. For example, graphics tablets may
 * also provide pressure and X/Y tilt information. This information is device-dependent, and may be
 * queried through gdk_device_get_axis(). In multidevice mode, virtual devices will change axes in order
 * to always represent the physical device that is routing events through it. Whenever the physical device
 * changes, the #GdkDevice:n-axes property will be notified, and gdk_device_list_axes() will return the
 * new device axes.
 *
 * Devices may also have associated <firstterm>keys</firstterm> or macro buttons. Such keys can be
 * globally set to map into normal X keyboard events. The mapping is set using gdk_device_set_key().
 *
 * In order to query the device hierarchy and be aware of changes in the device hierarchy (such as
 * virtual devices being created or removed, or physical devices being plugged or unplugged), GDK
 * provides #GdkDeviceManager. On X11, multidevice support is implemented through XInput 2. If
 * gdk_enable_multidevice() is called, the XInput 2.x #GdkDeviceManager implementation will be used
 * as input source, else either the core or XInput 1.x implementations will be used.
 */

static void gdk_device_manager_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void gdk_device_manager_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE (GdkDeviceManager, gdk_device_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY
};

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  DEVICE_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };


struct _GdkDeviceManagerPrivate
{
  GdkDisplay *display;
};


static void
gdk_device_manager_class_init (GdkDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_device_manager_set_property;
  object_class->get_property = gdk_device_manager_get_property;

  g_object_class_install_property (object_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        P_("Display"),
                                                        P_("Display for the device manager"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GdkDeviceManager::device-added:
   * @device_manager: the object on which the signal is emitted
   * @device: the newly added #GdkDevice.
   *
   * The ::device-added signal is emitted either when a new master
   * pointer is created, or when a slave (Hardware) input device
   * is plugged in.
   */
  signals [DEVICE_ADDED] =
    g_signal_new (g_intern_static_string ("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  /**
   * GdkDeviceManager::device-removed:
   * @device_manager: the object on which the signal is emitted
   * @device: the just removed #GdkDevice.
   *
   * The ::device-removed signal is emitted either when a master
   * pointer is removed, or when a slave (Hardware) input device
   * is unplugged.
   */
  signals [DEVICE_REMOVED] =
    g_signal_new (g_intern_static_string ("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  /**
   * GdkDeviceManager::device-changed:
   * @device_manager: the object on which the signal is emitted
   * @device: the #GdkDevice that changed.
   *
   * The ::device-changed signal is emitted either when some
   * #GdkDevice has changed the number of either axes or keys.
   * For example In X this will normally happen when the slave
   * device routing events through the master device changes,
   * in that case the master device will change to reflect the
   * new slave device axes and keys.
   */
  signals [DEVICE_CHANGED] =
    g_signal_new (g_intern_static_string ("device-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  g_type_class_add_private (object_class, sizeof (GdkDeviceManagerPrivate));
}

static void
gdk_device_manager_init (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerPrivate *priv;

  device_manager->priv = priv = G_TYPE_INSTANCE_GET_PRIVATE (device_manager,
                                                             GDK_TYPE_DEVICE_MANAGER,
                                                             GdkDeviceManagerPrivate);
}

static void
gdk_device_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkDeviceManagerPrivate *priv;

  priv = GDK_DEVICE_MANAGER (object)->priv;

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_manager_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GdkDeviceManagerPrivate *priv;

  priv = GDK_DEVICE_MANAGER (object)->priv;

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_manager_get_display:
 * @device_manager: a #GdkDeviceManager
 *
 * Gets the #GdkDisplay associated to @device_manager.
 *
 * Returns: the #GdkDisplay to which @device_manager is
 *          associated to, or #NULL.
 *
 * Since: 3.0
 **/
GdkDisplay *
gdk_device_manager_get_display (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerPrivate *priv;

  g_return_val_if_fail (GDK_IS_DEVICE_MANAGER (device_manager), NULL);

  priv = device_manager->priv;

  return priv->display;
}

/**
 * gdk_device_manager_list_devices:
 * @device_manager: a #GdkDeviceManager
 * @type: device type to get.
 *
 * Returns the list of devices of type @type currently attached to
 * @device_manager.
 *
 * Returns: a list of #GdkDevice<!-- -->s. The returned list must be
 *          freed with g_list_free (). The list elements are owned by
 *          GTK+ and must not be freed or unreffed.
 *
 * Since: 3.0
 **/
GList *
gdk_device_manager_list_devices (GdkDeviceManager *device_manager,
                                 GdkDeviceType     type)
{
  g_return_val_if_fail (GDK_IS_DEVICE_MANAGER (device_manager), NULL);

  return GDK_DEVICE_MANAGER_GET_CLASS (device_manager)->list_devices (device_manager, type);
}

/**
 * gdk_device_manager_get_client_pointer:
 * @device_manager: a #GdkDeviceManager
 *
 * Returns the client pointer, that is, the master pointer that acts as the core pointer
 * for this application. In X11, window managers may change this depending on the interaction
 * pattern under the presence of several pointers.
 *
 * You should use this function sheldomly, only in code that isn't triggered by a #GdkEvent
 * and there aren't other means to get a meaningful #GdkDevice to operate on.
 *
 * Returns: The client pointer.
 *
 * Since: 3.0
 **/
GdkDevice *
gdk_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  g_return_val_if_fail (GDK_IS_DEVICE_MANAGER (device_manager), NULL);

  return GDK_DEVICE_MANAGER_GET_CLASS (device_manager)->get_client_pointer (device_manager);
}