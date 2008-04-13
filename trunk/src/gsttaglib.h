/* GStreamer taglib-based ID3 muxer
 * (c) 2006 Christophe Fergeau  <teuf@gnome.org>
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

#ifndef GST_TAG_LIB_H
#define GST_TAG_LIB_H

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _SjTagLibMuxPriv SjTagLibMuxPriv;

/* Definition of structure storing data for this element. */
typedef struct _SjTagLibMux {
  GstElement element;

  GstPad *sinkpad, *srcpad;
  GstTagList *tags;
  gsize tag_size;
  gboolean render_tag;

} SjTagLibMux;

/* Standard definition defining a class for this element. */
typedef struct _SjTagLibMuxClass {
  GstElementClass parent_class;
} SjTagLibMuxClass;

/* Standard macros for defining types for this element.  */
#define SJ_TYPE_TAGLIB_MUX \
  (sj_tag_lib_mux_get_type())
#define SJ_TAGLIB_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),SJ_TYPE_TAGLIB_MUX,SjTagLibMux))
#define SJ_TAGLIB_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),SJ_TYPE_TAGLIB_MUX,SjTagLibMuxClass))
#define SJ_IS_TAGLIB_MUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),SJ_TYPE_TAGLIB_MUX))
#define SJ_IS_TAGLIB_MUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),SJ_TYPE_TAGLIB_MUX))

/* Standard function returning type information. */
GType gst_my_filter_get_type (void);

void id3mux_register(void);

G_END_DECLS

#endif
