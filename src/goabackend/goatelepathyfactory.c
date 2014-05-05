/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Intel Corporation
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <tp-account-widgets/tpaw-protocol.h>

#include "goatelepathyfactory.h"
#include "goaprovider-priv.h"
#include "goatelepathyprovider.h"

/**
 * SECTION:goatelepathyfactory
 * @title: GoaTelepathyFactory
 * @short_description: Factory for #GoaTelepathyProvider instances
 *
 * #GoaTelepathyFactory dynamically creates instances of #GoaTelepathyProvider
 * based on the protocols available through Telepathy.
 *
 * It accepts the protocol names from the Telepathy specification as
 * @provider_name in goa_provider_factory_get_provider().
 */

G_DEFINE_TYPE_WITH_CODE (GoaTelepathyFactory, goa_telepathy_factory, GOA_TYPE_PROVIDER_FACTORY,
                         goa_provider_ensure_extension_points_registered ();
                         g_io_extension_point_implement (GOA_PROVIDER_FACTORY_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         GOA_TELEPATHY_PROVIDER_BASE_TYPE,
                                                         0));

/* ---------------------------------------------------------------------------------------------------- */

static GoaProvider *
get_provider (GoaProviderFactory *factory,
              const gchar        *provider_name)
{
  g_return_val_if_fail (GOA_IS_TELEPATHY_FACTORY (factory), NULL);

  return GOA_PROVIDER (goa_telepathy_provider_new_from_protocol_name (provider_name));
}

/* ---------------------------------------------------------------------------------------------------- */

static void
free_list_and_unref (gpointer data)
{
  g_list_free_full (data, g_object_unref);
}

static void
get_protocols_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GSimpleAsyncResult *outer_result = user_data;
  GList *protocols = NULL;
  GList *ret;
  GList *l;
  GError *error = NULL;

#if GOA_FACEBOOK_ENABLED
  GQuark facebook_quark;
#endif
#if GOA_GOOGLE_ENABLED
  GQuark google_talk_quark;
#endif

  if (!tpaw_protocol_get_all_finish (&protocols, res, &error))
    {
      g_simple_async_result_take_error (outer_result, error);
      g_simple_async_result_complete_in_idle (outer_result);
      g_object_unref (outer_result);
      return;
    }

#if GOA_FACEBOOK_ENABLED
  facebook_quark = g_quark_from_static_string ("facebook");
#endif
#if GOA_GOOGLE_ENABLED
  google_talk_quark = g_quark_from_static_string ("google-talk");
#endif

  ret = NULL;
  for (l = protocols; l != NULL; l = l->next)
    {
      TpawProtocol *protocol = l->data;
      const gchar *service_name = tpaw_protocol_get_service_name (protocol);
      GQuark service_quark = g_quark_try_string (service_name);
      GoaTelepathyProvider *provider;

      /* If the  service is handled natively by GOA, so we don't allow
       * the creation of a Telepathy-only account. */
#if GOA_FACEBOOK_ENABLED
      if (service_quark == facebook_quark)
        continue;
#endif
#if GOA_GOOGLE_ENABLED
      if (service_quark == google_talk_quark)
        continue;
#endif

      provider = goa_telepathy_provider_new_from_protocol (protocol);
      ret = g_list_prepend (ret, provider);
    }
  ret = g_list_reverse (ret);
  g_list_free_full (protocols, g_object_unref);

  g_simple_async_result_set_op_res_gpointer (outer_result, ret, free_list_and_unref);
  g_simple_async_result_complete_in_idle (outer_result);

  g_object_unref (outer_result);
}

static void
get_providers (GoaProviderFactory  *factory,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GSimpleAsyncResult *result;

  g_return_if_fail (GOA_IS_TELEPATHY_FACTORY (factory));

  result = g_simple_async_result_new (G_OBJECT (factory), callback, user_data,
      get_providers);
  tpaw_protocol_get_all_async (get_protocols_cb, result);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
goa_telepathy_factory_init (GoaTelepathyFactory *provider)
{
}

static void
goa_telepathy_factory_class_init (GoaTelepathyFactoryClass *klass)
{
  GoaProviderFactoryClass *factory_class;

  factory_class = GOA_PROVIDER_FACTORY_CLASS (klass);
  factory_class->get_provider = get_provider;
  factory_class->get_providers = get_providers;
}
