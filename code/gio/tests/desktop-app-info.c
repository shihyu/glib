/* 
 * Copyright (C) 2008 Red Hat, Inc
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
 * Author: Matthias Clasen
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *basedir;

static GAppInfo * 
create_app_info (const char *name)
{
  GError *error;
  GAppInfo *info;

  error = NULL;
  info = g_app_info_create_from_commandline ("true blah",
                                             name,
                                             G_APP_INFO_CREATE_NONE,
                                             &error);
  g_assert (error == NULL);

  /* this is necessary to ensure that the info is saved */
  g_app_info_set_as_default_for_type (info, "application/x-blah", &error);
  g_assert (error == NULL);
  g_app_info_remove_supports_type (info, "application/x-blah", &error);
  g_assert (error == NULL);
  g_app_info_reset_type_associations ("application/x-blah");
  
  return info;
}

static void
test_delete (void)
{
  GAppInfo *info;

  const char *id;
  char *filename;
  gboolean res;

  info = create_app_info ("Blah");
 
  id = g_app_info_get_id (info);
  g_assert (id != NULL);

  filename = g_build_filename (basedir, "applications", id, NULL);

  res = g_file_test (filename, G_FILE_TEST_EXISTS);
  g_assert (res);

  res = g_app_info_can_delete (info);
  g_assert (res);

  res = g_app_info_delete (info);
  g_assert (res);

  res = g_file_test (filename, G_FILE_TEST_EXISTS);
  g_assert (!res);

  g_object_unref (info);

  if (g_file_test ("/usr/share/applications/gedit.desktop", G_FILE_TEST_EXISTS))
    {
      info = (GAppInfo*)g_desktop_app_info_new_from_filename ("/usr/share/applications/gedit.desktop");
      g_assert (info);
     
      res = g_app_info_can_delete (info);
      g_assert (!res);
 
      res = g_app_info_delete (info);
      g_assert (!res);
    }
}

static void
test_default (void)
{
  GAppInfo *info, *info1, *info2, *info3;
  GList *list;
  GError *error = NULL;  

  info1 = create_app_info ("Blah1");
  info2 = create_app_info ("Blah2");
  info3 = create_app_info ("Blah3");

  g_app_info_set_as_default_for_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  g_app_info_set_as_default_for_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 2);
  
  /* check that both are in the list, info2 before info1 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info1)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now try adding something at the end */
  g_app_info_add_supports_type (info3, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 3);
  
  /* check that all are in the list, info2, info1, info3 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info1)) == 0);

  info = (GAppInfo *)list->next->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info3)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now remove info1 again */
  g_app_info_remove_supports_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 2);
  
  /* check that both are in the list, info2 before info3 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info3)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now clean it all up */
  g_app_info_reset_type_associations ("application/x-test");

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (list == NULL);
  
  g_app_info_delete (info1);
  g_app_info_delete (info2);
  g_app_info_delete (info3);

  g_object_unref (info1);
  g_object_unref (info2);
}

static void
test_fallback (void)
{
  GAppInfo *info1, *info2, *app;
  GList *apps, *recomm, *fallback, *list, *l, *m;
  GError *error = NULL;
  gint old_length;

  info1 = create_app_info ("Test1");
  info2 = create_app_info ("Test2");

  g_assert (g_content_type_is_a ("text/x-python", "text/plain"));

  apps = g_app_info_get_all_for_type ("text/x-python");
  old_length = g_list_length (apps);
  g_list_free_full (apps, g_object_unref);

  g_app_info_set_as_default_for_type (info1, "text/x-python", &error);
  g_assert (error == NULL);

  g_app_info_set_as_default_for_type (info2, "text/plain", &error);
  g_assert (error == NULL);

  /* check that both apps are registered */
  apps = g_app_info_get_all_for_type ("text/x-python");
  g_assert (g_list_length (apps) == old_length + 2);

  /* check the ordering */
  app = g_list_nth_data (apps, 0);
  g_assert (g_app_info_equal (info1, app));

  /* check that Test1 is the first recommended app */
  recomm = g_app_info_get_recommended_for_type ("text/x-python");
  g_assert (recomm != NULL);
  app = g_list_nth_data (recomm, 0);
  g_assert (g_app_info_equal (info1, app));

  /* and that Test2 is the first fallback */
  fallback = g_app_info_get_fallback_for_type ("text/x-python");
  g_assert (fallback != NULL);
  app = g_list_nth_data (fallback, 0);
  g_assert (g_app_info_equal (info2, app));

  /* check that recomm + fallback = all applications */
  list = g_list_concat (g_list_copy (recomm), g_list_copy (fallback));
  g_assert (g_list_length (list) == g_list_length (apps));

  for (l = list, m = apps; l != NULL && m != NULL; l = l->next, m = m->next)
    {
      g_assert (g_app_info_equal (l->data, m->data));
    }

  g_list_free (list);

  g_list_free_full (apps, g_object_unref);
  g_list_free_full (recomm, g_object_unref);
  g_list_free_full (fallback, g_object_unref);

  g_app_info_reset_type_associations ("text/x-python");
  g_app_info_reset_type_associations ("text/plain");

  g_app_info_delete (info1);
  g_app_info_delete (info2);

  g_object_unref (info1);
  g_object_unref (info2);
}

