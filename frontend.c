/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/

#include "frontend.h"
#include "c11_parser.h"
#include "model.h"
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

//
// Purely parsing support
//

typedef struct name_list
{
  char *name;
  struct name_list *next;
} name_list_t;

static name_list_t *enum_constants;
static name_list_t *typedef_names;

static bool is_enum_constant (const char *name)
{
  name_list_t *p = enum_constants;
  for (; p; p = p->next)
  {
    if (strcmp (name, p->name) == 0)
      return true;
  }
  return false;
}

static bool is_typedef_name (const char *name)
{
  name_list_t *p = typedef_names;
  for (; p; p = p->next)
  {
    if (strcmp (name, p->name) == 0)
      return true;
  }
  return false;
}

void add_enum_constant (const char *name)
{
  name_list_t *p = malloc (sizeof (name_list_t));
  p->name = strdup (name);
  p->next = enum_constants;
  enum_constants = p;
}

extern int yylineno;
void add_typedef_name (const char *name)
{
  name_list_t *p = malloc (sizeof (name_list_t));
  p->name = strdup (name);
  p->next = typedef_names;
  typedef_names = p;
}

int sym_type (const char *name)
{
  if (is_typedef_name (name))
    return TYPEDEF_NAME;
  else if (is_enum_constant (name))
    return ENUMERATION_CONSTANT;
  else
    return IDENTIFIER;
}


//
// Acting on the information provided from the parsing
//

static parse_info_t base_info;
parse_info_t *info = &base_info;


static void free_info_node (parse_info_t *nfo)
{
  free (nfo->base_type);
  free (nfo->name);
  free (nfo->arr_sz);
  free (nfo);
}


static bool is_undecorated (void)
{
  return (!info->arr_sz && !info->ptr);
}


static void merge_decorations (decorations_t *dst, const decorations_t *src, parse_info_t *i)
{
  dst->is_ptr = i->ptr + src->is_ptr;


  bool one_dim =
    (src->cardinality == CDN_SINGLE || !i->arr_sz);
  bool two_dim =
    (src->cardinality == CDN_FIXED_ARRAY && i->arr_sz);
  if (!one_dim && !two_dim)
  {
    char *err;
    if (asprintf (&err,"unable to combine arrays for types '%s' and '%s'\n",
      i->base_type, i->name) < 0)
        yyerror ("out of memory");
    yyerror (err);
  }
  if (one_dim)
  {
    dst->cardinality =
      (!i->arr_sz && src->cardinality == CDN_SINGLE) ?
        CDN_SINGLE : CDN_FIXED_ARRAY;
    if (i->arr_sz)
      dst->arr_sz = strdup (i->arr_sz);
    else if (src->arr_sz)
      dst->arr_sz = strdup (src->arr_sz);
  }
  else // two_dim
  {
    dst->cardinality = CDN_FIXED_ARRAY;
    if (asprintf (&dst->arr_sz, "(%s)*(%s)", i->arr_sz, src->arr_sz) < 0)
      yyerror ("out of memory");
  }
}


static void do_lookup (char **lookup_name, const type_t **t)
{
  *lookup_name = strdup (info->base_type);

  *t = lookup_type (*lookup_name);
  bool ph = has_placeholder (*lookup_name);
  if (!*t && !ph)
  {
    char *err;
    if (asprintf (&err, "unrecognised type '%s' for '%s'\n",
      *lookup_name, info->name) < 0)
        yyerror ("out of memory");
    yyerror (err);
  }
}


static void mark_char_zeroterm (const char *base_type, decorations_t *d)
{
  if (d->is_ptr == 1 &&
      d->cardinality == CDN_SINGLE &&
      strcmp (base_type, "char") == 0)
    d->cardinality = CDN_ZEROTERM_ARRAY;
}


typedef struct member_list
{
  member_t *member;

  struct member_list *next;
} member_list_t;

static member_list_t *member_scope;


