/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   cd-drive.c: easy to use cd burner software

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

#ifdef USE_HAL
#include <libhal.h>
#endif /* USE_HAL */

#ifdef __linux__
#include <linux/cdrom.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#endif /* __linux__ */

#ifdef __FreeBSD__
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <camlib.h>
#endif /* __FreeBSD__ */

#include "cd-drive.h"

#define CD_ROM_SPEED 176

static struct {
	const char *name;
	gboolean can_write_cdr;
	gboolean can_write_cdrw;
	gboolean can_write_dvdr;
	gboolean can_write_dvdram;
} recorder_whitelist[] = {
	{ "IOMEGA - CDRW9602EXT-B", TRUE, TRUE, FALSE, FALSE },
	{ "SONY - CD-R   CDU948S",  TRUE, FALSE, FALSE, FALSE },
};

typedef enum {
	CD_PROTOCOL_IDE,
	CD_PROTOCOL_SCSI,
} CDProtocolType;

struct CDDrivePriv {
	CDProtocolType protocol;
	char *udi;
};

static CDDrive *cd_drive_new (void);

#ifdef USE_HAL
static LibHalContext *
get_hal_context (void)
{
	static LibHalContext *ctx = NULL;
	LibHalFunctions hal_functions = {
		NULL, /* mainloop integration */
		NULL, /* device_added */
		NULL, /* device_removed */
		NULL, /* device_new_capability */
		NULL, /* property_modified */
		NULL, /* device_condition */
	};
	
	if (ctx == NULL)
		ctx = hal_initialize (&hal_functions, FALSE);

	return ctx;
}
#endif /* USE_HAL */


/* Utility functions, be careful to have a match with what's use in the
 * different bits of code */
#if !defined (__linux__)
static char *
cdrecord_get_stdout_for_id (char *id)
{
	int max_speed, i;
	const char *argv[20]; /* Shouldn't need more than 20 arguments */
	char *dev_str, *stdout_data;

	max_speed = -1;

	i = 0;
	argv[i++] = "cdrecord";
	argv[i++] = "-prcap";
	dev_str = g_strdup_printf ("dev=%s", id);
	argv[i++] = dev_str;
	argv[i++] = NULL;

	if (g_spawn_sync (NULL,
				(char **)argv,
				NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				NULL, NULL,
				&stdout_data,
				NULL,
				NULL,
				NULL)) {
		g_free (dev_str);
		return stdout_data;
	}

	g_free (dev_str);
	return NULL;
}

static void
get_cd_properties (char *device, char *id, int *max_rd_speed, int *max_wr_speed,
	CDDriveType *type)
{
	char *stdout_data, *rd_speed, *wr_speed, *drive_cap;

	*max_rd_speed = -1;
	*max_wr_speed = -1;
	*type = 0;

	*max_rd_speed = get_device_max_read_speed (device);
	*max_wr_speed = get_device_max_write_speed (device);

	stdout_data = cdrecord_get_stdout_for_id (id);
	if (stdout_data == NULL) {
		return;
	}
	drive_cap = strstr (stdout_data, "Does write DVD-RAM media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_DVD_RAM_RECORDER;
	}
	drive_cap = strstr (stdout_data, "Does read DVD-R media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_DVD_RW_RECORDER;
	}
	drive_cap = strstr (stdout_data, "Does read DVD-ROM media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_DVD_DRIVE;
	}
	drive_cap = strstr (stdout_data, "Does write CD-RW media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_CDRW_RECORDER;
	}
	drive_cap = strstr (stdout_data, "Does write CD-R media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_CD_RECORDER;
	}
	drive_cap = strstr (stdout_data, "Does read CD-R media");
	if (drive_cap != NULL) {
		*type |= CDDRIVE_TYPE_CD_DRIVE;
	}
	g_free (stdout_data);
}
#endif /* !__linux__ */

#if !(defined (__linux__)) || (defined (__linux__) && (defined (USE_HAL)))
static void
add_whitelist (CDDrive *cdrom)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (recorder_whitelist); i++) {
		if (!strcmp (cdrom->display_name, recorder_whitelist[i].name)) {
			if (recorder_whitelist[i].can_write_cdr) {
				cdrom->type |= CDDRIVE_TYPE_CD_RECORDER;
			}
			if (recorder_whitelist[i].can_write_cdrw) {
				cdrom->type |= CDDRIVE_TYPE_CDRW_RECORDER;
			}
			if (recorder_whitelist[i].can_write_dvdr) {
				cdrom->type |= CDDRIVE_TYPE_DVD_RW_RECORDER;
			}
			if (recorder_whitelist[i].can_write_dvdram) {
				cdrom->type |= CDDRIVE_TYPE_DVD_RAM_RECORDER;
			}
		}
	}
}
#endif /* !__linux__ */

/* For dvd_plus_rw_utils.cpp */
int get_dvd_r_rw_profile (const char *name);
int get_mmc_profile (int fd);
int get_disc_size_cd (int fd);
gint64 get_disc_size_dvd (int fd, int mmc_profile);
int get_read_write_speed (int fd, int *read_speed, int *write_speed);
int get_disc_status (int fd, int *empty, int *is_rewritable);


static void
add_dvd_plus (CDDrive *cdrom)
{
	int caps;

	caps = get_dvd_r_rw_profile (cdrom->device);

	if (caps == -1) {
		return;
	}

	if (caps == 2) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER;
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_R_RECORDER;
	} else if (caps == 0) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_R_RECORDER;
	} else if (caps == 1) {
		cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER;
	}
}

