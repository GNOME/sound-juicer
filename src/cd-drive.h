/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   cd-drive.h: easy to use cd burner software

   Copyright (C) 2002-2004 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

  Authors: Alexander Larsson <alexl@redhat.com>
  Bastien Nocera <hadess@hadess.net>
*/

#ifndef CD_DRIVE_H
#define CD_DRIVE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	CD_MEDIA_TYPE_BUSY,
	CD_MEDIA_TYPE_ERROR,
	CD_MEDIA_TYPE_UNKNOWN,
	CD_MEDIA_TYPE_CD,
	CD_MEDIA_TYPE_CDR,
	CD_MEDIA_TYPE_CDRW,
	CD_MEDIA_TYPE_DVD,
	CD_MEDIA_TYPE_DVDR,
	CD_MEDIA_TYPE_DVDRW,
	CD_MEDIA_TYPE_DVD_RAM,
	CD_MEDIA_TYPE_DVD_PLUS_R,
	CD_MEDIA_TYPE_DVD_PLUS_RW,
} CDMediaType;

#define CD_MEDIA_SIZE_UNKNOWN -1
#define CD_MEDIA_SIZE_NA      -2
#define CD_MEDIA_SIZE_BUSY    -3

typedef enum {
	CDDRIVE_TYPE_FILE                   = 1 << 0,
	CDDRIVE_TYPE_CD_RECORDER            = 1 << 1,
	CDDRIVE_TYPE_CDRW_RECORDER          = 1 << 2,
	CDDRIVE_TYPE_DVD_RAM_RECORDER       = 1 << 3,
	/* Drives are usually DVD-R and DVD-RW */
	CDDRIVE_TYPE_DVD_RW_RECORDER        = 1 << 4,
	CDDRIVE_TYPE_DVD_PLUS_R_RECORDER    = 1 << 5,
	CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER   = 1 << 6,
	CDDRIVE_TYPE_CD_DRIVE               = 1 << 7,
	CDDRIVE_TYPE_DVD_DRIVE              = 1 << 8,
} CDDriveType;

typedef struct CDDrivePriv CDDrivePriv;

typedef struct {
	CDDriveType type;
	char *display_name;
	int max_speed_write;
	int max_speed_read;
	char *cdrecord_id;
	char *device;
	CDDrivePriv *priv;
} CDDrive;

#define SIZE_TO_TIME(size) (size > 0 ? (int) (((size / 1024 / 1024) - 1) * 48 / 7): 0)

/* Returns a list of CDDrive structs */
GList *scan_for_cdroms (gboolean recorder_only, gboolean add_image);
void cd_drive_free (CDDrive *drive);
CDDrive *cd_drive_copy (CDDrive *drive);
CDMediaType cd_drive_get_media_type (CDDrive *drive);
CDMediaType cd_drive_get_media_type_and_rewritable (CDDrive *drive, gboolean *is_rewritable);
CDMediaType cd_drive_get_media_type_from_path (const char *device_path);
gint64 cd_drive_get_media_size (CDDrive *drive);
gint64 cd_drive_get_media_size_from_path (const char *device_path);
CDDrive *cd_drive_get_file_image (void);
gboolean cd_drive_lock (CDDrive *drive, const char *reason,
			char **reason_for_failure);
gboolean cd_drive_unlock (CDDrive *drive);

G_END_DECLS

#endif
