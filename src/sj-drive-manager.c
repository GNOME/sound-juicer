/* sj-drive-manager.c
 *
 * Copyright 2023 Link Dupont <link@sub-pop.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "sj-drive-manager.h"

struct _SjDriveManager
{
  GObject parent_instance;
};

typedef struct
{
  UDisksClient *client;
  gulong object_added_signal_id;
  gulong object_removed_signal_id;
  GHashTable *drives;
} SjDriveManagerPrivate;

static void sj_drive_manager_initable_interface_init (GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED (SjDriveManager,
                        sj_drive_manager,
                        G_TYPE_OBJECT,
                        G_TYPE_FLAG_FINAL,
                        G_ADD_PRIVATE (SjDriveManager)
                        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, sj_drive_manager_initable_interface_init))

enum {
  SIGNAL_DRIVE_ADDED,
  SIGNAL_DRIVE_REMOVED,
  SIGNAL_MEDIA_ADDED,
  SIGNAL_MEDIA_REMOVED,
  N_SIGNALS
};

static guint signals [N_SIGNALS];

/**
 * sj_drive_manager_new_sync:
 * @cancellable: (nullable): A #GCancellable.
 * @error: (nullable) (out): Return location for a #GError.
 *
 * Creates a new #SjDriveManager.
 *
 * Returns: (nullable) (transfer full): A newly allocated #SjDriveManager. If
 * %NULL, @error will contain details.
 */
SjDriveManager *
sj_drive_manager_new_sync (GCancellable *cancellable, GError **error)
{
  GObject *object = g_object_new (SJ_TYPE_DRIVE_MANAGER, NULL);
  if (!g_initable_init (G_INITABLE (object), cancellable, error)) {
    return NULL;
  }
  return SJ_DRIVE_MANAGER (object);
}

/**
 * sj_drive_manager_get_drive_for_device:
 * @device: A block device path (i.e. "/dev/sr0").
 *
 * Scans the drive list for a drive associated with @device.
 *
 * Returns: (transfer full): (nullable): The #UDisksDrive instance associated
 * with @device, if any.
 */
UDisksDrive *
sj_drive_manager_get_drive_for_device (SjDriveManager *self,
                                       const gchar    *device)
{
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  g_autoptr (UDisksBlock) block = sj_drive_manager_get_block_for_device (self, device);
  if (block == NULL) {
    return NULL;
  }
  return udisks_client_get_drive_for_block (priv->client, block);
}

/**
 * sj_drive_manager_get_block_for_device:
 * @device: A block device path (i.e. "/dev/sr0").
 *
 * Scans the block device list for a drive associated with @device.
 *
 * Returns: (transfer full): (nullable): The #UDisksBlock instance associated
 * with @device, if any.
 */
UDisksBlock *
sj_drive_manager_get_block_for_device (SjDriveManager *self,
                                       const gchar    *device)
{
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  g_autofree gchar *basename = g_path_get_basename (device);
  g_autofree gchar *object_path = g_strjoin ("/", "/org/freedesktop/UDisks2/block_devices", basename, NULL);
  g_autoptr (UDisksObject) object = udisks_client_get_object (priv->client, object_path);
  if (object == NULL) {
    return NULL;
  }
  return udisks_object_get_block (object);
}

/**
 * sj_drive_manager_get_block_for_drive:
 * @drive: A #UDisksDrive instance.
 *
 * Retrieves the #UDisksBlock associated with @drive, if any.
 *
 * Returns: (transfer full): (nullable): The #UDisksBlock associated with @drive.
 */
UDisksBlock *
sj_drive_manager_get_block_for_drive (SjDriveManager *self,
                                      UDisksDrive    *drive)
{
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);
  return udisks_client_get_block_for_drive (priv->client, drive, FALSE);
}

/**
 * sj_drive_manager_get_drives:
 *
 * Get a list of optical drives.
 *
 * Returns: (transfer container): (element-type #UDisksDrive): A #GList of
 * #UDiskDrive instances.
 */
GList *
sj_drive_manager_get_drives (SjDriveManager *self)
{
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  return g_hash_table_get_values (priv->drives);
}

