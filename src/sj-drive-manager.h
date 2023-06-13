/* sj-drive-manager.h
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

#pragma once

#include <glib-object.h>
#include <udisks/udisks.h>

G_BEGIN_DECLS

#define SJ_TYPE_DRIVE_MANAGER (sj_drive_manager_get_type())

G_DECLARE_FINAL_TYPE (SjDriveManager, sj_drive_manager, SJ, DRIVE_MANAGER, GObject)

SjDriveManager *sj_drive_manager_new_sync (GCancellable *cancellable, GError **error);

UDisksDrive *sj_drive_manager_get_drive_for_device (SjDriveManager *manager,
                                                    const gchar    *device);

UDisksBlock *sj_drive_manager_get_block_for_device (SjDriveManager *manager,
                                                    const gchar    *device);

UDisksBlock *sj_drive_manager_get_block_for_drive (SjDriveManager *manager,
                                                   UDisksDrive    *drive);

GList *sj_drive_manager_get_drives (SjDriveManager *manager);

gboolean sj_drive_manager_drive_has_media_compatibility (SjDriveManager *manager,
                                                          UDisksDrive    *drive,
                                                          const gchar    *media);

G_END_DECLS
