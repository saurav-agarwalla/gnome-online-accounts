/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2011 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#if !defined (__GOA_BACKEND_INSIDE_GOA_BACKEND_H__) && !defined (GOA_BACKEND_COMPILATION)
#error "Only <goabackend/goabackend.h> can be included directly."
#endif

#ifndef __GOA_YAHOO_PROVIDER_H__
#define __GOA_YAHOO_PROVIDER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GOA_TYPE_YAHOO_PROVIDER   (goa_yahoo_provider_get_type ())
#define GOA_YAHOO_PROVIDER(o)     (G_TYPE_CHECK_INSTANCE_CAST ((o), GOA_TYPE_YAHOO_PROVIDER, GoaYahooProvider))
#define GOA_IS_YAHOO_PROVIDER(o)  (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOA_TYPE_YAHOO_PROVIDER))

typedef struct _GoaYahooProvider GoaYahooProvider;

GType goa_yahoo_provider_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GOA_YAHOO_PROVIDER_H__ */