/**
 * sj_drive_manager_drive_has_media_compatibility:
 * @drive: The #UDisksDrive instance to scan.
 * @media: The media-compatibility to search for.
 *
 * Scan the @drive media-compatibility property list for @media. See
 * http://storaged.org/doc/udisks2-api/latest/gdbus-org.freedesktop.UDisks2.Drive.html#gdbus-property-org-freedesktop-UDisks2-Drive.MediaCompatibility
 * for a full list of possible media values.
 *
 * Returns: %TRUE if @drive includes @media in its media-compatibility
 * property list.
 */
gboolean
sj_drive_manager_drive_has_media_compatibility (SjDriveManager *self,
                                                UDisksDrive    *drive,
                                                const gchar    *media)
{
  return g_strv_contains (udisks_drive_get_media_compatibility (drive), media);
}

static void
sj_drive_manager_finalize (GObject *object)
{
  G_OBJECT_CLASS (sj_drive_manager_parent_class)->finalize (object);
}

static void
sj_drive_manager_dispose (GObject *object)
{
  SjDriveManager *self = SJ_DRIVE_MANAGER (object);
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  GDBusObjectManager *manager = udisks_client_get_object_manager (priv->client);
  g_assert_nonnull (manager);
  g_signal_handler_disconnect (manager, priv->object_added_signal_id);
  g_signal_handler_disconnect (manager, priv->object_removed_signal_id);

  g_object_unref (priv->client);

  G_OBJECT_CLASS (sj_drive_manager_parent_class)->dispose (object);
}

static void
sj_drive_manager_class_init (SjDriveManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = sj_drive_manager_dispose;
  object_class->finalize = sj_drive_manager_finalize;

  /**
   * SjDriveManager::drive-added:
   * @drive: The newly added drive.
   *
   * The ::drive-added signal is emitted when a new optical drive was connected
   * to the system.
   */
  signals[SIGNAL_DRIVE_ADDED] = g_signal_new ("drive-added",
                                               G_TYPE_FROM_CLASS (object_class),
                                               G_SIGNAL_RUN_LAST,
                                               0,
                                               NULL,
                                               NULL,
                                               NULL,
                                               G_TYPE_NONE,
                                               1,
                                               UDISKS_TYPE_DRIVE,
                                               NULL);

  /**
   * SjDriveManager::drive-removed:
   * @drive: The removed drive.
   *
   * The ::drive-removed signal is emitted when an optical drive is disconnected
   * from the system.
   */
  signals[SIGNAL_DRIVE_REMOVED] = g_signal_new ("drive-removed",
                                                G_TYPE_FROM_CLASS (object_class),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                UDISKS_TYPE_DRIVE,
                                                NULL);

  /**
   * SjDriveManager::media-added:
   * @drive: The drive to which media was added.
   *
   * The ::media-added signal is emitted when media is added to a drive.
   */
  signals[SIGNAL_MEDIA_ADDED] = g_signal_new ("media-added",
                                              G_TYPE_FROM_CLASS (object_class),
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL,
                                              NULL,
                                              NULL,
                                              G_TYPE_NONE,
                                              1,
                                              UDISKS_TYPE_DRIVE,
                                              NULL);

  /**
   * SjDriveManager::media-removed:
   * @drive: The drive from which media was removed.
   *
   * The ::media-removed signal is emitted when media is removed from a drive.
   */
  signals[SIGNAL_MEDIA_REMOVED] = g_signal_new ("media-removed",
                                                G_TYPE_FROM_CLASS (object_class),
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL,
                                                NULL,
                                                NULL,
                                                G_TYPE_NONE,
                                                1,
                                                UDISKS_TYPE_DRIVE,
                                                NULL);
}

static void
media_available_handler (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    data)
{
  SjDriveManager *self = SJ_DRIVE_MANAGER (data);

  UDisksDrive *drive = UDISKS_DRIVE (object);
  gboolean media_available = udisks_drive_get_media_available (drive);
  g_debug ("drive: %s media-available: %d", udisks_drive_get_id (drive), media_available);

  if (media_available) {
    g_signal_emit (self, signals[SIGNAL_MEDIA_ADDED], 0, drive);
  } else {
    g_signal_emit (self, signals[SIGNAL_MEDIA_REMOVED], 0, drive);
  }
}

