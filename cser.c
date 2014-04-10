/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/

#include "model.h"
#include "frontend.h"
#include "c11_parser.h"
#include "backend_raw.h"
#include "backend_xml.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#define VERSION "1.0.0"

static type_list_t *types;
static alias_list_t *aliases;

static int verbose;

static void print_decs (const decorations_t *d)
{
  for (size_t i = 0; i < d->is_ptr; ++i)
    printf ("*");
  switch (d->cardinality)
  {
    case CDN_SINGLE: break;
    case CDN_FIXED_ARRAY: printf ("[%s]", d->arr_sz); break;
    case CDN_VAR_ARRAY: printf (" /*vararray:%s*/", d->variable_array_size_member); break;
    case CDN_ZEROTERM_ARRAY: printf (" /*zeroterm*/"); break;
  }
}


static void print_type (const type_t *t)
{
  switch (t->csfn)
  {
    case TYPE_NATIVE:
      printf ("%s /* native */", t->type_name);
      break;
    case TYPE_DECORATED:
      printf ("typedef %s ", t->decorated.base_type);
      print_decs (&t->decorated.opts);
      printf (" %s", t->type_name);
      break;
    case TYPE_COMPOSITE:
    {
      printf ("typedef struct {\n");
      for (member_t *m = t->composite; m; m = m->next)
      {
        printf ("  %s", m->base_type);
        print_decs (&m->opts);
        printf (" %s;\n", m->member_name);
      }
      printf ("} %s", t->type_name);
      break;
    }
    default:
      printf ("<ERROR>");
  }
  printf (";\n\n");
}


void add_type (type_list_t *t)
{
  t->next = types;
  types = t;

  if (verbose)
    print_type (&t->def);
}


void add_alias (alias_list_t *a)
{
  a->next = aliases;
  aliases = a;

  if (verbose)
    printf ("typedef %s %s;\n", a->actual_name, a->alias_name);
}


const type_t *lookup_type (const char *type_name)
{
  for (type_list_t *t = types; t; t = t->next)
    if (strcmp (type_name, t->def.type_name) == 0)
      return &t->def;

  for (alias_list_t *a = aliases; a; a = a->next)
    if (strcmp (type_name, a->alias_name) == 0)
      return lookup_type (a->actual_name);

  return 0;
}


char *make_cname (const char *name)
{
  char *underscored = strdup (name);
  for (char *p = underscored; *p; ++p)
    if (*p == ' ')
      *p = '_';
  return underscored;
}


static void init_builtin_types (void)
{
  static type_list_t builtins[] =
  {
    { .def = { .type_name = "void",               .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "_Bool",              .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "char",               .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed char",        .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned char",      .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "short",              .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed short",       .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned short",     .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "short int",          .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed short int",   .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned short int", .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "short signed int",   .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "short unsigned int", .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "int",                .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed",             .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned",           .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed int",         .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned int",       .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long",               .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed long",        .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned long",      .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long int",           .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed long int",    .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned long int",  .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long signed int",    .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long unsigned int",  .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long long",          .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long long int",      .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "signed long long int",   .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "unsigned long long int", .csfn = TYPE_NATIVE, }},

    { .def = { .type_name = "float",    .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "double",   .csfn = TYPE_NATIVE, }},
    { .def = { .type_name = "long double",.csfn = TYPE_NATIVE, }},
  };

  for (size_t i = 0; i < sizeof (builtins) / sizeof (builtins[0]); ++i)
    add_type (&builtins[i]);

  static alias_list_t quirk = { "__builtin_va_list", "void", 0, 0 };
  add_typedef_name ("__builtin_va_list");
  add_alias (&quirk);
}


static void mark_used (const char *type_name)
{
  for (type_list_t *t = types; t; t = t->next)
    if (strcmp (type_name, t->def.type_name) == 0)
    {
      if (t->used)
        return;
      t->used = true;

      if (t->def.csfn == TYPE_DECORATED && t->def.decorated.opts.is_ptr > 1)
      {
        fprintf (stderr, "error: unsupported pointer level %zu for type '%s'\n", t->def.decorated.opts.is_ptr, type_name);
        exit (3);
      }

      // descend as needed
      switch (t->def.csfn)
      {
        case TYPE_NATIVE: break;
        case TYPE_DECORATED: mark_used (t->def.decorated.base_type); break;
        case TYPE_COMPOSITE:
          for (member_t *m = t->def.composite; m; m = m->next)
            mark_used (m->base_type);
          break;
      }
      return;
    }

  for (alias_list_t *a = aliases; a; a = a->next)
    if (strcmp (type_name, a->alias_name) == 0)
    {
      if (a->used)
        return;
      a->used = true;
      mark_used (a->actual_name);
      return;
    }

  fprintf (stderr, "internal error: failed to mark '%s' as used\n", type_name);
  exit (2);
}