static gboolean
cd_drive_door_open (gboolean mmc_profile, int fd)
{
#ifdef __linux__
	{
		int status;

		status = ioctl (fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
		if (status < 0) {
			return FALSE;
		}

		return status == CDS_TRAY_OPEN;
	}
#else
	if (mmc_profile == 0) {
		return TRUE;
	}

	return FALSE;
#endif
}

static CDMediaType
cd_drive_get_media_type_from_path_full (const char *device, gboolean *is_rewritable)
{
	int fd;
	int mmc_profile;

	g_return_val_if_fail (device != NULL, CD_MEDIA_TYPE_ERROR);

	fd = open (device, O_RDWR|O_EXCL|O_NONBLOCK);
	if (fd < 0) {
		if (errno == EBUSY) {
			return CD_MEDIA_TYPE_BUSY;
		}
		return CD_MEDIA_TYPE_ERROR;
	}

	mmc_profile = get_mmc_profile (fd);

	/* Couldn't get the data about the media */
	if (mmc_profile < 0) {
		gboolean opened;

		opened = cd_drive_door_open (mmc_profile, fd);

		if (opened != FALSE) {
			close (fd);
			return CD_MEDIA_TYPE_ERROR;
		} else {
			int rewrite, empty;
			if (get_disc_status (fd, &empty, &rewrite) == 0) {
				close (fd);
				*is_rewritable = rewrite;
				if (empty)
					return CD_MEDIA_TYPE_ERROR;
				else
					return CD_MEDIA_TYPE_UNKNOWN;
			}
			close (fd);
			return CD_MEDIA_TYPE_UNKNOWN;
		}
	}

	close (fd);

	switch (mmc_profile) {
	case -1:
		g_assert_not_reached ();
	case 0:		/* No Media */
		return CD_MEDIA_TYPE_ERROR;
	case 0x8:	/* Commercial CDs and Audio CD	*/
		return CD_MEDIA_TYPE_CD;
	case 0x9:	/* CD-R                         */
		return CD_MEDIA_TYPE_CDR;
	case 0xa:	/* CD-RW			*/
		*is_rewritable = TRUE;
		return CD_MEDIA_TYPE_CDRW;
	case 0x10:	/* Commercial DVDs		*/
		return CD_MEDIA_TYPE_DVD;
	case 0x11:      /* DVD-R                        */
		return CD_MEDIA_TYPE_DVDR;
	case 0x13:      /* DVD-RW Restricted Overwrite  */
	case 0x14:      /* DVD-RW Sequential            */
		*is_rewritable = TRUE;
		return CD_MEDIA_TYPE_DVDRW;
	case 0x1B:      /* DVD+R                        */
		return CD_MEDIA_TYPE_DVD_PLUS_R;
	case 0x1A:      /* DVD+RW                       */
		*is_rewritable = TRUE;
		return CD_MEDIA_TYPE_DVD_PLUS_RW;
	case 0x12:      /* DVD-RAM                      */
		return CD_MEDIA_TYPE_DVD_RAM;
	default:
		return CD_MEDIA_TYPE_UNKNOWN;
	}
}


CDMediaType
cd_drive_get_media_type_from_path (const char *device)
{
	gboolean is_rewritable;
	
	return cd_drive_get_media_type_from_path_full (device, &is_rewritable);
}


CDMediaType
cd_drive_get_media_type (CDDrive *cdrom)
{
	gboolean is_rewritable;
	
	return cd_drive_get_media_type_and_rewritable (cdrom, &is_rewritable);
}

CDMediaType
cd_drive_get_media_type_and_rewritable (CDDrive *cdrom, gboolean *is_rewritable)
{
	g_return_val_if_fail (cdrom != NULL, CD_MEDIA_TYPE_ERROR);

	*is_rewritable = FALSE;

#ifdef USE_HAL
	if (cdrom->priv != NULL && cdrom->priv->udi != NULL) {
		LibHalContext *ctx;
		char** device_names;
		int num_devices;
		CDMediaType type;
		char *hal_type;
		
		ctx = get_hal_context ();
		if (ctx != NULL) {
			device_names = hal_manager_find_device_string_match (ctx, 
									     "info.parent",
									     cdrom->priv->udi,
									     &num_devices);
			if (num_devices == 0) {
				return CD_MEDIA_TYPE_ERROR;
			}

			/* just look at the first child */
			if (hal_device_get_property_bool (ctx, 
							  device_names[0],
							  "volume.is_mounted")) {
				type = CD_MEDIA_TYPE_BUSY;
			} else {		    
				*is_rewritable = hal_device_get_property_bool (ctx, 
									       device_names[0],
									       "volume.disc.is_rewritable");
				type = CD_MEDIA_TYPE_BUSY;
				hal_type = hal_device_get_property_string (ctx, 
									   device_names[0],
									   "volume.disc.type");
				if (hal_type == NULL || strcmp (hal_type, "unknown") == 0) {
					type = CD_MEDIA_TYPE_UNKNOWN;
				} else if (strcmp (hal_type, "cd_rom") == 0) {
					type = CD_MEDIA_TYPE_CD;
				} else if (strcmp (hal_type, "cd_r") == 0) {
					type = CD_MEDIA_TYPE_CDR;
				} else if (strcmp (hal_type, "cd_rw") == 0) {
					type = CD_MEDIA_TYPE_CDRW;
				} else if (strcmp (hal_type, "dvd_rom") == 0) {
					type = CD_MEDIA_TYPE_DVD;
				} else if (strcmp (hal_type, "dvd_r") == 0) {
					type = CD_MEDIA_TYPE_DVDR;
				} else if (strcmp (hal_type, "dvd_ram") == 0) {
					type = CD_MEDIA_TYPE_DVD_RAM;
				} else if (strcmp (hal_type, "dvd_rw") == 0) {
					type = CD_MEDIA_TYPE_DVDRW;
				} else if (strcmp (hal_type, "dvd_plus_rw") == 0) {
					type = CD_MEDIA_TYPE_DVD_PLUS_RW;
				} else if (strcmp (hal_type, "dvd_plus_r") == 0) {
					type = CD_MEDIA_TYPE_DVD_PLUS_R;
				} else {
					type = CD_MEDIA_TYPE_UNKNOWN;
				}
				
				if (hal_type != NULL)
					hal_free_string (hal_type);
			}

			hal_free_string_array (device_names);

			return type;
		}
	}
#endif

	return cd_drive_get_media_type_from_path_full (cdrom->device, is_rewritable);
}

gint64
cd_drive_get_media_size_from_path (const char *device)
{
	int fd;
	int secs;
	int mmc_profile;
	gint64 size;

	g_return_val_if_fail (device != NULL, CD_MEDIA_SIZE_UNKNOWN);

	secs = 0;

	fd = open (device, O_RDWR|O_EXCL|O_NONBLOCK);
	if (fd < 0) {
		if (errno == EBUSY) {
			return CD_MEDIA_SIZE_BUSY;
		}
		return CD_MEDIA_SIZE_UNKNOWN;
	}

	mmc_profile = get_mmc_profile (fd);

	/* See cd_drive_get_media_type_from_path for details */
	switch (mmc_profile) {
	case 0x9:
	case 0xa:
		secs = get_disc_size_cd (fd);
		size = (1 + secs * 7 / 48) * 1024 * 1024;
		break;
	case 0x11:
	case 0x13:
	case 0x14:
	case 0x1B:
	case 0x1A:
	case 0x12:
		size = get_disc_size_dvd (fd, mmc_profile);
		break;
	default:
		size = CD_MEDIA_SIZE_NA;
	}

	close (fd);

	return size;
}

gint64
cd_drive_get_media_size (CDDrive *cdrom)
{
	g_return_val_if_fail (cdrom != NULL, CD_MEDIA_SIZE_UNKNOWN);

	return cd_drive_get_media_size_from_path (cdrom->device);
}

#ifdef USE_HAL

#define GET_BOOL_PROP(x) (hal_device_property_exists (ctx, device_names[i], x) && hal_device_get_property_bool (ctx, device_names[i], x))

static GList *
hal_scan (gboolean recorder_only)
{
	GList *cdroms = NULL;
	int i;
	int num_devices;
	char** device_names;
	LibHalContext *ctx;

	ctx = get_hal_context ();
	if (ctx == NULL) {
		return NULL;
	}

	device_names = hal_find_device_by_capability (ctx,
			"storage.cdrom", &num_devices);

	if (device_names == NULL)
	{
		return NULL;
	}

	for (i = 0; i < num_devices; i++)
	{
		CDDrive *cdrom;
		char *string;
		gboolean is_cdr;

		/* Is it a CD burner? */
		is_cdr = GET_BOOL_PROP ("storage.cdrom.cdr");

		cdrom = cd_drive_new ();
		cdrom->type = CDDRIVE_TYPE_CD_DRIVE;
		if (is_cdr != FALSE) {
			cdrom->type |= CDDRIVE_TYPE_CD_RECORDER;
		}
		if (GET_BOOL_PROP ("storage.cdrom.cdrw")) {
			cdrom->type |= CDDRIVE_TYPE_CDRW_RECORDER;
		}
		if (GET_BOOL_PROP ("storage.cdrom.dvd")) {
			cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;

			if (GET_BOOL_PROP ("storage.cdrom.dvdram")) {
				cdrom->type |= CDDRIVE_TYPE_DVD_RAM_RECORDER;
			}
			if (GET_BOOL_PROP ("storage.cdrom.dvdr")) {
				cdrom->type |= CDDRIVE_TYPE_DVD_RW_RECORDER;
			}
			if (GET_BOOL_PROP ("storage.cdrom.dvd")) {
				cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;
			}
			if (GET_BOOL_PROP ("storage.cdrom.dvdplusr")) {
				cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_R_RECORDER;
			}
			if (GET_BOOL_PROP ("storage.cdrom.dvdplusrw")) {
				cdrom->type |= CDDRIVE_TYPE_DVD_PLUS_RW_RECORDER;
			}
		}

		cdrom->device = hal_device_get_property_string (ctx,
				device_names[i], "block.device");
		cdrom->cdrecord_id = g_strdup (cdrom->device);

		string = hal_device_get_property_string (ctx,
				device_names[i], "storage.model");
		if (string != NULL) {
			cdrom->display_name = string;
		} else {
			cdrom->display_name = g_strdup_printf ("Unnamed Drive (%s)", cdrom->device);
		}

		cdrom->max_speed_read = hal_device_get_property_int
			(ctx, device_names[i], "storage.cdrom.read_speed")
			/ CD_ROM_SPEED;

		if (hal_device_property_exists (ctx, device_names[i], "storage.cdrom.write_speed")) {
			cdrom->max_speed_write = hal_device_get_property_int
				(ctx, device_names[i],
				 "storage.cdrom.write_speed")
				/ CD_ROM_SPEED;
		}

		add_whitelist (cdrom);

		if (recorder_only && !(cdrom->type & CDDRIVE_TYPE_CD_RECORDER)) {
			cd_drive_free (cdrom);
		} else {
			cdroms = g_list_prepend (cdroms, cdrom);
		}

		cdrom->priv->udi = g_strdup (device_names[i]);
	}

	hal_free_string_array (device_names);

	cdroms = g_list_reverse (cdroms);

	return cdroms;
}
#endif /* USE_HAL */

#if defined(__linux__) || defined(__FreeBSD__)



#endif /* __linux__ || __FreeBSD__ */

#if defined (__linux__)

static char **
read_lines (char *filename)
{
	char *contents;
	gsize len;
	char *p, *n;
	GPtrArray *array;
	
	if (g_file_get_contents (filename,
				 &contents,
				 &len, NULL)) {
		
		array = g_ptr_array_new ();
		
		p = contents;
		while ((n = memchr (p, '\n', len - (p - contents))) != NULL) {
			*n = 0;
			g_ptr_array_add (array, g_strdup (p));
			p = n + 1;
		}
		if ((gsize)(p - contents) < len) {
			g_ptr_array_add (array, g_strndup (p, len - (p - contents)));
		}

		g_ptr_array_add (array, NULL);
		
		g_free (contents);
		return (char **)g_ptr_array_free (array, FALSE);
	}
	return NULL;
}

struct scsi_unit {
	char *vendor;
	char *model;
	char *rev;
	int bus;
	int id;
	int lun;
	int type;
};

struct cdrom_unit {
	CDProtocolType protocol;
	char *device;
	char *display_name;
	int speed;
	gboolean can_write_cdr;
	gboolean can_write_cdrw;
	gboolean can_write_dvdr;
	gboolean can_write_dvdram;
	gboolean can_read_dvd;
};

static char *cdrom_get_name (struct cdrom_unit *cdrom, struct scsi_unit *scsi_units, int n_scsi_units);

static void
linux_add_whitelist (struct cdrom_unit *cdrom_s,
	       struct scsi_unit *scsi_units, int n_scsi_units)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (recorder_whitelist); i++) {
		if (cdrom_s->display_name == NULL) {
			continue;
		}

		if (!strcmp (cdrom_s->display_name, recorder_whitelist[i].name)) {
			cdrom_s->can_write_cdr =
				recorder_whitelist[i].can_write_cdr;
			cdrom_s->can_write_cdrw =
				recorder_whitelist[i].can_write_cdrw;
			cdrom_s->can_write_dvdr =
				recorder_whitelist[i].can_write_dvdr;
			cdrom_s->can_write_dvdram =
				recorder_whitelist[i].can_write_dvdram;
		}
	}
}