static int capturing;
void capture (bool expect_members)
{
  ++capturing;
  if (expect_members)
  {
    parse_info_t *pinfo = calloc (1, sizeof (parse_info_t));
    pinfo->next = info;
    info = pinfo;

    member_list_t *mems = calloc (1, sizeof (member_list_t));
    mems->next = member_scope;
    member_scope = mems;
  }
}


static type_list_t *unnamed_struct;
void capture_member (void)
{
  if (!member_scope)
    yyerror ("nowhere to capture member to");

  if (info->omit)
  {
    reset_info ();
    return;
  }

  if (!info->base_type)
  {
    fprintf (stderr, "warning: ignoring unsupported member on line %d\n", yylineno);
    reset_info ();
    return;
  }

  char *lookup_name;
  const type_t *t;
  do_lookup (&lookup_name, &t);

  member_t *m = calloc (1, sizeof (member_t));
  m->next = member_scope->member;

  if (info->name)
    m->member_name = strdup (info->name);
  else // unnamed bit fields
  {
    static size_t unnamed;
    if (asprintf (&m->member_name, "__unnamed_bitfield_%zu", ++unnamed) < 0)
      yyerror ("out of memory");
  }
  
  if (t)
    m->base_type = strdup (
      (t->csfn == TYPE_DECORATED) ? t->decorated.base_type : t->type_name);
  else
    m->base_type = strdup (info->base_type);

  static const decorations_t no_opts;
  merge_decorations (
    &m->opts,
    (t && t->csfn == TYPE_DECORATED) ?
      &t->decorated.opts : &no_opts, info);

  if (!info->array_def)
    mark_char_zeroterm (m->base_type, &m->opts);
  else
  {
    if (!info->ptr)
      yyerror ("pragma can only apply to pointer type");
    if (*info->array_def == '0')
      m->opts.cardinality = CDN_SINGLE;
    else if (*info->array_def == '1')
      m->opts.cardinality = CDN_ZEROTERM_ARRAY;
    else
    {
      m->opts.cardinality = CDN_VAR_ARRAY;
      bool found = false;
      for (member_t *as = member_scope->member; as; as = as->next)
      {
        if (strcmp (as->member_name, info->array_def) == 0)
        {
          found = true;
          break;
        }
      }
      if (!found)
        yyerror ("specified variable array size member not found");
      m->opts.variable_array_size_member = strdup (info->array_def);
    }
  }

  member_scope->member = m;

  reset_info ();
}


void end_capture (bool end_of_members)
{
  --capturing;

  type_list_t *nt = calloc (1, sizeof (type_list_t));
  type_t *new_type = &nt->def;

  if (end_of_members)
  {
    char *name = info->name ? strdup (info->name) : 0;

    parse_info_t *pinfo = info;
    info = info->next;
    free_info_node (pinfo);

    new_type->csfn = TYPE_COMPOSITE;

    member_list_t *mems = member_scope;
    member_scope = member_scope->next;
    // Yoink all the members, reversing the order so they end up correctly
    for (member_t *m = mems->member; m; )
    {
      member_t *yoinked = m;
      m = m->next;
      yoinked->next = new_type->composite;
      new_type->composite = yoinked;
    }
    free (mems);

    if (name)
      new_type->type_name = name;
    else
    {
      unnamed_struct = nt;
      return;
    }
  }
  else
  {
    char *lookup_name = 0;
    const type_t *t;
    if (unnamed_struct && info->base_type)
    {
      fprintf (stderr, "warning: ignoring unmentionable struct/union on line %d\n", yylineno);
      free (unnamed_struct);
      unnamed_struct = 0;
    }

    if (unnamed_struct)
    {
      if (!is_undecorated ())
        yyerror ("typedefs to unnamed struct pointers not supported");
      t = &unnamed_struct->def;
      unnamed_struct->def.type_name = strdup (info->name);
      add_type (unnamed_struct);
      unnamed_struct = 0;
      free (nt);
      return;
    }
    else
       do_lookup (&lookup_name, &t);

    if (unnamed_struct)
      yyerror ("dangling unnamed struct");

    if (is_undecorated ())
    {
      // just alias the base type
      alias_list_t *alias = calloc (1, sizeof (alias_list_t));
      alias->alias_name = strdup (info->name);
      alias->actual_name = lookup_name;
      add_alias (alias);
      return;
    }
    else switch (t->csfn)
    {
      case TYPE_COMPOSITE:
      case TYPE_NATIVE:
        new_type->csfn = TYPE_DECORATED;
        new_type->decorated.base_type = t->type_name;
        new_type->decorated.opts.is_ptr = info->ptr;
        new_type->decorated.opts.cardinality =
          info->arr_sz ? CDN_FIXED_ARRAY : CDN_SINGLE;
        new_type->decorated.opts.arr_sz = info->arr_sz;
        //new_type->decorated.variable_array_size_member = info->???

        break;
      case TYPE_DECORATED:
        new_type->csfn = TYPE_DECORATED;
        new_type->decorated.base_type = t->decorated.base_type;
        merge_decorations (
          &new_type->decorated.opts, &t->decorated.opts, info);
        break;
    }

    // TODO: add support for general zeroterm/var (via _Pragma)
    mark_char_zeroterm (new_type->decorated.base_type, &new_type->decorated.opts);

    new_type->type_name = strdup (info->name);
  }
  add_type (nt);
}


