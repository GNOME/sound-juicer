/*
 * sj-metadata-cdtext.h
 * Copyright (C) 2005 Ross Burton <ross@burtonini.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SJ_METADATA_CDTEXT_H
#define SJ_METADATA_CDTEXT_H

#include <glib-object.h>
#include "sj-metadata.h"

G_BEGIN_DECLS

#define SJ_TYPE_METADATA_CDTEXT            (sj_metadata_cdtext_get_type ())
#define SJ_METADATA_CDTEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_METADATA_CDTEXT, SjMetadataCdtext))
#define SJ_METADATA_CDTEXT_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), SJ_TYPE_METADATA_CDTEXT, SjMetadataCdtextClass))
#define SJ_IS_METADATA_CDTEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SJ_TYPE_METADATA_CDTEXT))
#define SJ_IS_METADATA_CDTEXT_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), SJ_TYPE_METADATA_CDTEXT))
#define SJ_METADATA_CDTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SJ_TYPE_METADATA_CDTEXT, SjMetadataCdtextClass))

typedef struct _SjMetadataCdtext SjMetadataCdtext;
typedef struct _SjMetadataCdtextClass SjMetadataCdtextClass;
typedef struct SjMetadataCdtextPrivate SjMetadataCdtextPrivate;

struct _SjMetadataCdtext
{
  GObject parent;
  SjMetadataCdtextPrivate *priv;
};

struct _SjMetadataCdtextClass
{
  GObjectClass parent;
};

GType sj_metadata_cdtext_get_type (void);

GObject *sj_metadata_cdtext_new (void);

G_END_DECLS

#endif /* SJ_METADATA_CDTEXT_H */