static void
get_scsi_units (char **device_str, char **devices, struct scsi_unit *scsi_units)
{
	char vendor[9], model[17], rev[5];
	int host_no, access_count, queue_depth, device_busy, online, channel;
	int scsi_id, scsi_lun, scsi_type;
	int i, j;

	j = 0;

	for (i = 0; device_str[i] != NULL && devices[i] != NULL; i++) {
		if (strcmp (device_str[i], "<no active device>") == 0) {
			continue;
		}
		if (sscanf (devices[i], "%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d",
			    &host_no,
			    &channel, &scsi_id, &scsi_lun, &scsi_type, &access_count, &queue_depth,
				&device_busy, &online) != 9) {
		
			g_warning ("Couldn't match line in /proc/scsi/sg/devices\n");
			continue;
		}
		if (scsi_type == 5) { /* TYPE_ROM (include/scsi/scsi.h) */
			if (sscanf (device_str[i], "%8c\t%16c\t%4c", 
						vendor, model, rev) != 3) {
				g_warning ("Couldn't match line /proc/scsi/sg/device_strs\n");
				continue;
			}
			vendor[8] = '\0'; model[16] = '\0'; rev[4] = '\0';

			scsi_units[j].vendor = g_strdup (g_strstrip (vendor));
			scsi_units[j].model = g_strdup (g_strstrip (model));
			scsi_units[j].rev = g_strdup (g_strstrip (rev));
			scsi_units[j].bus = host_no;
			scsi_units[j].id = scsi_id;
			scsi_units[j].lun = scsi_lun; 
			scsi_units[j].type = scsi_type;
			
			j++;
		}
	}
}

