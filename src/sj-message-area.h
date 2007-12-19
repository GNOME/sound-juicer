/*
 * Sound Juicer - sj-message-area.h
 *
 * Based on gedit code
 * Copyright (C) 2005 - Paolo Maggi
 * Copyright (C) 2007 - The Free Software Foundation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA. 
 */

#ifndef __SJ_MESSAGE_AREA_H__
#define __SJ_MESSAGE_AREA_H__

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define SJ_TYPE_MESSAGE_AREA              (sj_message_area_get_type())
#define SJ_MESSAGE_AREA(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), SJ_TYPE_MESSAGE_AREA, SjMessageArea))
#define SJ_MESSAGE_AREA_CONST(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), SJ_TYPE_MESSAGE_AREA, SjMessageArea const))
#define SJ_MESSAGE_AREA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), SJ_TYPE_MESSAGE_AREA, SjMessageAreaClass))
#define SJ_IS_MESSAGE_AREA(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), SJ_TYPE_MESSAGE_AREA))
#define SJ_IS_MESSAGE_AREA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), SJ_TYPE_MESSAGE_AREA))
#define SJ_MESSAGE_AREA_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), SJ_TYPE_MESSAGE_AREA, SjMessageAreaClass))

/* Private structure type */
typedef struct _SjMessageAreaPrivate SjMessageAreaPrivate;

/*
 * Main object structure
 */
typedef struct _SjMessageArea SjMessageArea;

struct _SjMessageArea 
{
	GtkHBox parent;

	/*< private > */
	SjMessageAreaPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _SjMessageAreaClass SjMessageAreaClass;

struct _SjMessageAreaClass
{
	GtkHBoxClass parent_class;

	/* Signals */
	void (* response) (SjMessageArea *message_area, gint response_id);

	/* Keybinding signals */
	void (* close)    (SjMessageArea *message_area);

	/* Padding for future expansion */
	void (*_sj_reserved1) (void);
	void (*_sj_reserved2) (void);
};

/*
 * Public methods
 */
GType 		 sj_message_area_get_type 		(void) G_GNUC_CONST;

GtkWidget	*sj_message_area_new      		(void);

GtkWidget	*sj_message_area_new_with_buttons	(const gchar   *first_button_text,
                                        		 ...);

void		 sj_message_area_set_contents		(SjMessageArea *message_area,
                                             		 GtkWidget     *contents);

void		 sj_message_area_add_action_widget	(SjMessageArea *message_area,
                                         		 GtkWidget     *child,
                                         		 gint           response_id);

GtkWidget	*sj_message_area_add_button        	(SjMessageArea *message_area,
                                         		 const gchar   *button_text,
                                         		 gint           response_id);

GtkWidget	*sj_message_area_add_stock_button_with_text
							(SjMessageArea *message_area,
				    			 const gchar   *text,
				    			 const gchar   *stock_id,
				    			 gint           response_id);

void       	 sj_message_area_add_buttons 		(SjMessageArea *message_area,
                                         		 const gchar   *first_button_text,
                                         		 ...);

void		 sj_message_area_set_response_sensitive
							(SjMessageArea *message_area,
                                        		 gint           response_id,
                                        		 gboolean       setting);
void 		 sj_message_area_set_default_response
							(SjMessageArea *message_area,
                                        		 gint           response_id);

/* Emit response signal */
void		 sj_message_area_response           	(SjMessageArea *message_area,
                                    			 gint           response_id);

G_END_DECLS

#endif  /* __SJ_MESSAGE_AREA_H__  */
