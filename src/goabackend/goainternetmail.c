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

#include "config.h"
#include <glib/gi18n-lib.h>
#include <glib-unix.h>

#include <rest/oauth-proxy.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>

#include "goalogging.h"
#include "goaimapauth.h"
#include "goaimapclient.h"
#include "goainternetmail.h"

/**
 * GoaInternetMail:
 *
 * The #GoaInternetMail structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _GoaInternetMail
{
  /*< private >*/
  GoaMailSkeleton parent_instance;

  gboolean imap_ignore_bad_tls;
  GoaImapAuth *imap_auth;

  gboolean smtp_ignore_bad_tls;
};

typedef struct _GoaInternetMailClass GoaInternetMailClass;

struct _GoaInternetMailClass
{
  GoaMailSkeletonClass parent_class;
};

enum
{
  PROP_0,
  PROP_IMAP_IGNORE_BAD_TLS,
  PROP_IMAP_AUTH,
  PROP_SMTP_IGNORE_BAD_TLS
};

/**
 * SECTION:goainternetmail
 * @title: GoaInternetMail
 * @short_description: Implementation of the #GoaMail interface for standards-based Internet Mail.
 *
 * #GoaInternetMail is an implementation of the #GoaMail D-Bus
 * interface for IMAP and SMTP servers.
 */

static void goa_internet_mail__goa_mail_iface_init (GoaMailIface *iface);

G_DEFINE_TYPE_WITH_CODE (GoaInternetMail, goa_internet_mail, GOA_TYPE_MAIL_SKELETON,
                         G_IMPLEMENT_INTERFACE (GOA_TYPE_MAIL, goa_internet_mail__goa_mail_iface_init));

/* ---------------------------------------------------------------------------------------------------- */

static void
goa_internet_mail_finalize (GObject *object)
{
  GoaInternetMail *mail = GOA_INTERNET_MAIL (object);

  g_object_unref (mail->imap_auth);

  G_OBJECT_CLASS (goa_internet_mail_parent_class)->finalize (object);
}