static int
count_strings (char *p)
{
	int n_strings;

	n_strings = 0;
	while (*p != 0) {
		n_strings++;
		while (*p != '\t' && *p != 0) {
			p++;
		}
		if (*p == '\t') {
			p++;
		}
	}
	return n_strings;
}

static int
get_cd_scsi_id (const char *dev, int *bus, int *id, int *lun)
{
	int fd;
	char *devfile;
	struct {
		long mux4;
		long hostUniqueId;
	} m_idlun;
	
	devfile = g_strdup_printf ("/dev/%s", dev);
	fd = open(devfile, O_RDWR | O_NONBLOCK);
	g_free (devfile);

	/* Avoid problems with Valgrind */
	memset (&m_idlun, 1, sizeof (m_idlun));
	*bus = *id = *lun = -1;

	if (fd < 0) {
		g_warning ("Failed to open cd device %s\n", dev);
		return 0;
	}

	if (ioctl (fd, SCSI_IOCTL_GET_BUS_NUMBER, bus) < 0 || *bus < 0) {
		g_warning ("Failed to get scsi bus nr\n");
		close (fd);
		return 0;
	}

	if (ioctl (fd, SCSI_IOCTL_GET_IDLUN, &m_idlun) < 0) {
		g_warning ("Failed to get scsi id and lun\n");
		close(fd);
		return 0;
	}
	*id = m_idlun.mux4 & 0xFF;
	*lun = (m_idlun.mux4 >> 8)  & 0xFF;

	close(fd);
	return 1;
}

static struct scsi_unit *
lookup_scsi_unit (int bus, int id, int lun,
		  struct scsi_unit *scsi_units, int n_scsi_units)
{
	int i;

	for (i = 0; i < n_scsi_units; i++) {
		if (scsi_units[i].bus == bus &&
		    scsi_units[i].id == id &&
		    scsi_units[i].lun == lun) {
			return &scsi_units[i];
		}
	}
	return NULL;
}