// FIXME: remove
#include "test.h"


typedef struct include_list
{
  char *fname;
  struct include_list *next;
} include_list_t;


void syntax (const char *name)
{
  fprintf (stderr, "cser v%s\n", VERSION);
  fprintf (stderr, "Syntax: %s [-v] [-o <basename>] [[-b <backend>]...] <type...>\n", name);
  fprintf (stderr, "  available backends:\n");
  fprintf (stderr, "    raw     binary format (default)\n");
  fprintf (stderr, "\n");
  exit (1);
}

#define BACKEND_RAW  0x01
#define BACKEND_XML  0x02

int main (int argc, char *argv[])
{
  init_builtin_types ();

  include_list_t *includes = 0;
  const char *basename = "out";

  int backends = 0;

  int opt;
  while ((opt = getopt (argc, argv, "hvo:i:b:")) != -1)
  {
    switch (opt)
    {
      case 'h': syntax (argv[0]);
      // -E => load up some extra types and try to parse without preprocessing?
      case 'v': ++verbose; break;
      case 'o': basename = optarg; break;
      case 'i':
      {
        include_list_t *inc = calloc (1, sizeof (include_list_t));
        inc->fname = optarg;
        include_list_t **i = &includes;
        while (*i)
          i = &(*i)->next;
        *i = inc;
        break;
      }
      case 'b':
        if (strcmp ("raw", optarg) == 0) { backends |= BACKEND_RAW; break; }
        if (strcmp ("xml", optarg) == 0) { backends |= BACKEND_XML; break; }
        // fall through
      default:
        syntax (argv[0]);
    }
  }
  if (optind >= argc)
  {
    fprintf (stderr, "error: no types specified\n");
    return 9;
  }


  yyparse ();


  // work out which types we need
  while (optind < argc)
  {
    const type_t *t = lookup_type (argv[optind]);
    if (!t)
    {
      fprintf (stderr, "error: type '%s' not found\n", argv[optind]);
      return 4;
    }
    if (t->csfn != TYPE_COMPOSITE)
    {
      fprintf (stderr, "error: type '%s' is not a struct\n", argv[optind]);
      return 5;
    }
    mark_used (argv[optind++]);
  }

  // discard unused types & aliases
  type_list_t **t = &types;
  while (*t)
  {
    if (!(*t)->used)
    {
      type_list_t *next = (*t)->next;
      //free (*t); // we can't free as some of these are static, not malloc'd
      *t = next;
    }
    else
      t = &(*t)->next;
  }
  alias_list_t **a = &aliases;
  while (*a)
  {
    if (!(*a)->used)
    {
      alias_list_t *next = (*a)->next;
      //free (*a); // we can't free as some of these are static. not malloc'd
      *a = next;
    }
    else
      a = &(*a)->next;
  }

  // start writing the output
  char *h, *c;
  if (asprintf(&h, "%s.h", basename) < 0 || asprintf(&c, "%s.c", basename) < 0)
    return 2;

  FILE *fh = fopen (h, "w");
  FILE *fc = fopen (c, "w");
  if (!fh || !fc)
  {
    perror ("fopen");
    return 3;
  }

  // TODO: write out autogen message to fc+fh
  fprintf (fh, "#ifndef _%s_h_\n#define _%s_h_\n", basename, basename);
  fprintf (fc, "#include \"%s\"\n", h);

  for (include_list_t *i = includes; i; i = i->next)
    fprintf (fh, "#include \"%s\"\n", i->fname);


  if (!backends)
    backends = BACKEND_RAW;

  // invoke chosen backend(s)
  if (backends & BACKEND_RAW)
    backend_raw (types, aliases, fh, fc);
  if (backends & BACKEND_XML)
    backend_xml (types, aliases, fh, fc);


  fprintf (fh, "#endif\n");


  int ret = 0;
  if (ferror (fh))
  {
    fprintf (stderr, "error: writing to '%s' failed\n", h);
    ret = 6;
  }
  if (ferror (fc))
  {
    fprintf (stderr, "error: writing to '%s' failed\n", c);
    ret = 7;
  }

  fclose (fh);
  fclose (fc);

  return ret;
}