static void
goa_internet_mail_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GoaInternetMail *mail = GOA_INTERNET_MAIL (object);

  switch (prop_id)
    {
    case PROP_IMAP_IGNORE_BAD_TLS:
      g_value_set_boolean (value, mail->imap_ignore_bad_tls);
      break;

    case PROP_IMAP_AUTH:
      g_value_set_object (value, mail->imap_auth);
      break;

    case PROP_SMTP_IGNORE_BAD_TLS:
      g_value_set_boolean (value, mail->smtp_ignore_bad_tls);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
goa_internet_mail_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GoaInternetMail *mail = GOA_INTERNET_MAIL (object);

  switch (prop_id)
    {
    case PROP_IMAP_IGNORE_BAD_TLS:
      mail->imap_ignore_bad_tls = g_value_get_boolean (value);
      break;

    case PROP_IMAP_AUTH:
      mail->imap_auth = g_value_dup_object (value);
      break;

    case PROP_SMTP_IGNORE_BAD_TLS:
      mail->smtp_ignore_bad_tls = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
goa_internet_mail_init (GoaInternetMail *mail)
{
  /* Ensure D-Bus method invocations run in their own thread */
  g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (mail),
                                       G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
}

static void
goa_internet_mail_class_init (GoaInternetMailClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = goa_internet_mail_finalize;
  gobject_class->set_property = goa_internet_mail_set_property;
  gobject_class->get_property = goa_internet_mail_get_property;

  g_object_class_install_property (gobject_class,
                                   PROP_IMAP_IGNORE_BAD_TLS,
                                   g_param_spec_boolean ("imap-ignore-bad-tls",
                                                         "imap-ignore-bad-tls",
                                                         "imap-ignore-bad-tls",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_IMAP_AUTH,
                                   g_param_spec_object ("imap-auth",
                                                        "imap-auth",
                                                        "imap-auth",
                                                        GOA_TYPE_IMAP_AUTH,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
                                   PROP_SMTP_IGNORE_BAD_TLS,
                                   g_param_spec_boolean ("smtp-ignore-bad-tls",
                                                         "smtp-ignore-bad-tls",
                                                         "smtp-ignore-bad-tls",
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * goa_internet_mail_new:
 * @imap_host: The IMAP server.
 * @imap_user_name: (allow-none): The user name to use.
 * @imap_use_tls: Whether TLS should be used connections to the IMAP server.
 * @imap_ignore_bad_tls: Whether errors (e.g. %G_TLS_ERROR_BAD_CERTIFICATE) about TLS certificates while connecting to @imap_host should be ignored.
 * @imap_auth: Object used for authenticating the IMAP connection.
 * @smtp_host: The SMTP server.
 * @smtp_user_name: (allow-none): The user name to use.
 * @smtp_use_tls: Whether TLS should be used connections to the SMTP server.
 * @smtp_ignore_bad_tls: Whether errors (e.g. %G_TLS_ERROR_BAD_CERTIFICATE) about TLS certificates while connecting to @smtp_host should be ignored.
 *
 * Creates a new #GoaMail object.
 *
 * Returns: (type GoaInternetMail): A new #GoaMail instance.
 */
GoaMail *
goa_internet_mail_new (const gchar  *imap_host,
                       const gchar  *imap_user_name,
                       gboolean      imap_use_tls,
                       gboolean      imap_ignore_bad_tls,
                       GoaImapAuth  *imap_auth,
                       const gchar  *smtp_host,
                       const gchar  *smtp_user_name,
                       gboolean      smtp_use_tls,
                       gboolean      smtp_ignore_bad_tls)
{
  g_return_val_if_fail (imap_host != NULL, NULL);
  return GOA_MAIL (g_object_new (GOA_TYPE_INTERNET_MAIL,
                                 "imap-supported", TRUE,
                                 "imap-host", imap_host,
                                 "imap-user-name", imap_user_name,
                                 "imap-use-tls", imap_use_tls,
                                 "imap-ignore-bad-tls", imap_ignore_bad_tls,
                                 "imap-auth", imap_auth,
                                 "smtp-supported", TRUE,
                                 "smtp-host", smtp_host,
                                 "smtp-user-name", smtp_user_name,
                                 "smtp-use-tls", smtp_use_tls,
                                 "smtp-ignore-bad-tls", smtp_ignore_bad_tls,
                                 NULL));
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  volatile gint ref_count;

  GoaInternetMail *mail;
  GoaMailMonitor *monitor;

  /* Used so we can nuke the monitor if the creator vanishes */
  guint name_watcher_id;

  /* Set only when we are connected */
  gchar       *spam_folder;
  gchar       *starred_folder;

  /* Used to communicate with the thread running the IMAP client */
  GCancellable *imap_cancellable;
  gboolean      imap_request_close;
  GMutex       *imap_counter_lock;
  GCond        *imap_counter_cond;
  gint          imap_num_refreshes;
  gint          imap_num_connections_failed;
} MonitorData;

static MonitorData *
monitor_data_ref (MonitorData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static void
monitor_data_unref (MonitorData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_clear_object (&data->mail);
      g_clear_object (&data->monitor);
      if (data->name_watcher_id)
        g_bus_unwatch_name (data->name_watcher_id);

      g_free (data->spam_folder);
      g_free (data->starred_folder);

      g_clear_object (&data->imap_cancellable);
      if (data->imap_counter_lock != NULL)
        g_mutex_free (data->imap_counter_lock);
      if (data->imap_counter_cond != NULL)
        g_cond_free (data->imap_counter_cond);
      g_slice_free (MonitorData, data);
    }
}

static void
nuke_monitor (MonitorData *data)
{
  /* unexport the D-Bus object */
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (data->monitor));
  /* nuke the running IMAP client */
  data->imap_request_close = TRUE;
  g_mutex_lock (data->imap_counter_lock);
  g_cancellable_cancel (data->imap_cancellable);
  g_mutex_unlock (data->imap_counter_lock);
  monitor_data_unref (data);
}

static void
on_monitor_owner_vanished (GDBusConnection *connection,
                           const gchar     *name,
                           gpointer         user_data)
{
  MonitorData *data = user_data;
  /* yippee ki yay motherfucker */
  nuke_monitor (data);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  MonitorData *monitor_data;
  gint         num_exists;
  gint         last_num_exists;

  gint         uidvalidity;

  gchar      **caps;
  gchar       *caps_string;
} ImapClientData;

static gboolean
imap_client_has_capability (ImapClientData *data,
                            const gchar    *capability)
{
  guint n;
  gboolean ret;

  ret = FALSE;

  for (n = 0; data->caps != NULL && data->caps[n] != NULL; n++)
    {
      if (g_strcmp0 (data->caps[n], capability) == 0)
        {
          ret = TRUE;
          goto out;
        }
    }
 out:
  return ret;
}

static gboolean
parse_int (const gchar *s,
           gint        *out_result)
{
  gboolean ret;
  gchar *endp;
  gint result;

  g_return_val_if_fail (s != NULL, FALSE);

  ret = FALSE;
  result = strtol (s, &endp, 0);
  if (result == 0 && endp == s)
    goto out;

  if (out_result != NULL)
    *out_result = result;

  ret = TRUE;

 out:
  return ret;
}

static gboolean
fetch_check (const gchar *data,
             guint       *pos,
             const gchar *key)
{
  gsize key_len;
  gboolean ret;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  ret = FALSE;

  key_len = strlen (key);
  if (g_ascii_strncasecmp (data + *pos, key, key_len) == 0 && data[*pos + key_len] == ' ')
    {
      ret = TRUE;
      *pos += key_len + 1;
      goto out;
    }
 out:
  return ret;
}

static gchar **
fetch_parenthesized_list (const gchar *data,
                          guint       *pos)
{
  gchar **ret;
  gchar *s;
  guint start_pos;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);

  ret = NULL;

  if (data[*pos] != '(')
    goto out;
  *pos += 1;
  start_pos = *pos;

  while (data[*pos] != ')' && data[*pos] != '\0')
    *pos += 1;
  if (data[*pos] != ')')
    goto out;

  s = g_strndup (data + start_pos, *pos - start_pos);
  ret = g_strsplit (s, " ", -1);
  g_free (s);

  *pos += 1;

 out:
  return ret;
}

static gchar *
fetch_string (const gchar *data,
              guint       *pos)
{
  gchar *ret;
  guint start_pos;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);

  ret = NULL;

  start_pos = *pos;

  while (data[*pos] != ' ' && data[*pos] != ')' && data[*pos] != '\0')
    *pos += 1;

  ret = g_strndup (data + start_pos, *pos - start_pos);

  return ret;
}

static gboolean
fetch_int (const gchar *data,
           guint       *pos,
           gint        *out_value)
{
  gchar *str_value;
  gboolean ret;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);
  g_return_val_if_fail (out_value != NULL, FALSE);

  str_value = NULL;
  ret = FALSE;

  str_value = fetch_string (data, pos);
  if (str_value == NULL)
    goto out;

  if (!parse_int (str_value, out_value))
    goto out;

  ret = TRUE;

 out:
  g_free (str_value);
  return ret;
}

static gchar *
fetch_quoted_string (const gchar *data,
                     guint       *pos)
{
  gchar *ret;
  guint start_pos;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);

  ret = NULL;

  if (data[*pos] != '"')
    goto out;
  *pos += 1;

  start_pos = *pos;

  while (data[*pos] != '"' && data[*pos] != '\0')
    *pos += 1;

  ret = g_strndup (data + start_pos, *pos - start_pos);

  *pos += 1;

 out:
  return ret;
}

static gchar *
fetch_literal_string (const gchar *data,
                      guint       *pos,
                      guint       *out_len)
{
  gchar *ret;
  guint start_pos;
  guint len;

  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (pos != NULL, FALSE);

  ret = NULL;

  start_pos = *pos;

  if (data[*pos] != '{')
    goto out;
  *pos += 1;
  while (g_ascii_isdigit (data[*pos]))
    *pos += 1;
  if (strncmp (data + *pos, "}\r\n", 3) != 0)
    goto out;
  *pos += 3;

  if (!parse_int (data + start_pos + 1, (gint*) &len))
    goto out;

  ret = g_strndup (data + *pos, len);
  *pos += len;

  if (out_len != NULL)
    *out_len = len;

 out:
  return ret;
}

/* TODO: try a little harder to make this a conformant RFC822 parser */
static GHashTable *
parse_rfc822_headers (const gchar *rfc822_headers)
{
  GHashTable *ret;
  gchar **lines;
  guint n;

  ret = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
  lines = g_strsplit (rfc822_headers, "\r\n", -1);
  for (n = 0; lines[n] != NULL; n++)
    {
      const gchar *line = lines[n];
      const gchar *s;

      if (line[0] == '\0')
        continue;

      s = strstr (line, ": ");
      if (s != NULL)
        {
          gchar *key;
          gchar *value;
          key = g_strndup (line, s - line);
          value = g_strdup (s + 2);
          g_hash_table_insert (ret, key, value);
        }
      else
        {
          goa_warning ("%s: ignoring mysterious line `%s' whilst parsing `%s'",
                       G_STRFUNC, line, rfc822_headers);
        }
    }
  g_strfreev (lines);

  return ret;
}

/* Simple FETCH response parser only handling a subset of FETCH
 * responses, see
 *
 *   http://tools.ietf.org/html/rfc3501#section-7.4.2
 *
 * for more details.
 */
static void
imap_client_handle_fetch_response (ImapClientData  *data,
                                   guint            message_seqnum,
                                   const gchar     *params)
{
  guint n;
  gboolean parsed;
  gboolean has_uid;
  gint uid;
  gchar *rfc822_headers;
  guint rfc822_headers_len;
  gchar *excerpt;
  guint excerpt_len;
  GHashTable *headers;
  const gchar *from_header;
  const gchar *subject_header;
  gchar *uid_str;
  gchar *uri;
  /* GVariantBuilder extras_builder; */

  g_return_if_fail (message_seqnum >= 1);
  g_return_if_fail (params != NULL);

  uid = 0;
  has_uid = FALSE;
  excerpt = NULL;
  rfc822_headers = NULL;
  headers = NULL;
  uid_str = NULL;
  uri = NULL;
  parsed = FALSE;

  if (params[0] != '(')
    goto out;
  n = 1;
  while (params[n] != ')' && params[n] != '\0')
    {
      if (fetch_check (params, &n, "UID"))
        {
          if (!fetch_int (params, &n, &uid))
            goto out;
          has_uid = TRUE;
        }
      else if (fetch_check (params, &n, "BODY[HEADER.FIELDS (Date From To Cc Subject)]"))
        {
          rfc822_headers = fetch_literal_string (params, &n, &rfc822_headers_len);
          if (rfc822_headers == NULL)
            goto out;
        }
      else if (fetch_check (params, &n, "BODY[TEXT]<0>"))
        {
          excerpt = fetch_literal_string (params, &n, &excerpt_len);
          if (excerpt == NULL)
            goto out;
        }
      else
        {
          /* Don't know how to handle unknown params so fail completely */
          goto out;
        }
      /* advance to next value in FETCH response list, if any */
      while (params[n] == ' ')
        n++;
    }

  if (!has_uid || rfc822_headers == NULL || excerpt == NULL)
    goto out;

  /* OK, message is valid */
  parsed = TRUE;

  uid_str = g_strdup_printf ("%" G_GUINT64_FORMAT,
                             ((guint64) data->uidvalidity << 32) | ((guint64) uid));
  headers = parse_rfc822_headers (rfc822_headers);
  from_header = g_hash_table_lookup (headers, "From");
  subject_header = g_hash_table_lookup (headers, "Subject");
  if (from_header == NULL)
    from_header = "";
  if (subject_header == NULL)
    subject_header = "";

  /* TODO: set this */
  uri = g_strdup ("");

  /* extras is currently unused (and not currently in the D-Bus signature) */
  /* g_variant_builder_init (&extras_builder, G_VARIANT_TYPE_VARDICT); */

  /* Emit D-Bus message */
  goa_mail_monitor_emit_message_received (data->monitor_data->monitor,
                                          uid_str,
                                          from_header,
                                          subject_header,
                                          excerpt,
                                          uri,
                                          data->monitor_data->spam_folder != NULL,
                                          data->monitor_data->starred_folder != NULL);
  /* g_variant_builder_end (&extras_builder)); */
 out:
  if (!parsed)
    {
      /* Use goa_warning() since we want bug-reports in order to improve the FETCH parser */
      goa_warning ("Failed parsing FETCH response for message with sequence number %d and parameters `%s'. "
                   "Please report this to %s",
                   message_seqnum,
                   params,
                   PACKAGE_BUGREPORT);
    }
  g_free (uri);
  g_free (uid_str);
  if (headers != NULL)
    g_hash_table_unref (headers);
  g_free (rfc822_headers);
  g_free (excerpt);
}

static void
imap_client_handle_xlist_response (ImapClientData  *data,
                                   const gchar     *response)
{
  guint pos;
  gchar **flags;
  gchar *delimiter;
  gchar *name;
  gboolean is_spam;
  gboolean is_starred;
  guint n;

  flags = NULL;
  delimiter = NULL;
  name = NULL;
  is_spam = FALSE;
  is_starred = FALSE;

  /* http://code.google.com/apis/gmail/imap/#xlist and
   * http://tools.ietf.org/html/rfc3501#section-7.2.2
   *
   * Example: XLIST (\HasChildren \HasNoChildren \Starred) "/" "[Gmail]/Starred"
   */
  pos = sizeof "XLIST " - 1;
  flags = fetch_parenthesized_list (response, &pos);
  if (flags == NULL)
    {
      goa_warning ("Error extracting flags from XLIST response `%s'", response);
      goto out;
    }
  pos += 1;
  delimiter = fetch_quoted_string (response, &pos);
  if (delimiter == NULL)
    {
      goa_warning ("Error extracting delimiter from XLIST response `%s'", response);
      goto out;
    }
  pos += 1;
  name = fetch_quoted_string (response, &pos);
  if (name == NULL)
    {
      goa_warning ("Error extracting name from XLIST response `%s'", response);
      goto out;
    }

  for (n = 0; flags[n] != NULL; n++)
    {
      if (g_strcmp0 (flags[n], "\\Spam") == 0)
        is_spam = TRUE;
      else if (g_strcmp0 (flags[n], "\\Starred") == 0)
        is_starred = TRUE;
    }

  if (is_spam)
    {
      goa_info ("Setting the Spam folder to `%s'", name);
      g_free (data->monitor_data->spam_folder);
      data->monitor_data->spam_folder = g_strdup (name);
    }
  else if (is_starred)
    {
      goa_info ("Setting the Starred folder to `%s'", name);
      g_free (data->monitor_data->starred_folder);
      data->monitor_data->starred_folder = g_strdup (name);
    }

 out:
  g_strfreev (flags);
  g_free (delimiter);
  g_free (name);
}

static void
imap_client_on_untagged_response (GoaImapClient  *client,
                                  const gchar    *response,
                                  gpointer        user_data)
{
  ImapClientData *data = user_data;
  gchar s[64+1];
  gint i;
  gint n;

  if (sscanf (response, "%d %64s", &i, s) == 2 && g_strcmp0 (s, "EXISTS") == 0)
    {
      data->num_exists = i;
    }
  else if (sscanf (response, "%d %64s", &i, s) == 2 && g_strcmp0 (s, "EXPUNGE") == 0)
    {
      /* See http://tools.ietf.org/html/rfc3501#section-7.4.1 */
      data->num_exists -= 1;
      data->last_num_exists -= 1;
    }
  else if (sscanf (response, "OK [UIDVALIDITY %d]", &i) == 1)
    {
      data->uidvalidity = i;
    }
  else if (sscanf (response, "%d %64s%n", &i, s, &n) == 2 && g_strcmp0 (s, "FETCH") == 0)
    {
      const gchar *params;
      params = response + n;
      while (g_ascii_isspace (*params))
        params++;
      imap_client_handle_fetch_response (data, i, params);
    }
  else if (g_str_has_prefix (response, "CAPABILITY "))
    {
      /* http://tools.ietf.org/html/rfc3501#section-7.2.1 */
      g_strfreev (data->caps);
      g_free (data->caps_string);
      data->caps_string = g_strdup (response + sizeof "CAPABILITY " - 1);
      data->caps = g_strsplit (data->caps_string, " ", -1);
    }
  else if (g_str_has_prefix (response, "XLIST "))
    {
      imap_client_handle_xlist_response (data, response);
    }
  else
    {
      goa_debug ("unhandled untagged response `%s'", response);
    }
}

static void
imap_client_sync_single (ImapClientData *data)
{
  GError *error;
  gchar *response;
  GoaImapClient *client;

  g_strfreev (data->caps);
  g_free (data->caps_string);
  data->caps = NULL;
  data->caps_string = NULL;

  /* Get ourselves an IMAP client and connect to the server */
  data->num_exists = -1;
  client = goa_imap_client_new ();

  g_signal_connect (client,
                    "untagged-response",
                    G_CALLBACK (imap_client_on_untagged_response),
                    data);

  error = NULL;
  if (!goa_imap_client_connect_sync (client,
                                     goa_mail_get_imap_host (GOA_MAIL (data->monitor_data->mail)),
                                     goa_mail_get_imap_use_tls (GOA_MAIL (data->monitor_data->mail)),
                                     data->monitor_data->mail->imap_ignore_bad_tls,
                                     data->monitor_data->mail->imap_auth,
                                     NULL, /* GCancellable */
                                     &error))
    goto out;

  /* Houston, we have a connection */
  goa_mail_monitor_set_connected (data->monitor_data->monitor, TRUE);

  /* Request capabilities unless we received them already */
  if (data->caps == NULL)
    {
      /* http://tools.ietf.org/html/rfc3501#section-6.1.1 */
      error = NULL;
      response = goa_imap_client_run_command_sync (client,
                                                   "CAPABILITY",
                                                   NULL, /* GCancellable */
                                                   &error);
      if (response == NULL)
        goto out;
      g_free (response);
    }
  if (!imap_client_has_capability (data, "IMAP4rev1"))
    {
      g_set_error (&error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Expected capability IMAP4rev1 but server reported: %s",
                   data->caps_string);
      goto out;
    }
  goa_info ("IMAP Server reported capabilities: %s", data->caps_string);

  /* If available, use the XLIST command to find the Spam and Starred folders */
  if (imap_client_has_capability (data, "XLIST"))
    {
      /* http://code.google.com/apis/gmail/imap/#xlist and
       * http://tools.ietf.org/html/rfc3501#section-6.3.8
       */
      error = NULL;
      response = goa_imap_client_run_command_sync (client,
                                                   "XLIST \"\" \"*\"",
                                                   NULL, /* GCancellable */
                                                   &error);
      if (response == NULL)
        goto out;
      g_free (response);
    }

  /* First, select the INBOX - this is guaranteed to emit the EXISTS untagged response */
  error = NULL;
  response = goa_imap_client_run_command_sync (client,
                                               "SELECT INBOX",
                                               NULL, /* GCancellable */
                                               &error);
  if (response == NULL)
    goto out;
  g_free (response);

  if (data->num_exists == -1)
    {
      g_set_error (&error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "Expected EXISTS untagged response for SELECT but received none");
      goto out;
    }
  data->last_num_exists = data->num_exists;

  /* This is the main loop where we idle, then refresh, then idle,
   * then refresh again and around and around she goes...
   */
  while (TRUE)
    {
      /* If the connection is closed/severed, this is the way we find out since
       * the IDLE command submitted above disables timeouts
       */
      response = goa_imap_client_run_command_sync (client,
                                                   "NOOP",
                                                   NULL, /* GCancellable */
                                                   &error);
      if (response == NULL)
        goto out;
      g_free (response);

      goa_debug ("EXISTS delta: %d", data->num_exists - data->last_num_exists);

      /* Fetch newly received messages, if any - the D-Bus signal will
       * get emitted from imap_client_handle_fetch_response() that
       * will be called while the command is pending
       */
      if (data->num_exists > data->last_num_exists)
        {
          GString *request_str;
          guint num_new_messages;
          guint n;

          num_new_messages = data->num_exists - data->last_num_exists;
          request_str = g_string_new ("FETCH ");
          for (n = 0; n < num_new_messages; n++)
            {
              if (n > 0)
                g_string_append_c (request_str, ',');
              g_string_append_printf (request_str, "%d", data->last_num_exists + 1 + n);
            }

          g_string_append (request_str,
                           " ("
                           "UID "
                           "BODY.PEEK[HEADER.FIELDS (Date From To Cc Subject)] "
                           "BODY.PEEK[TEXT]<0.1000>"
                           ")");
          error = NULL;
          response = goa_imap_client_run_command_sync (client,
                                                               request_str->str,
                                                               NULL, /* GCancellable */
                                                               &error);
          g_string_free (request_str, TRUE);
          if (response == NULL)
            goto out;
          g_free (response);
        }
      data->last_num_exists = data->num_exists;

      /* Wake up waiters */
      g_mutex_lock (data->monitor_data->imap_counter_lock);
      data->monitor_data->imap_num_refreshes += 1;
      g_cond_broadcast (data->monitor_data->imap_counter_cond);
      g_mutex_unlock (data->monitor_data->imap_counter_lock);

      goa_debug ("Idling");

      /* Never idle for more than 25 minutes cf. the recommendation
       * in RFC 2177: http://tools.ietf.org/html/rfc2177
       */
      error = NULL;
      if (!goa_imap_client_idle_sync (client,
                                      25 * 60,
                                      data->monitor_data->imap_cancellable,
                                      &error))
        {
          if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
            {
              g_cancellable_reset (data->monitor_data->imap_cancellable);
              g_error_free (error);
              error = NULL;
            }
          else
            {
              goto out;
            }
        }

      /* Check if asked to close */
      if (data->monitor_data->imap_request_close)
        goto out;

    } /* Main loop */

 out:
  /* We no longer have a connection */
  goa_mail_monitor_set_connected (data->monitor_data->monitor, FALSE);

  /* Wake up waiters */
  g_mutex_lock (data->monitor_data->imap_counter_lock);
  data->monitor_data->imap_num_connections_failed += 1;
  g_cond_broadcast (data->monitor_data->imap_counter_cond);
  g_mutex_unlock (data->monitor_data->imap_counter_lock);

  if (error != NULL)
    {
      goa_info ("IMAP connection failed: error: %s (%s, %d)",
                error->message, g_quark_to_string (error->domain), error->code);
      g_error_free (error);
    }
  else
    {
      goa_info ("IMAP connection closed");
    }
  g_signal_handlers_disconnect_by_func (client,
                                        G_CALLBACK (imap_client_on_untagged_response),
                                        data);
  error = NULL;
  if (!goa_imap_client_disconnect_sync (client,
                                        NULL, /* GCancellable */
                                        &error))
    {
      goa_warning ("Error closing connection: %s (%s, %d)",
                   error->message, g_quark_to_string (error->domain), error->code);
      g_error_free (error);
    }
  g_object_unref (client);
}

static void
imap_client_sync (MonitorData *data)
{
  ImapClientData *imap_data;

  imap_data = g_slice_new0 (ImapClientData);
  imap_data->monitor_data = monitor_data_ref (data);

  goa_info ("Using thread for IMAP client at %s for account %s",
            goa_mail_get_imap_host (GOA_MAIL (data->mail)),
            g_dbus_object_get_object_path (g_dbus_interface_get_object (G_DBUS_INTERFACE (data->mail))));

  while (TRUE)
    {
      GPollFD poll_fd;

      goa_info ("Connecting to IMAP server at %s for account %s",
                goa_mail_get_imap_host (GOA_MAIL (data->mail)),
                g_dbus_object_get_object_path (g_dbus_interface_get_object (G_DBUS_INTERFACE (data->mail))));

      /* tries connecting - blocks until the connection is closed */
      imap_client_sync_single (imap_data);

      if (data->imap_request_close)
        goto out;

      goa_info ("Not connected anymore. Sleeping until Refresh() is called...");

      /* Wait to get woken up */
      if (g_cancellable_make_pollfd (data->imap_cancellable, &poll_fd))
        {
          gint poll_ret;
          do
            {
              poll_ret = g_poll (&poll_fd, 1, -1);
            }
          while (poll_ret == -1 && errno == EINTR);
          g_cancellable_release_fd (data->imap_cancellable);
          g_cancellable_reset (data->imap_cancellable);
        }

      if (data->imap_request_close)
        goto out;
    }

 out:

  /* Wake up waiters (if any) */
  g_mutex_lock (data->imap_counter_lock);
  data->imap_num_refreshes = -1;
  data->imap_num_connections_failed = -1;
  g_cond_broadcast (data->imap_counter_cond);
  g_mutex_unlock (data->imap_counter_lock);

  goa_info ("Exiting IMAP client thread");

  monitor_data_unref (imap_data->monitor_data);
  g_strfreev (imap_data->caps);
  g_free (imap_data->caps_string);
  g_slice_free (ImapClientData, imap_data);
}


/* ---------------------------------------------------------------------------------------------------- */

/* runs in thread dedicated to the method invocation */
static gboolean
monitor_on_handle_refresh (GoaMailMonitor        *monitor,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
  MonitorData *data = user_data;
  gint orig_imap_num_connections_failed;
  gint orig_imap_num_refreshes;
  gboolean refreshed, connection_failed, closed;
  gint num_attempts;

  monitor_data_ref (data);

  num_attempts = 0;
 again:
  g_mutex_lock (data->imap_counter_lock);
  orig_imap_num_refreshes = data->imap_num_refreshes;
  orig_imap_num_connections_failed = data->imap_num_connections_failed;
  /* Wake up the IMAP client thread - this will cause either a connection
   * failure or a refresh
   */
  g_cancellable_cancel (data->imap_cancellable);
  g_cond_wait (data->imap_counter_cond, data->imap_counter_lock);
  num_attempts += 1;

  closed = (orig_imap_num_refreshes == -1);
  refreshed = (orig_imap_num_refreshes != data->imap_num_refreshes);
  connection_failed = (orig_imap_num_connections_failed != data->imap_num_connections_failed);
  g_mutex_unlock (data->imap_counter_lock);

  g_warn_if_fail (refreshed || connection_failed || closed);

  if (refreshed)
    {
      goa_mail_monitor_complete_refresh (monitor, invocation);
      goto out;
    }

  if (closed)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             GOA_ERROR,
                                             GOA_ERROR_FAILED,
                                             "The monitor was closed");
      goto out;
    }

  /* Try at least three times to cope with broken connections */
  if (connection_failed && num_attempts < 3)
    goto again;

  if (connection_failed)
    {
      g_dbus_method_invocation_return_error (invocation,
                                             GOA_ERROR,
                                             GOA_ERROR_FAILED,
                                             "Failed to reconnect (tried %d times)",
                                             num_attempts);
      goto out;
    }

  /* should never end up here but if we do, make sure
   * the bug reporters can give us something useful
   */
  goa_warning ("Unexpected state while trying to refresh: refreshed=%d connection_failed=%d closed=%d num_attempts=%d",
               refreshed, connection_failed, closed, num_attempts);
  g_dbus_method_invocation_return_error (invocation,
                                         GOA_ERROR,
                                         GOA_ERROR_FAILED,
                                         "Failed with unexpected state: "
                                         "refreshed=%d connection_failed=%d closed=%d num_attempts=%d",
                                         refreshed, connection_failed, closed, num_attempts);

 out:
  monitor_data_unref (data);
  return TRUE; /* invocation was handled */
}