#if !defined(__linux)
static int
get_device_max_read_speed (char *device)
{
	int fd;
	int max_speed;
	int read_speed, write_speed;

	max_speed = -1;

	fd = open (device, O_RDWR|O_EXCL|O_NONBLOCK);
	if (fd < 0) {
		return -1;
	}

	get_read_write_speed (fd, &read_speed, &write_speed);
	close (fd);
	max_speed = (int)floor  (read_speed) / CD_ROM_SPEED;

	return max_speed;
}
#endif

static int
get_device_max_write_speed (char *device)
{
	int fd;
	int max_speed;
	int read_speed, write_speed;

	max_speed = -1;

	fd = open (device, O_RDWR|O_EXCL|O_NONBLOCK);
	if (fd < 0) {
		return -1;
	}

	get_read_write_speed (fd, &read_speed, &write_speed);
	close (fd);
	max_speed = (int)floor  (write_speed) / CD_ROM_SPEED;

	return max_speed;
}

static char *
get_scsi_cd_name (int bus, int id, int lun, const char *dev,
		  struct scsi_unit *scsi_units, int n_scsi_units)
{
	struct scsi_unit *scsi_unit;

	scsi_unit = lookup_scsi_unit (bus, id, lun, scsi_units, n_scsi_units);
	if (scsi_unit == NULL) {
		return g_strdup_printf (_("Unnamed SCSI Drive (%s)"), dev);
	}

	return g_strdup_printf ("%s - %s",
				scsi_unit->vendor,
				scsi_unit->model);
}

static char *
cdrom_get_name (struct cdrom_unit *cdrom, struct scsi_unit *scsi_units, int n_scsi_units)
{
	char *filename, *line, *retval;
	char stdname[4], devfsname[15];
	int bus, id, lun, i;

	g_return_val_if_fail (cdrom != NULL, FALSE);

	/* clean up the string again if we have devfs */
	i = sscanf(cdrom->device, "%4s %14s", stdname, devfsname);
	if (i < 1) { /* should never happen */
		g_warning("cdrom_get_name: cdrom->device string broken!");
		return NULL;
	}
	if (i == 2) {
		g_free (cdrom->device);
		cdrom->device = g_strdup(devfsname);
	}
	stdname[3] = '\0'; devfsname[14] = '\0'; /* just in case */
	
	if (cdrom->protocol == CD_PROTOCOL_SCSI) {
		get_cd_scsi_id (cdrom->device, &bus, &id, &lun);
		retval = get_scsi_cd_name (bus, id, lun, cdrom->device, scsi_units,
			   n_scsi_units);
	} else {
		filename = g_strdup_printf ("/proc/ide/%s/model", stdname);
		if (!g_file_get_contents (filename, &line, NULL, NULL) ||
		    line == NULL) {
			g_free (filename);
			return NULL;
		}
		g_free (filename);

		i = strlen (line);
		if (line[i-1] != '\n') {
			retval = g_strdup (line);
		} else {
			retval = g_strndup (line, i - 1);
		}

		g_free (line);
	}

	return retval;
}

static GList *
add_linux_cd_recorder (GList *cdroms,
		       gboolean recorder_only,
		       struct cdrom_unit *cdrom_s,
		       struct scsi_unit *scsi_units,
		       int n_scsi_units)
{
	int bus, id, lun;
	CDDrive *cdrom;

	cdrom = cd_drive_new ();

	cdrom->type = CDDRIVE_TYPE_CD_DRIVE;
	cdrom->display_name = g_strdup (cdrom_s->display_name);

	if (cdrom_s->protocol == CD_PROTOCOL_SCSI) {
		cdrom->priv->protocol = CD_PROTOCOL_SCSI;
		if (!get_cd_scsi_id (cdrom_s->device, &bus, &id, &lun)) {
			g_free (cdrom->display_name);
			g_free (cdrom);
			return cdroms;
		}
		cdrom->cdrecord_id = g_strdup_printf ("%d,%d,%d",
				bus, id, lun);
	} else {
		cdrom->priv->protocol = CD_PROTOCOL_IDE;
		/* kernel >=2.5 can write cd w/o ide-scsi */
		cdrom->cdrecord_id = g_strdup_printf ("/dev/%s",
				cdrom_s->device);
	}

	if (recorder_only) {
		cdrom->max_speed_write = get_device_max_write_speed
			(cdrom->device);
		if (cdrom->max_speed_write == -1) {
			cdrom->max_speed_write = cdrom_s->speed;
		}
	} else {
		/* Have a wild guess, the drive should actually correct us */
		cdrom->max_speed_write = cdrom_s->speed;
	}

	cdrom->device = g_strdup_printf ("/dev/%s", cdrom_s->device);
	cdrom->max_speed_read = cdrom_s->speed;
	if (cdrom_s->can_write_dvdr) {
		cdrom->type |= CDDRIVE_TYPE_DVD_RW_RECORDER;
	}

	if (cdrom_s->can_write_dvdram) {
		cdrom->type |= CDDRIVE_TYPE_DVD_RAM_RECORDER;
	}

	if (cdrom_s->can_write_cdr) {
		cdrom->type |= CDDRIVE_TYPE_CD_RECORDER;
	}
	if (cdrom_s->can_write_cdrw) {
		cdrom->type |= CDDRIVE_TYPE_CDRW_RECORDER;
	}
	if (cdrom_s->can_read_dvd) {
		cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;
		add_dvd_plus (cdrom);
	}

	return g_list_append (cdroms, cdrom);
}

static GList *
add_linux_cd_drive (GList *cdroms, struct cdrom_unit *cdrom_s,
		    struct scsi_unit *scsi_units, int n_scsi_units)
{
	CDDrive *cdrom;

