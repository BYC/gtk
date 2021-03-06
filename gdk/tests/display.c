#include <stdlib.h>

#include <gdk/gdk.h>

static void
test_unset_display (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      GdkDisplayManager *manager;

      g_unsetenv ("DISPLAY");

      g_assert (!gdk_init_check (NULL, NULL));
      manager = gdk_display_manager_get ();
      g_assert (manager != NULL);
      g_assert (gdk_display_manager_get_default_display (manager) == NULL);

      exit (0);
    }
  g_test_trap_assert_passed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      g_unsetenv ("DISPLAY");

      gdk_init (NULL, NULL);

      exit (0);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*cannot open display*");
}

static void
test_bad_display (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      GdkDisplayManager *manager;

      g_setenv ("DISPLAY", "poo", TRUE);

      g_assert (!gdk_init_check (NULL, NULL));
      manager = gdk_display_manager_get ();
      g_assert (manager != NULL);
      g_assert (gdk_display_manager_get_default_display (manager) == NULL);

      exit (0);
    }
  g_test_trap_assert_passed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      g_setenv ("DISPLAY", "poo", TRUE);

      gdk_init (NULL, NULL);

      exit (0);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*cannot open display*");
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/display/unset-display", test_unset_display);
  g_test_add_func ("/display/bad-display", test_bad_display);

  return g_test_run ();
}
