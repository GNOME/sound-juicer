#ifndef SJ_ERROR_H
#define SJ_ERROR_H

#include <glib.h>

#define SJ_ERROR sj_error_quark ()

typedef enum {
        SJ_ERROR_INTERNAL_ERROR
} SjError;

GQuark sj_error_quark (void) G_GNUC_CONST;

#endif