static void
object_removed_handler (GDBusObjectManager *manager,
                        GDBusObject        *object,
                        gpointer            data)
{
  SjDriveManager *self = SJ_DRIVE_MANAGER (data);
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  UDisksObject *obj = UDISKS_OBJECT (object);
  g_autoptr (UDisksDrive) drive = udisks_object_get_drive (obj);

  if (drive) {
    g_debug ("object removed: %s", g_dbus_object_get_object_path (object));
    if (sj_drive_manager_drive_has_media_compatibility (self, drive, "optical_cd")) {
      g_debug ("optical_cd drive removed: %s", udisks_drive_get_id (drive));
      g_signal_handlers_disconnect_by_func (drive, G_CALLBACK (media_available_handler), self);
      g_hash_table_remove (priv->drives, udisks_drive_get_id (drive));
      g_signal_emit (self, signals[SIGNAL_DRIVE_REMOVED], 0, drive);
    }
  }
}

static void
object_added_handler (GDBusObjectManager *manager,
                      GDBusObject        *object,
                      gpointer            data)
{
  SjDriveManager *self = SJ_DRIVE_MANAGER (data);
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  UDisksObject *obj = UDISKS_OBJECT (object);
  g_autoptr (UDisksDrive) drive = udisks_object_get_drive (obj);

  if (drive) {
    g_debug ("object added: %s", g_dbus_object_get_object_path (object));
    if (sj_drive_manager_drive_has_media_compatibility (self, drive, "optical_cd")) {
      g_debug ("optical_cd drive added: %s", udisks_drive_get_id (drive));
      g_hash_table_insert (priv->drives, udisks_drive_dup_id (drive), g_object_ref (drive));
      g_signal_connect (drive, "notify::media-available", G_CALLBACK (media_available_handler), self);
      g_signal_emit (self, signals[SIGNAL_DRIVE_ADDED], 0, drive);
    }
  }
}

static gboolean
sj_drive_manager_initable_init (GInitable     *initable,
                                GCancellable  *cancellable,
                                GError       **error)
{
  SjDriveManager *self = SJ_DRIVE_MANAGER (initable);
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  priv->client = udisks_client_new_sync (cancellable, error);
  if (priv->client == NULL) {
    return FALSE;
  }

  GDBusObjectManager *manager = udisks_client_get_object_manager (priv->client);
  g_assert_nonnull (manager);

  g_autoptr (GList) objects = g_dbus_object_manager_get_objects (manager);
  for (GList *elem = objects; elem != NULL; elem = elem->next) {
    UDisksObject *obj = UDISKS_OBJECT (elem->data);
    g_autoptr (UDisksDrive) drive = udisks_object_get_drive (obj);
    if (drive && g_strv_contains (udisks_drive_get_media_compatibility (drive), "optical_cd")) {
      g_debug ("adding %s to drives", udisks_drive_get_id (drive));
      g_signal_connect (drive, "notify::media-available", G_CALLBACK (media_available_handler), self);
      g_hash_table_insert (priv->drives, udisks_drive_dup_id (drive), g_object_ref (drive));
    }
  }

  priv->object_added_signal_id = g_signal_connect (manager, "object-added", G_CALLBACK (object_added_handler), self);
  priv->object_removed_signal_id = g_signal_connect (manager, "object-removed", G_CALLBACK (object_removed_handler), self);

  return TRUE;
}

static void
sj_drive_manager_initable_interface_init (GInitableIface *iface)
{
  iface->init = sj_drive_manager_initable_init;
}

static void
sj_drive_manager_init (SjDriveManager *self)
{
  SjDriveManagerPrivate *priv = sj_drive_manager_get_instance_private (self);

  priv->object_added_signal_id = 0;
  priv->object_removed_signal_id = 0;
  priv->client = NULL;
  priv->drives = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}