static void
test_last_used (void)
{
  GList *applications;
  GAppInfo *info1, *info2, *default_app;
  GError *error = NULL;

  info1 = create_app_info ("Test1");
  info2 = create_app_info ("Test2");

  g_app_info_set_as_default_for_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  g_app_info_add_supports_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  applications = g_app_info_get_recommended_for_type ("application/x-test");
  g_assert (g_list_length (applications) == 2);

  /* the first should be the default app now */
  g_assert (g_app_info_equal (g_list_nth_data (applications, 0), info1));
  g_assert (g_app_info_equal (g_list_nth_data (applications, 1), info2));

  g_list_free_full (applications, g_object_unref);

  g_app_info_set_as_last_used_for_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  applications = g_app_info_get_recommended_for_type ("application/x-test");
  g_assert (g_list_length (applications) == 2);

  default_app = g_app_info_get_default_for_type ("application/x-test", FALSE);
  g_assert (g_app_info_equal (default_app, info1));

  /* the first should be the other app now */
  g_assert (g_app_info_equal (g_list_nth_data (applications, 0), info2));
  g_assert (g_app_info_equal (g_list_nth_data (applications, 1), info1));

  g_list_free_full (applications, g_object_unref);

  g_app_info_reset_type_associations ("application/x-test");

  g_app_info_delete (info1);
  g_app_info_delete (info2);

  g_object_unref (info1);
  g_object_unref (info2);
  g_object_unref (default_app);
}