	cdrom = cd_drive_new ();
	cdrom->type = CDDRIVE_TYPE_CD_DRIVE;
	cdrom->cdrecord_id = NULL;
	cdrom->display_name = g_strdup (cdrom_s->display_name);
	cdrom->device = g_strdup_printf ("/dev/%s", cdrom_s->device);
	cdrom->max_speed_write = 0; /* Can't write */
	cdrom->max_speed_read = cdrom_s->speed;
	if (cdrom_s->can_read_dvd) {
		cdrom->type |= CDDRIVE_TYPE_DVD_DRIVE;
	}

	return g_list_append (cdroms, cdrom);
}

static char *
get_cd_device_file (const char *str)
{
	char *devname;
	
	if (str[0] == 's') {
		devname = g_strdup_printf ("/dev/scd%c", str[2]);
		if (g_file_test (devname, G_FILE_TEST_EXISTS)) {
			g_free (devname);
			return g_strdup_printf ("scd%c", str[2]);
		}
		g_free (devname);
	}
	return 	g_strdup (str);
}

static GList *
linux_scan (gboolean recorder_only)
{
	char **device_str, **devices;
	char **cdrom_info;
	struct scsi_unit *scsi_units;
	struct cdrom_unit *cdroms;
	char *p, *t;
	int n_cdroms, maj, min, i, j;
	int n_scsi_units;
	int fd;
	FILE *file;
	GList *cdroms_list;
	gboolean have_devfs;

	/* devfs creates and populates the /dev/cdroms directory when its mounted
	 * the 'old style names' are matched with devfs names below.
	 * The cdroms.device string gets cleaned up again in cdrom_get_name()
	 * we need the oldstyle name to get device->display_name for ide.
	 */
	have_devfs = FALSE;
	if (g_file_test ("/dev/.devfsd", G_FILE_TEST_EXISTS)) {
		have_devfs = TRUE;
	}
	
	cdrom_info = read_lines ("/proc/sys/dev/cdrom/info");
	if (cdrom_info == NULL || cdrom_info[0] == NULL || cdrom_info[1] == NULL) {
		g_warning ("Couldn't read /proc/sys/dev/cdrom/info");
		return NULL;
	}
	if (!g_str_has_prefix (cdrom_info[2], "drive name:\t")) {
		return NULL;
	}
	p = cdrom_info[2] + strlen ("drive name:\t");
	while (*p == '\t') {
		p++;
	}
	n_cdroms = count_strings (p);
	cdroms = g_new0 (struct cdrom_unit, n_cdroms);

	for (j = 0; j < n_cdroms; j++) {
		t = strchr (p, '\t');
		if (t != NULL) {
			*t = 0;
		}
		cdroms[j].device = get_cd_device_file (p);
		/* Assume its an IDE device for now */
		cdroms[j].protocol = CD_PROTOCOL_IDE;
		if (t != NULL) {
			p = t + 1;
		}
	}

	/* we only have to check the first char, since only ide or scsi 	
	 * devices are listed in /proc/sys/dev/cdrom/info. It will always
	 * be 'h' or 's'
	 */
	n_scsi_units = 0;
	for (i = 0; i < n_cdroms; i++) {
		if (cdroms[i].device[0] == 's') {
			cdroms[i].protocol = CD_PROTOCOL_SCSI;
			n_scsi_units++;
		}
	}

	if (n_scsi_units > 0) {
		/* open /dev/sg0 to force loading of the sg module if not loaded yet */
		fd = open ("/dev/sg0", O_RDWR);
		if (fd >= 0) {
			close (fd);
		}
		
		devices = read_lines ("/proc/scsi/sg/devices");
		device_str = read_lines ("/proc/scsi/sg/device_strs");
		if (device_str == NULL) {
			g_warning ("Can't read /proc/scsi/sg/device_strs");
			g_strfreev (devices);
			return NULL;
		}

		scsi_units = g_new0 (struct scsi_unit, n_scsi_units);
		get_scsi_units (device_str, devices, scsi_units);

		g_strfreev (device_str);
		g_strfreev (devices);
	} else {
		scsi_units = NULL;
	}

	for (i = 3; cdrom_info[i] != NULL; i++) {
		if (g_str_has_prefix (cdrom_info[i], "Can write CD-R:")) {
			p = cdrom_info[i] + strlen ("Can write CD-R:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_cdr = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write CD-RW:")) {
			p = cdrom_info[i] + strlen ("Can write CD-RW:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_cdrw = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write DVD-R:")) {
			p = cdrom_info[i] + strlen ("Can write DVD-R:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_dvdr = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can write DVD-RAM:")) {
			p = cdrom_info[i] + strlen ("Can write DVD-RAM:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_write_dvdram = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "Can read DVD:")) {
			p = cdrom_info[i] + strlen ("Can read DVD:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
				cdroms[j].can_read_dvd = *p++ == '1';

				/* Skip tab */
				p++;
			}
		}
		if (g_str_has_prefix (cdrom_info[i], "drive speed:")) {
			p = cdrom_info[i] + strlen ("drive speed:");
			while (*p == '\t') {
				p++;
			}
			for (j = 0; j < n_cdroms; j++) {
  				cdroms[j].speed = atoi (p);

				/* Skip tab */
				p++;
			}
		}
	}
	g_strfreev (cdrom_info);

	/* get kernel major.minor version */
	file = fopen("/proc/sys/kernel/osrelease", "r");
	if (file == NULL || fscanf(file, "%d.%d", &maj, &min) != 2) {
		g_warning("Could not get kernel version.");
		maj = min = 0;
	}
	fclose(file);

	cdroms_list = NULL;
	for (i = n_cdroms - 1, j = 0; i >= 0; i--, j++) {
		if (have_devfs) {
			char *s;
			s = g_strdup_printf("%s cdroms/cdrom%d",
					cdroms[i].device,  j);
			g_free (cdroms[i].device);
			cdroms[i].device = s;
		}
		cdroms[i].display_name = cdrom_get_name (&cdroms[i],
				scsi_units, n_scsi_units);
		linux_add_whitelist (&cdroms[i], scsi_units, n_scsi_units);

		if ((cdroms[i].can_write_cdr ||
		    cdroms[i].can_write_cdrw ||
		    cdroms[i].can_write_dvdr ||
		    cdroms[i].can_write_dvdram) &&
			(cdroms[i].protocol == CD_PROTOCOL_SCSI ||
			(maj > 2) || (maj == 2 && min >= 5))) {
			cdroms_list = add_linux_cd_recorder (cdroms_list,
					recorder_only, &cdroms[i],
					scsi_units, n_scsi_units);
		} else if (!recorder_only) {
			cdroms_list = add_linux_cd_drive (cdroms_list,
					&cdroms[i], scsi_units, n_scsi_units);
		}
	}

	for (i = n_cdroms - 1; i >= 0; i--) {
		g_free (cdroms[i].display_name);
		g_free (cdroms[i].device);
	}
	g_free (cdroms);

	for (i = n_scsi_units - 1; i >= 0; i--) {
		g_free (scsi_units[i].vendor);
		g_free (scsi_units[i].model);
		g_free (scsi_units[i].rev);
	}
	g_free (scsi_units);

	return cdroms_list;
}

#elif defined (__FreeBSD__)

static GList *
freebsd_scan (gboolean recorder_only)
{
	GList *cdroms_list = NULL;
	const char *dev_type = "cd";
	int speed = 16; /* XXX Hardcode the write speed for now. */
	int i = 0;
	int cnode = 1; /* Use the CD device's 'c' node. */

	while (1) {
		CDDrive *cdrom;
		gchar *cam_path;
		struct cam_device *cam_dev;

		cam_path = g_strdup_printf ("/dev/%s%dc", dev_type, i);

		if (!g_file_test (cam_path, G_FILE_TEST_EXISTS)) {
			g_free (cam_path);
			cam_path = g_strdup_printf ("/dev/%s%d", dev_type, i);
			cnode = 0;
			if (!g_file_test (cam_path, G_FILE_TEST_EXISTS)) {
				g_free (cam_path);
				break;
			}
		}

		if ((cam_dev = cam_open_spec_device (dev_type, i, O_RDWR, NULL)) == NULL) {
			i++;
			g_free (cam_path);
			continue;
		}

		cdrom = cd_drive_new ();
		cdrom->display_name = g_strdup_printf ("%s %s", cam_dev->inq_data.vendor, cam_dev->inq_data.revision);
		cdrom->device = g_strdup (cam_path);
		cdrom->cdrecord_id = g_strdup_printf ("%d,%d,%d", cam_dev->path_id, cam_dev->target_id, cam_dev->target_lun);
		/* Attempt to get more specific information from
		 * this drive by using cdrecord.
		 */
		get_cd_properties (cdrom->device, cdrom->cdrecord_id,
			&(cdrom->max_speed_read),
			&(cdrom->max_speed_write),
			&(cdrom->type));
		if (cdrom->type & CDDRIVE_TYPE_CD_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_CDRW_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_DVD_RAM_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_DVD_RW_RECORDER
				|| !recorder_only) {

			if (cdrom->max_speed_read == -1) {
		    		cdrom->max_speed_read = speed;
			}
			if (cdrom->max_speed_write == -1) {
			    	cdrom->max_speed_write = speed;
			}

			if (cdrom->type & CDDRIVE_TYPE_DVD_DRIVE) {
				add_dvd_plus (cdrom);
			}

			cdroms_list = g_list_append (cdroms_list, cdrom);
		} else {
		    	cd_drive_free (cdrom);
		}

		g_free (cam_path);
		free (cam_dev);

		i++;
	}

	return cdroms_list;
}

#else



static char *
cdrecord_scan_get_stdout (void)
{
	int max_speed, i;
	const char *argv[20]; /* Shouldn't need more than 20 arguments */
	char *stdout_data;

	max_speed = -1;

	i = 0;
	argv[i++] = "cdrecord";
	argv[i++] = "-scanbus";
	argv[i++] = NULL;

	if (g_spawn_sync (NULL,
				(char **)argv,
				NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
				NULL, NULL,
				&stdout_data,
				NULL,
				NULL,
				NULL)) {
		return stdout_data;
	}

	return NULL;
}

#define DEFAULT_SPEED 2

static GList *
cdrecord_scan (gboolean recorder_only)
{
	GList *cdroms_list;
	CDDrive *cdrom;
	char *stdout_data, **lines, vendor[9], model[17];
	int i, bus, id, lun, index;

	cdroms_list = NULL;

	stdout_data = cdrecord_scan_get_stdout ();
	if (stdout_data == NULL) {
		return cdroms_list;
	}

	lines = g_strsplit (stdout_data, "\n", 0);
	g_free (stdout_data);

	for (i = 0; lines[i] != NULL; i++) {
		if (sscanf (lines[i], "\t%d,%d,%d\t  %d) '%8c' '%16c'",
					&bus, &id, &lun, &index,
					vendor, model) != 6) {
			continue;
		}

		vendor[8] = '\0'; model[16] = '\0';

		cdrom = cd_drive_new ();
		cdrom->display_name = g_strdup_printf ("%s - %s",
				g_strstrip (vendor), g_strstrip (model));
		cdrom->cdrecord_id = g_strdup_printf ("%d,%d,%d", bus, id, lun);
		/* FIXME we don't have any way to guess the real device
		 * from the info we get from CDRecord */
		cdrom->device = g_strdup_printf ("/dev/pg%d", index);
		get_cd_properties (cdrom->device, cdrom->cdrecord_id,
			&(cdrom->max_speed_read),
			&(cdrom->max_speed_write),
			&(cdrom->type));
		add_whitelist (cdrom);
		if (cdrom->type & CDDRIVE_TYPE_CD_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_CDRW_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_DVD_RAM_RECORDER
				|| cdrom->type & CDDRIVE_TYPE_DVD_RW_RECORDER
				|| !recorder_only) {

			if (cdrom->max_speed_read == -1) {
		    		cdrom->max_speed_read = DEFAULT_SPEED;
			}
			if (cdrom->max_speed_write == -1) {
			    	cdrom->max_speed_write = DEFAULT_SPEED;
			}

			if (cdrom->type & CDDRIVE_TYPE_DVD_DRIVE) {
				add_dvd_plus (cdrom);
			}

			cdroms_list = g_list_append (cdroms_list, cdrom);
		} else {
		    	cd_drive_free (cdrom);
		}
	}

	g_strfreev (lines);

	return cdroms_list;
}

#endif

CDDrive *
cd_drive_get_file_image (void)
{
	CDDrive *cdrom;

	cdrom = cd_drive_new ();
	cdrom->display_name = g_strdup (_("File image"));
	cdrom->max_speed_read = 0;
	cdrom->max_speed_write = 0;
	cdrom->type = CDDRIVE_TYPE_FILE;

	return cdrom;
}

/* This is used for testing different configurations */
#if 0
static GList*
test_cdroms (void)
{
	GList *list = NULL;
	CDDrive *drive = g_new0 (CDDrive, 1);
	drive->type = CDDRIVE_TYPE_CD_DRIVE | CDDRIVE_TYPE_DVD_DRIVE;
	drive->display_name = g_strdup ("HL-DT-STDVD-ROM");
	drive->device = g_strdup ("/dev/hdc");
	list = g_list_append (list, drive);
	drive = g_new0 (CDDrive, 1);
	drive->type = CDDRIVE_TYPE_CD_DRIVE | CDDRIVE_TYPE_DVD_DRIVE | CDDRIVE_TYPE_CD_RECORDER;
	drive->display_name = g_strdup ("_NEC DVD_RW ND-2500A");
	drive->device = g_strdup ("/dev/hdd");
	list = g_list_append (list, drive);
	return list;
}
#endif

GList *
scan_for_cdroms (gboolean recorder_only, gboolean add_image)
{
	GList *cdroms = NULL;

#ifdef USE_HAL
	cdroms = hal_scan (recorder_only);
#endif

	if (cdroms == NULL) {
#if defined (__linux__)
		cdroms = linux_scan (recorder_only);
#elif defined (__FreeBSD__)
		cdroms = freebsd_scan (recorder_only);
#else
		cdroms = cdrecord_scan (recorder_only);
#endif
	}

	if (add_image) {
		CDDrive *cdrom;
		cdrom = cd_drive_get_file_image ();
		cdroms = g_list_append (cdroms, cdrom);
	}

	return cdroms;
}

void
cd_drive_free (CDDrive *drive)
{
	g_return_if_fail (drive != NULL);

	if (drive->priv) {
		g_free (drive->priv->udi);
		g_free (drive->priv);
	}

	g_free (drive->display_name);
	g_free (drive->cdrecord_id);
	g_free (drive->device);
	g_free (drive);
}

gboolean
cd_drive_lock (CDDrive *drive,
	       const char *reason,
	       char **reason_for_failure) 
{
	gboolean res;

	if (reason_for_failure != NULL)
		*reason_for_failure = NULL;

	res = TRUE;
#ifdef USE_HAL
	if (drive->priv->udi != NULL) {
		LibHalContext *ctx;
		char *dbus_reason;
		
		ctx = get_hal_context ();
		if (ctx != NULL) {
			res = hal_device_lock (ctx, 
					       drive->priv->udi,
					       reason,
					       &dbus_reason);
			if (dbus_reason != NULL && 
			    reason_for_failure != NULL)
				*reason_for_failure = g_strdup (dbus_reason);
			if (dbus_reason != NULL)
				dbus_free (dbus_reason);
		}
	}
#endif
	return res;
}


gboolean 
cd_drive_unlock (CDDrive *drive)
{
	gboolean res;

	res = TRUE;
#ifdef USE_HAL
	if (drive->priv->udi != NULL) {
		LibHalContext *ctx;

		ctx = get_hal_context ();
		if (ctx != NULL) {
			res = hal_device_unlock (ctx, 
						 drive->priv->udi);
		}
	}
#endif
	return res;
}


static CDDrive *
cd_drive_new (void)
{
	CDDrive *cdrom;

	cdrom = g_new0 (CDDrive, 1);
	cdrom->priv = g_new0 (CDDrivePriv, 1);
	return cdrom;
}

CDDrive *
cd_drive_copy (CDDrive *drive)
{
	CDDrive *cdrom;

	cdrom = cd_drive_new ();
	cdrom->type = drive->type;
	cdrom->display_name = g_strdup (drive->display_name);
	cdrom->max_speed_write = drive->max_speed_write;
	cdrom->max_speed_read = drive->max_speed_read;
	cdrom->cdrecord_id = g_strdup (drive->cdrecord_id);
	cdrom->device = g_strdup (drive->device);
	cdrom->priv->protocol = drive->priv->protocol;
	cdrom->priv->udi = g_strdup (drive->priv->udi);

	return cdrom;
}