/* runs in thread dedicated to the method invocation */
static gboolean
monitor_on_handle_close (GoaMailMonitor        *monitor,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  MonitorData *data = user_data;
  /* yippee ki yay motherfucker */
  nuke_monitor (data);
  goa_mail_monitor_complete_close (monitor, invocation);
  return TRUE; /* invocation was handled */
}

/* runs in thread dedicated to the method invocation */
static gboolean
monitor_on_handle_simulate_message_received (GoaMailMonitor        *monitor,
                                             GDBusMethodInvocation *invocation,
                                             const gchar           *uid,
                                             const gchar           *from,
                                             const gchar           *subject,
                                             const gchar           *excerpt,
                                             const gchar           *uri,
                                             gboolean               can_be_marked_as_spam,
                                             gboolean               can_be_starred,
                                             gpointer               user_data)
{
  goa_mail_monitor_emit_message_received (monitor, uid, from, subject, excerpt, uri, can_be_marked_as_spam, can_be_starred);
  goa_mail_monitor_complete_simulate_message_received (monitor, invocation);
  return TRUE; /* invocation was handled */
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_untagged_response_uidvalidity (GoaImapClient  *client,
                                  const gchar    *response,
                                  gpointer        user_data)
{
  guint *uidvalidity = user_data;
  guint n;

  if (sscanf (response, "OK [UIDVALIDITY %d]", &n) == 1)
    *uidvalidity = n;
}


typedef gboolean (*ImapHelperFunc) (GoaImapClient  *client,
                                    gpointer        user_data,
                                    GError        **error);

static gboolean
imap_helper (const gchar    *imap_host,
             gboolean        imap_use_tls,
             gboolean        imap_ignore_bad_tls,
             GoaImapAuth    *auth,
             guint           uidvalidity,
             ImapHelperFunc  func,
             gpointer        func_user_data,
             GError        **error)
{
  GoaImapClient *client;
  gchar *response;
  guint read_uidvalidity;
  gboolean ret;

  client = NULL;
  read_uidvalidity = 0;
  ret = FALSE;

  client = goa_imap_client_new ();
  g_signal_connect (client,
                    "untagged-response",
                    G_CALLBACK (on_untagged_response_uidvalidity),
                    &read_uidvalidity);
  if (!goa_imap_client_connect_sync (client,
                                     imap_host,
                                     imap_use_tls,
                                     imap_ignore_bad_tls,
                                     auth,
                                     NULL, /* GCancellable */
                                     error))
    goto out;

  response = goa_imap_client_run_command_sync (client,
                                               "SELECT INBOX",
                                               NULL, /* GCancellable */
                                               error);
  if (response == NULL)
    goto out;
  g_free (response);

  if (read_uidvalidity != uidvalidity)
    {
      g_set_error (error,
                   GOA_ERROR,
                   GOA_ERROR_FAILED,
                   "UID validity does not match");
      goto out;
    }

  if (!func (client, func_user_data, error))
    goto out;

  ret = TRUE;

 out:
  if (client != NULL)
    {
      GError *local_error;

      g_signal_handlers_disconnect_by_func (client,
                                            G_CALLBACK (on_untagged_response_uidvalidity),
                                            &uidvalidity);

      local_error = NULL;
      if (!goa_imap_client_disconnect_sync (client,
                                            NULL, /* GCancellable */
                                            &local_error))
        {
          goa_warning ("Error closing connection: %s (%s, %d)",
                       local_error->message, g_quark_to_string (local_error->domain), local_error->code);
          g_error_free (local_error);
        }
      g_object_unref (client);
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
add_star_func (GoaImapClient   *client,
               gpointer         user_data,
               GError         **error)
{
  const gchar *request = user_data;
  gchar *response;
  response = goa_imap_client_run_command_sync (client,
                                               request,
                                               NULL, /* GCancellable */
                                               error);
  if (response == NULL)
    return FALSE;
  g_free (response);
  return TRUE;
}

/* runs in thread dedicated to the method invocation */
static gboolean
monitor_on_handle_add_star (GoaMailMonitor        *monitor,
                            GDBusMethodInvocation *invocation,
                            const gchar           *message_uid_as_str,
                            gpointer               user_data)
{
  MonitorData *data = user_data;
  GError *error;
  guint64 message_uid;
  gchar *request;
  gchar *endp;

  request = NULL;
  error = NULL;

  monitor_data_ref (data);

  if (data->starred_folder == NULL)
    {
      error = g_error_new (GOA_ERROR,
                           GOA_ERROR_FAILED,
                           "Starred folder not found");
      goto out;
    }
  message_uid = strtoll (message_uid_as_str, &endp, 10);
  if (*endp != '\0' || endp == message_uid_as_str)
    {
      error = g_error_new (GOA_ERROR,
                           GOA_ERROR_FAILED,
                           "Given message UID is not valid");
      goto out;
    }

  request = g_strdup_printf ("UID COPY %d %s",
                             (gint) (message_uid & 0xffffffff),
                             data->starred_folder);
  imap_helper (goa_mail_get_imap_host (GOA_MAIL (data->mail)),
               goa_mail_get_imap_use_tls (GOA_MAIL (data->mail)),
               data->mail->imap_ignore_bad_tls,
               data->mail->imap_auth,
               (message_uid >> 32),
               add_star_func,
               request,
               &error);
 out:
  if (error == NULL)
    goa_mail_monitor_complete_add_star (monitor, invocation);
  else
    g_dbus_method_invocation_take_error (invocation, error);
  g_free (request);
  monitor_data_unref (data);
  return TRUE; /* invocation was handled */
}

/* ---------------------------------------------------------------------------------------------------- */


static gboolean
mark_as_spam_func (GoaImapClient   *client,
                   gpointer         user_data,
                   GError         **error)
{
  const gchar **requests = user_data;
  gboolean ret;
  guint n;

  ret = FALSE;
  for (n = 0; requests[n] != NULL; n++)
    {
      gchar *response;
      response = goa_imap_client_run_command_sync (client, requests[n], NULL, error);
      if (response == NULL)
        goto out;
      g_free (response);
    }
  ret = TRUE;

 out:
  return ret;
}

/* runs in thread dedicated to the method invocation */
static gboolean
monitor_on_handle_mark_as_spam (GoaMailMonitor        *monitor,
                                GDBusMethodInvocation *invocation,
                                const gchar           *message_uid_as_str,
                                gpointer               user_data)
{
  MonitorData *data = user_data;
  GError *error;
  guint64 message_uid;
  gchar **requests;
  gchar *endp;

  requests = NULL;
  error = NULL;

  monitor_data_ref (data);

  if (data->spam_folder == NULL)
    {
      error = g_error_new (GOA_ERROR,
                           GOA_ERROR_FAILED,
                           "Spam folder not found");
      goto out;
    }
  message_uid = strtoll (message_uid_as_str, &endp, 10);
  if (*endp != '\0' || endp == message_uid_as_str)
    {
      error = g_error_new (GOA_ERROR,
                           GOA_ERROR_FAILED,
                           "Given message UID is not valid");
      goto out;
    }

  requests = g_new0 (gchar*, 3);
  requests[0] = g_strdup_printf ("UID COPY %d %s",
                                 (gint) (message_uid & 0xffffffff),
                                 data->spam_folder);
  requests[1] = g_strdup_printf ("UID STORE %d +FLAGS (\\Deleted)",
                                 (gint) (message_uid & 0xffffffff));
  imap_helper (goa_mail_get_imap_host (GOA_MAIL (data->mail)),
               goa_mail_get_imap_use_tls (GOA_MAIL (data->mail)),
               data->mail->imap_ignore_bad_tls,
               data->mail->imap_auth,
               (message_uid >> 32),
               mark_as_spam_func,
               requests,
               &error);
 out:
  if (error == NULL)
    goa_mail_monitor_complete_mark_as_spam (monitor, invocation);
  else
    g_dbus_method_invocation_take_error (invocation, error);
  g_strfreev (requests);
  monitor_data_unref (data);
  return TRUE; /* invocation was handled */
}

/* ---------------------------------------------------------------------------------------------------- */

/* runs in thread dedicated to the method invocation */
static gboolean
handle_create_monitor (GoaMail                *_mail,
                       GDBusMethodInvocation  *invocation)
{
  GoaInternetMail *mail = GOA_INTERNET_MAIL (_mail);
  gchar *monitor_object_path;
  GError *error;
  MonitorData *data;
  static gint _g_monitor_count = 0;

  monitor_object_path = NULL;

  goa_info ("Creating mail monitor for %s on account %s",
            g_dbus_method_invocation_get_sender (invocation),
            g_dbus_object_get_object_path (g_dbus_interface_get_object (G_DBUS_INTERFACE (mail))));

  data = g_slice_new0 (MonitorData);
  data->ref_count = 1;
  data->mail = g_object_ref (mail);
  data->monitor = goa_mail_monitor_skeleton_new ();
  /* Be optimistic that the connection works - if this is not so,
   * imap_client_sync() will clear the flag
   */
  goa_mail_monitor_set_connected (data->monitor, TRUE);
  g_dbus_interface_skeleton_set_flags (G_DBUS_INTERFACE_SKELETON (data->monitor),
                                       G_DBUS_INTERFACE_SKELETON_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
  g_signal_connect (data->monitor,
                    "handle-refresh",
                    G_CALLBACK (monitor_on_handle_refresh),
                    data);
  g_signal_connect (data->monitor,
                    "handle-close",
                    G_CALLBACK (monitor_on_handle_close),
                    data);
  g_signal_connect (data->monitor,
                    "handle-add-star",
                    G_CALLBACK (monitor_on_handle_add_star),
                    data);
  g_signal_connect (data->monitor,
                    "handle-mark-as-spam",
                    G_CALLBACK (monitor_on_handle_mark_as_spam),
                    data);
  g_signal_connect (data->monitor,
                    "handle-simulate-message-received",
                    G_CALLBACK (monitor_on_handle_simulate_message_received),
                    data);

  monitor_object_path = g_strdup_printf ("/org/gnome/OnlineAccounts/mail_monitors/%d", _g_monitor_count++);
  error = NULL;
  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (data->monitor),
                                         g_dbus_method_invocation_get_connection (invocation),
                                         monitor_object_path,
                                         &error))
    {
      g_prefix_error (&error, "Error exporting mail monitor: ");
      g_dbus_method_invocation_return_gerror (invocation, error);
      monitor_data_unref (data);
      goto out;
    }

  data->name_watcher_id = g_bus_watch_name_on_connection (g_dbus_method_invocation_get_connection (invocation),
                                                          g_dbus_method_invocation_get_sender (invocation),
                                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                          NULL, /* name_appeared_handler */
                                                          on_monitor_owner_vanished,
                                                          data,
                                                          NULL);

  /* OK, we're in business - finish the invocation
   *
   * TODO: set up things so only caller can access the created object?
   */
  goa_mail_complete_create_monitor (GOA_MAIL (mail), invocation, monitor_object_path);

  /* Create the IMAP client - this blocks our thread until the owner
   * vanishes or the Close() method is called ...
   *
   * The data->imap_cancellable member can be used to wake up the loop
   * to check for data->imap_request_close member or just to
   * refresh...
   */
  data->imap_cancellable = g_cancellable_new ();
  data->imap_counter_lock = g_mutex_new ();
  data->imap_counter_cond = g_cond_new ();
  imap_client_sync (data);

 out:
  g_free (monitor_object_path);
  return TRUE; /* invocation was handled */
}

/* ---------------------------------------------------------------------------------------------------- */

static void
goa_internet_mail__goa_mail_iface_init (GoaMailIface *iface)
{
  iface->handle_create_monitor = handle_create_monitor;
}

/* ---------------------------------------------------------------------------------------------------- */