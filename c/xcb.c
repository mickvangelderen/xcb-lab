#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <xcb/xcb.h>

#include <X11/Xlib.h>

double
get_time(void)
{
  struct timeval timev;

  gettimeofday(&timev, NULL);

  return (double)timev.tv_sec + (((double)timev.tv_usec) / 1000000);
}

int
main ()
{
  xcb_connection_t         *c;
  xcb_atom_t               *atoms;
  xcb_intern_atom_cookie_t *cookies;
  char                    **names;
  int                       count;
  int                       i;
  double                    start;
  double                    end;
  double                    diff;

  c = xcb_connect (NULL, NULL);

  count = 500;
  atoms = (xcb_atom_t *)malloc (count * sizeof (atoms));
  names = (char **)malloc (count * sizeof (char *));

  /* init names */
  for (i = 0; i < count; ++i) {
    char buf[100];

    sprintf (buf, "NAME%d", i);
    names[i] = strdup (buf);
  }

  /* good use */
  start = get_time ();

  cookies = (xcb_intern_atom_cookie_t *) malloc (count * sizeof(xcb_intern_atom_cookie_t));
  for(i = 0; i < count; ++i)
    cookies[i] = xcb_intern_atom (c, 0, strlen(names[i]), names[i]);

  for(i = 0; i < count; ++i) {
    xcb_intern_atom_reply_t *r;

    r = xcb_intern_atom_reply(c, cookies[i], 0);
    if(r)
      atoms[i] = r->atom;
    free(r);
  }

  end = get_time ();
  printf ("good use time : %f\n", end - start);
  printf ("ratio         : %f\n", diff / (end - start));
  diff = end - start;

  /* free var */
  free (atoms);
  free (cookies);

  xcb_disconnect (c);

  return 0;
}
