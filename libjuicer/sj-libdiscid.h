/*
 * sj-discid.h
 * Copyright (C) 2016 Phillip Wood <phillip.wood@dunelm.org.uk>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Provide dummy functions for older versions of libdiscid that are
 * missing have discid_get_mcn(), discid_has_feature() or
 * discid_sparse_read()
 */

#ifndef SJ_LIBDISCID
#define SJ_LIBDISCID

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <discid/discid.h>

#ifndef HAVE_DISCID_GET_MCN
#define discid_get_mcn(x) NULL
#endif /* HAVE_DISCID_GET_MCN */

#ifndef HAVE_DISCID_HAS_FEATURE
enum discid_feature {
        DISCID_FEATURE_READ = 1 << 0,
        DISCID_FEATURE_MCN  = 1 << 1,
        DISCID_FEATURE_ISRC = 1 << 2,
};

/* Report TRUE for everything */
#define discid_has_feature(x) TRUE
#endif /* HAVE_DISCID_HAS_FEATURE */

#ifndef DISCID_HAVE_SPARSE_READ
#define discid_read_sparse(disc, dev, i) discid_read(disc, dev)
#endif /* DISCID_HAVE_SPARSE_READ */

#endif /* SJ_LIBDISCID */