static void
cleanup_dir_recurse (GFile *parent, GFile *root)
{
  gboolean res;
  GError *error;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GFile *descend;
  char *relative_path;

  g_assert (root != NULL);

  error = NULL;
  enumerator =
    g_file_enumerate_children (parent, "*",
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
                               &error);
  if (! enumerator)
          return;
  error = NULL;
  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  while ((info) && (!error))
    {
      descend = g_file_get_child (parent, g_file_info_get_name (info));
      g_assert (descend != NULL);
      relative_path = g_file_get_relative_path (root, descend);
      g_assert (relative_path != NULL);

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
          cleanup_dir_recurse (descend, root);

      error = NULL;
      res = g_file_delete (descend, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);

      g_object_unref (descend);
      error = NULL;
      info = g_file_enumerator_next_file (enumerator, NULL, &error);
    }
  g_assert (error == NULL);

  error = NULL;
  res = g_file_enumerator_close (enumerator, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert (error == NULL);
}

static void
cleanup_subdirs (const char *base_dir)
{
  GFile *base, *file;
 
  base = g_file_new_for_path (base_dir);  
  file = g_file_get_child (base, "applications");
  cleanup_dir_recurse (file, file);
  g_object_unref (file);
  file = g_file_get_child (base, "mime");
  cleanup_dir_recurse (file, file);
  g_object_unref (file);
}

static void
test_extra_getters (void)
{
  GDesktopAppInfo *appinfo;
  gchar *s;
  gboolean b;

  appinfo = g_desktop_app_info_new_from_filename (g_test_get_filename (G_TEST_DIST, "appinfo-test.desktop", NULL));
  g_assert (appinfo != NULL);

  g_assert (g_desktop_app_info_has_key (appinfo, "Terminal"));
  g_assert (!g_desktop_app_info_has_key (appinfo, "Bratwurst"));

  s = g_desktop_app_info_get_string (appinfo, "StartupWMClass");
  g_assert_cmpstr (s, ==, "appinfo-class");
  g_free (s);

  b = g_desktop_app_info_get_boolean (appinfo, "Terminal");
  g_assert (b);

  g_object_unref (appinfo);
}

static void
wait_for_file (const gchar *want_this,
               const gchar *but_not_this,
               const gchar *or_this)
{
  gint retries = 600;

  /* I hate time-based conditions in tests, but this will wait up to one
   * whole minute for "touch file" to finish running.  I think it should
   * be OK.
   *
   * 600 * 100ms = 60 seconds.
   */
  while (access (want_this, F_OK) != 0)
    {
      g_usleep (100000); /* 100ms */
      g_assert (retries);
      retries--;
    }

  g_assert (access (but_not_this, F_OK) != 0);
  g_assert (access (or_this, F_OK) != 0);

  unlink (want_this);
  unlink (but_not_this);
  unlink (or_this);
}

static void
test_actions (void)
{
  const gchar * const *actions;
  GDesktopAppInfo *appinfo;
  gchar *name;

  appinfo = g_desktop_app_info_new_from_filename (g_test_get_filename (G_TEST_DIST, "appinfo-test-actions.desktop", NULL));
  g_assert (appinfo != NULL);

  actions = g_desktop_app_info_list_actions (appinfo);
  g_assert_cmpstr (actions[0], ==, "frob");
  g_assert_cmpstr (actions[1], ==, "tweak");
  g_assert_cmpstr (actions[2], ==, "twiddle");
  g_assert_cmpstr (actions[3], ==, "broken");
  g_assert_cmpstr (actions[4], ==, NULL);

  name = g_desktop_app_info_get_action_name (appinfo, "frob");
  g_assert_cmpstr (name, ==, "Frobnicate");
  g_free (name);

  name = g_desktop_app_info_get_action_name (appinfo, "tweak");
  g_assert_cmpstr (name, ==, "Tweak");
  g_free (name);

  name = g_desktop_app_info_get_action_name (appinfo, "twiddle");
  g_assert_cmpstr (name, ==, "Twiddle");
  g_free (name);

  name = g_desktop_app_info_get_action_name (appinfo, "broken");
  g_assert (name != NULL);
  g_assert (g_utf8_validate (name, -1, NULL));
  g_free (name);

  unlink ("frob"); unlink ("tweak"); unlink ("twiddle");

  g_desktop_app_info_launch_action (appinfo, "frob", NULL);
  wait_for_file ("frob", "tweak", "twiddle");

  g_desktop_app_info_launch_action (appinfo, "tweak", NULL);
  wait_for_file ("tweak", "frob", "twiddle");

  g_desktop_app_info_launch_action (appinfo, "twiddle", NULL);
  wait_for_file ("twiddle", "frob", "tweak");

  g_object_unref (appinfo);
}

static gchar *
run_apps (const gchar *command,
          const gchar *arg,
          gboolean     with_usr,
          gboolean     with_home,
          const gchar *locale_name,
          const gchar *language)
{
  gboolean success;
  gchar **envp;
  gchar **argv;
  gint status;
  gchar *out;

  argv = g_new (gchar *, 4);
  argv[0] = g_test_build_filename (G_TEST_BUILT, "apps", NULL);
  argv[1] = g_strdup (command);
  argv[2] = g_strdup (arg);
  argv[3] = NULL;

  envp = g_get_environ ();

  if (with_usr)
    {
      gchar *tmp = g_test_build_filename (G_TEST_DIST, "desktop-files", "usr", NULL);
      envp = g_environ_setenv (envp, "XDG_DATA_DIRS", tmp, TRUE);
      g_free (tmp);
    }
  else
    envp = g_environ_setenv (envp, "XDG_DATA_DIRS", "/does-not-exist", TRUE);

  if (with_home)
    {
      gchar *tmp = g_test_build_filename (G_TEST_DIST, "desktop-files", "home", NULL);
      envp = g_environ_setenv (envp, "XDG_DATA_HOME", tmp, TRUE);
      g_free (tmp);
    }
  else
    envp = g_environ_setenv (envp, "XDG_DATA_HOME", "/does-not-exist", TRUE);

  if (locale_name)
    envp = g_environ_setenv (envp, "LC_ALL", locale_name, TRUE);
  else
    envp = g_environ_setenv (envp, "LC_ALL", "C", TRUE);

  if (language)
    envp = g_environ_setenv (envp, "LANGUAGE", language, TRUE);
  else
    envp = g_environ_unsetenv (envp, "LANGUAGE");

  success = g_spawn_sync (NULL, argv, envp, 0, NULL, NULL, &out, NULL, &status, NULL);
  g_assert (success);
  g_assert (status == 0);

  g_strfreev (envp);
  g_strfreev (argv);

  return out;
}

static void
assert_strings_equivalent (const gchar *expected,
                           const gchar *result)
{
  gchar **expected_words;
  gchar **result_words;
  gint i, j;

  expected_words = g_strsplit (expected, " ", 0);
  result_words = g_strsplit (result, " ", 0);

  for (i = 0; expected_words[i]; i++)
    {
      for (j = 0; result_words[j]; j++)
        if (g_str_equal (expected_words[i], result_words[j]))
          goto got_it;

      g_test_message ("Unable to find expected string '%s' in result '%s'", expected_words[i], result);
      g_test_fail ();

got_it:
      continue;
    }

  g_assert_cmpint (g_strv_length (expected_words), ==, g_strv_length (result_words));
  g_strfreev (expected_words);
  g_strfreev (result_words);
}

static void
assert_list (const gchar *expected,
             gboolean     with_usr,
             gboolean     with_home,
             const gchar *locale_name,
             const gchar *language)
{
  gchar *result;

  result = run_apps ("list", NULL, with_usr, with_home, locale_name, language);
  g_strchomp (result);
  assert_strings_equivalent (expected, result);
  g_free (result);
}

static void
assert_info (const gchar *desktop_id,
             const gchar *expected,
             gboolean     with_usr,
             gboolean     with_home,
             const gchar *locale_name,
             const gchar *language)
{
  gchar *result;

  result = run_apps ("show-info", desktop_id, with_usr, with_home, locale_name, language);
  g_assert_cmpstr (result, ==, expected);
  g_free (result);
}

static void
assert_search (const gchar *search_string,
               const gchar *expected,
               gboolean     with_usr,
               gboolean     with_home,
               const gchar *locale_name,
               const gchar *language)
{
  gchar **expected_lines;
  gchar **result_lines;
  gchar *result;
  gint i;

  expected_lines = g_strsplit (expected, "\n", -1);
  result = run_apps ("search", search_string, with_usr, with_home, locale_name, language);
  result_lines = g_strsplit (result, "\n", -1);
  g_assert_cmpint (g_strv_length (expected_lines), ==, g_strv_length (result_lines));
  for (i = 0; expected_lines[i]; i++)
    assert_strings_equivalent (expected_lines[i], result_lines[i]);
  g_strfreev (expected_lines);
  g_strfreev (result_lines);
  g_free (result);
}

#define ALL_USR_APPS  "evince-previewer.desktop nautilus-classic.desktop gnome-font-viewer.desktop "         \
                      "baobab.desktop yelp.desktop eog.desktop cheese.desktop gnome-clocks.desktop "         \
                      "gnome-contacts.desktop kde4-kate.desktop gcr-prompter.desktop totem.desktop "         \
                      "gnome-terminal.desktop nautilus-autorun-software.desktop gcr-viewer.desktop "         \
                      "nautilus-connect-server.desktop kde4-dolphin.desktop gnome-music.desktop "            \
                      "kde4-konqbrowser.desktop gucharmap.desktop kde4-okular.desktop nautilus.desktop "     \
                      "gedit.desktop evince.desktop file-roller.desktop dconf-editor.desktop glade.desktop"
#define HOME_APPS     "epiphany-weather-for-toronto-island-9c6a4e022b17686306243dada811d550d25eb1fb.desktop"
#define ALL_HOME_APPS HOME_APPS " eog.desktop"

static void
test_search (void)
{
  assert_list ("", FALSE, FALSE, NULL, NULL);
  assert_list (ALL_USR_APPS, TRUE, FALSE, NULL, NULL);
  assert_list (ALL_HOME_APPS, FALSE, TRUE, NULL, NULL);
  assert_list (ALL_USR_APPS " " HOME_APPS, TRUE, TRUE, NULL, NULL);

  /* The user has "installed" their own version of eog.desktop which
   * calls it "Eye of GNOME".  Do some testing based on that.
   *
   * We should always find "Pictures" keyword no matter where we look.
   */
  assert_search ("Picture", "eog.desktop\n", TRUE, TRUE, NULL, NULL);
  assert_search ("Picture", "eog.desktop\n", TRUE, FALSE, NULL, NULL);
  assert_search ("Picture", "eog.desktop\n", FALSE, TRUE, NULL, NULL);
  assert_search ("Picture", "", FALSE, FALSE, NULL, NULL);

  /* We should only find it called "eye of gnome" when using the user's
   * directory.
   */
  assert_search ("eye gnome", "", TRUE, FALSE, NULL, NULL);
  assert_search ("eye gnome", "eog.desktop\n", FALSE, TRUE, NULL, NULL);
  assert_search ("eye gnome", "eog.desktop\n", TRUE, TRUE, NULL, NULL);

  /* We should only find it called "image viewer" when _not_ using the
   * user's directory.
   */
  assert_search ("image viewer", "eog.desktop\n", TRUE, FALSE, NULL, NULL);
  assert_search ("image viewer", "", FALSE, TRUE, NULL, NULL);
  assert_search ("image viewer", "", TRUE, TRUE, NULL, NULL);

  /* Obvious multi-word search */
  assert_search ("gno hel", "yelp.desktop\n", TRUE, TRUE, NULL, NULL);

  /* Repeated search terms should do nothing... */
  assert_search ("files file fil fi f", "nautilus.desktop\n"
                                        "gedit.desktop\n", TRUE, TRUE, NULL, NULL);

  /* "con" will match "connect" and "contacts" on name but dconf only on
   * the "config" keyword
   */
  assert_search ("con", "nautilus-connect-server.desktop gnome-contacts.desktop\n"
                        "dconf-editor.desktop\n", TRUE, TRUE, NULL, NULL);

  /* "gnome" will match "eye of gnome" from the user's directory, plus
   * matching "GNOME Clocks" X-GNOME-FullName.  It's only a comment on
   * yelp and gnome-contacts, though.
   */
  assert_search ("gnome", "eog.desktop\n"
                          "gnome-clocks.desktop\n"
                          "yelp.desktop gnome-contacts.desktop\n", TRUE, TRUE, NULL, NULL);

  /* "gnome con" will match only gnome contacts; via the name for
   * "contacts" and the comment for "gnome"
   */
  assert_search ("gnome con", "gnome-contacts.desktop\n", TRUE, TRUE, NULL, NULL);

  /* make sure we get the correct kde4- prefix on the application IDs
   * from subdirectories
   */
  assert_search ("konq", "kde4-konqbrowser.desktop\n", TRUE, TRUE, NULL, NULL);
  assert_search ("kate", "kde4-kate.desktop\n", TRUE, TRUE, NULL, NULL);

  /* make sure we can lookup apps by name properly */
  assert_info ("kde4-kate.desktop",
               "kde4-kate.desktop\n"
               "Kate\n"
               "Kate\n"
               "nil\n", TRUE, TRUE, NULL, NULL);

  assert_info ("nautilus.desktop",
               "nautilus.desktop\n"
               "Files\n"
               "Files\n"
               "Access and organize files\n", TRUE, TRUE, NULL, NULL);

  /* make sure localised searching works properly */
  assert_search ("foliumi", "nautilus.desktop\n"
                            "kde4-konqbrowser.desktop\n"
                            "eog.desktop\n", TRUE, FALSE, "en_US.UTF-8", "eo");
  /* the user's eog.desktop has no translations... */
  assert_search ("foliumi", "nautilus.desktop\n"
                            "kde4-konqbrowser.desktop\n", TRUE, TRUE, "en_US.UTF-8", "eo");
}

int
main (int   argc,
      char *argv[])
{
  gint result;

  g_test_init (&argc, &argv, NULL);

  basedir = g_get_current_dir ();
  g_setenv ("XDG_DATA_HOME", basedir, TRUE);
  cleanup_subdirs (basedir);

  g_test_add_func ("/desktop-app-info/delete", test_delete);
  g_test_add_func ("/desktop-app-info/default", test_default);
  g_test_add_func ("/desktop-app-info/fallback", test_fallback);
  g_test_add_func ("/desktop-app-info/lastused", test_last_used);
  g_test_add_func ("/desktop-app-info/extra-getters", test_extra_getters);
  g_test_add_func ("/desktop-app-info/actions", test_actions);
  g_test_add_func ("/desktop-app-info/search", test_search);

  result = g_test_run ();

  cleanup_subdirs (basedir);

  return result;
}
