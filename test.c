#include "out.h"
#include <stdio.h>

typedef struct
{
  uint8_t *mem;
  uint8_t *end;
  uint8_t *p;
} buf_t;

int w (const uint8_t *bytes, size_t n, void *q)
{
  buf_t *b = q;
  if ((b->p + n) >= b->end)
    return -1;
  while (n--)
    *b->p++ = *bytes++;
  return 0;
}

int r (uint8_t *bytes, size_t n, void *q)
{
  buf_t *b = q;
  if ((b->p + n) >= b->end)
    return -1;
  while (n--)
    *bytes++ = *b->p++;
  return 0;
}


int main (int argc, char *argv[])
{
  (void)argc; (void)argv;

  int16_t stuff[3] = { 0x9876, 0xf0f0, 0x0000 };
  const foo f = { 12, "this is a test!", { &stuff[0], &stuff[1], &stuff[2]}, "short string" };
  printf ("a: %x\nb: %s\nmc[0]: %hx\nmc[1]: %hx\nmc[2]: %hx\nmd: %s\n",
    f.a, f.b, *f.mc[0], *f.mc[1], *f.mc[2], f.md);

  uint8_t space[256] = { 0 };
  buf_t buf = { space, space + sizeof (space), space };
  printf ("store: %d\n", cser_raw_store_foo (&f, w, &buf));

  FILE *o = fopen ("test.dmp", "w");
  fwrite (buf.mem, buf.end - buf.mem, 1, o);
  fclose (o);

  buf.p = buf.mem;

  foo f2;
  printf ("load: %d\n", cser_raw_load_foo (&f2, r, &buf));

  printf ("a: %x\nb: %s\nmc[0]: %hx\nmc[1]: %hx\nmc[2]: %hx\nmd: %s\n",
    f2.a, f2.b, *f2.mc[0], *f2.mc[1], *f2.mc[2], f2.md);

  return 0;
}