void reset_info (void)
{
  free (info->base_type);
  free (info->name);
  free (info->arr_sz);

  parse_info_t *next = info->next;
  memset (info, 0, sizeof (*info));
  info->next = next;
}


void note_array_size (const char *arr_str)
{
  if (capturing)
  {
    if (info->arr_sz)
    {
      char *new_arr_sz;
      if (asprintf (&new_arr_sz, "(%s)*(%s)", info->arr_sz, arr_str) < 0)
        yyerror ("out of memory");
      free (info->arr_sz);
      info->arr_sz = new_arr_sz;
    }
    else
      info->arr_sz = strdup (arr_str);
  }
}


void note_pointer (void)
{
  if (capturing)
    ++info->ptr;
}


void set_type (const char *base_type)
{
  if (capturing)
  {
    if (info->base_type)
      fprintf (stderr, "warning: changing basetype from '%s' to '%s'\n", info->base_type, base_type);

    free (info->base_type);
    if (base_type)
      info->base_type = strdup (base_type);
  }
}


void set_name (const char *name)
{
  if (capturing)
  {
    info->name = strdup (name);
  }
}


void handle_pragma (const char *prag)
{
  if (!capturing)
    return;

  if (*prag == '"') // if we get a raw _Pragma("string here")
    ++prag;

  if (strncmp (prag, "cser ", 5) != 0)
    return; // not for us

  prag += 5;
  if (strcmp (prag, "single") == 0)
    info->array_def = strdup ("0");
  else if (strcmp (prag, "zeroterm") == 0)
    info->array_def = strdup ("1");
  else if (strncmp (prag, "vararray:", 9) == 0)
    info->array_def = strdup (prag + 9);
  else if (strcmp (prag, "omit") == 0)
    info->omit = true;
  else if (strcmp (prag, "emit") == 0)
    info->omit = false;

  // ensure we omit any trailing "
  for (char *p = info->array_def; p && *p; ++p)
    if (*p == '"')
      *p = 0;
}

//
// Struct names use a placeholder until they've been fully defined
//

typedef struct placeholder
{
  const char *name;
  struct placeholder *next;
} placeholder_t;

static placeholder_t *placeholders;
  
void add_placeholder (const char *name)
{   
  struct placeholder *ph = calloc (1, sizeof (struct placeholder));
  ph->name = strdup (name);
  ph->next = placeholders;
  placeholders = ph;
}

bool has_placeholder (const char *name)
{
  for (struct placeholder *ph = placeholders; ph; ph = ph->next)
    if (strcmp (ph->name, name) == 0)
      return true;
  return false;
}

