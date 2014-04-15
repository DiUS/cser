/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/

#include "backend_xml.h"
#include <string.h>
#include <stdlib.h>


static bool write_store_native (const type_t *type, FILE *fh, FILE *fc)
{
  if (strstr (type->type_name, "float") ||
      strstr (type->type_name, "double"))
  {
    fprintf (stderr, "error: backend_xml does not currently support floating types\n");
    return false;
  }

  char *utype = make_cname (type->type_name);

  fprintf (fh,
    "bool cser_xml_store_%s (const %s *val, void *ctx);\n",
    utype, type->type_name
    );
  fprintf (fc,
    "bool cser_xml_store_%s (const %s *val, void *ctx)\n"
    "{\n",
    utype, type->type_name
    );

  bool unsign = strstr (type->type_name, "unsigned");
  fprintf (fc,
    "  char *str;\n"
    "  if (asprintf (&str, \"%%ll%s\", (%s long long)*val) < 0)\n"
    "    return false;\n"
    "  if (!cser_xml_setvalue (str, ctx))\n"
    "    return false;\n"
    "  free (str);\n",
    unsign ? "u" : "d", unsign ? "unsigned" : ""
    );

  fputs ("  return true;\n}\n\n", fc);

  free (utype);

  return true;
}


static bool write_load_native (const type_t *type, FILE *fh, FILE *fc)
{
  if (strstr (type->type_name, "float") ||
      strstr (type->type_name, "double"))
  {
    fprintf (stderr, "error: backend_xml does not currently support floating types\n");
    return false;
  }

  char *utype = make_cname (type->type_name);
  fprintf (fh,
    "bool cser_xml_load_%s (%s *val, void *ctx);\n",
    utype, type->type_name
    );
  fprintf (fc,
    "bool cser_xml_load_%s (%s *val, void *ctx)\n"
    "{\n",
    utype, type->type_name
    );

  fprintf (fc,
    "  char *str = cser_xml_getvalue (ctx);\n"
    "  if (!str)\n"
    "    return false;\n"
    "  %s tmp = (%s)strto%sll (str, 0, 0);\n"
    "  free (str);\n"
    "  *val = (%s)tmp;\n",
    type->type_name, type->type_name,
      strstr (type->type_name, "unsigned") ? "u" : "",
    type->type_name
    );

  fputs ("return true;\n}\n\n", fc);

  free (utype);

  return true;
}


static bool is_string (const member_t *m)
{
  return
    m->opts.is_ptr &&
    m->opts.cardinality == CDN_ZEROTERM_ARRAY &&
    strcmp (m->base_type, "char") == 0;
}


static void write_store_member_item (const member_t *m, FILE *fc)
{
  fputs ("  {\n", fc);

  char *utype = make_cname (m->base_type);
  bool use_idx = (m->opts.cardinality != CDN_SINGLE);
  bool fixed_size =
    (m->opts.cardinality == CDN_SINGLE ||
     m->opts.cardinality == CDN_FIXED_ARRAY);
  bool item_is_ptr = (fixed_size && m->opts.is_ptr);

  if (is_string (m))
    fprintf (fc,
      "    if (val->%s && !cser_xml_setvalue (val->%s, ctx))\n"
      "      return false;\n"
      , m->member_name, m->member_name
      );
  else
  {
    if (item_is_ptr)
      fprintf (fc,
        "    bool has_value = val->%s%s;\n"
        , m->member_name, use_idx ? "[i]" : ""
        );
    else
      fputs (
        "    bool has_value = true;\n", fc);

    if (m->opts.cardinality != CDN_SINGLE)
      fputs (
        "    const cser_xml_tag_t tag = { \"i\", has_value };\n"
        "    if (!cser_xml_opentag (&tag, ctx))\n"
        "      return false;\n", fc);

    fprintf (fc,
      "    if (has_value && !cser_xml_store_%s (%sval->%s%s, ctx))\n"
      "      return false;\n",
      utype, item_is_ptr ? "" : "&", m->member_name, use_idx ? "[i]" : ""
      );

    if (m->opts.cardinality != CDN_SINGLE)
      fputs (
        "    if (!cser_xml_closetag (\"i\", ctx))\n"
        "      return false;\n", fc);
  }
  free (utype);

  fputs ("  }\n", fc);
}

static void write_store_begin (const char *name, bool maybe_null, FILE *fc)
{
  fprintf (fc,
    "  {\n"
    "    const cser_xml_tag_t tag = { \"%s\", %s%s };\n"
    "    if (!cser_xml_opentag (&tag, ctx))\n"
    "      return false;\n"
    "  }\n"
    , name, maybe_null ? "val->" : "", maybe_null ? name : "true"
    );
}


static bool write_store_struct (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);
  fprintf (fh,
    "bool cser_xml_store_%s (const %s *val, void *ctx);\n",
    utype, type->type_name
    );
  fprintf (fc,
    "bool cser_xml_store_%s (const %s *val, void *ctx)\n{\n",
    utype, type->type_name
    );
  free (utype);

  for (member_t *m = type->composite; m; m = m->next)
  {
    switch (m->opts.cardinality)
    {
      case CDN_VAR_ARRAY:
        write_store_begin (m->member_name, true, fc);
        fprintf (fc,
          "  for (size_t i = 0; i < val->%s; ++i)\n",
          m->opts.variable_array_size_member
          );
        write_store_member_item (m, fc);
        break;
      case CDN_ZEROTERM_ARRAY:
        write_store_begin (m->member_name, true, fc);
        if (is_string (m))
          write_store_member_item (m, fc);
        else
        {
          fprintf (fc,
            "  for (size_t i = 0; val->%s && val->%s[i]; ++i)\n",
            m->member_name, m->member_name
            );
          write_store_member_item (m, fc);
        }
        break;
      case CDN_FIXED_ARRAY:
        write_store_begin (m->member_name, m->opts.is_ptr, fc);
        fprintf (fc,
          "  for (size_t i = 0; i < (%s); ++i)\n",
          m->opts.arr_sz
          );
        write_store_member_item (m, fc);
        break;
      case CDN_SINGLE:
        write_store_begin (m->member_name, m->opts.is_ptr, fc);
        write_store_member_item (m, fc);
        break;
    }
    fprintf (fc,
      "  if (!cser_xml_closetag (\"%s\", ctx))\n"
      "    return false;\n\n"
      , m->member_name
      );
  }

  fputs ("  return true;\n}\n\n", fc);

  return true;
}


static void write_load_member_item (const member_t *m, FILE *fc)
{
  bool use_idx = (m->opts.cardinality != CDN_SINGLE);
  bool fixed_size =
    (m->opts.cardinality == CDN_SINGLE ||
     m->opts.cardinality == CDN_FIXED_ARRAY);
  char *utype = make_cname (m->base_type);
  if (m->opts.is_ptr && fixed_size)
    fprintf (fc,
      "    if (!tag.has_value)\n"
      "      val->%s%s = 0;\n"
      "    else\n"
      "    {\n",
      m->member_name, use_idx ? "[i]" : ""
      );

  if (is_string (m))
    fprintf (fc,
      "    val->%s = cser_xml_getvalue (ctx);\n",
      m->member_name
      );
  else
  {
    bool needs_alloc = (m->opts.is_ptr && fixed_size);
    if (needs_alloc)
    {
      fprintf (fc,
        "    %s *tmpval = (%s *)calloc (1, sizeof (%s));\n"
        "    if (!cser_xml_load_%s (tmpval, ctx))\n"
        "    {\n"
        "      free (tmpval);\n"
        "      return false;\n"
        "    }\n"
        "    val->%s%s = tmpval;\n",
        m->base_type, m->base_type, m->base_type,
        utype,
        m->member_name, use_idx ? "[i]" : ""
        );
    }
    else
      fprintf (fc,
        "    if (!cser_xml_load_%s ((%s *)&val->%s%s, ctx))\n"
        "      return false;\n",
        utype, m->base_type, m->member_name, use_idx ? "[i]" : ""
        );
  }
  if (m->opts.is_ptr && fixed_size)
    fputs ("    }\n", fc);
  free (utype);
}


static bool write_load_struct (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);
  fprintf (fh,
    "bool cser_xml_load_%s (%s *val, void *ctx);\n",
    utype, type->type_name
    );
  fprintf (fc,
    "bool cser_xml_load_%s (%s *val, void *ctx)\n"
    "{\n"
    "  cser_xml_tag_t tag;\n",
    utype, type->type_name
    );
  free (utype);

  for (member_t *m = type->composite; m; m = m->next)
  {
    utype = make_cname (type->type_name);
    fprintf (fc,
      "  if (!cser_xml_nexttag (&tag, ctx))\n"
      "    return false;\n"
      "  if (strcmp (tag.name, \"%s\") != 0)\n"
      "    return false;\n",
      m->member_name
      );

    switch (m->opts.cardinality)
    {
      case CDN_VAR_ARRAY:
        fprintf (fc,
          "  val->%s = (%s *)calloc (val->%s, sizeof (%s));\n"
          "  for (size_t i = 0; i < (val->%s); ++i)\n"
          "  {\n"
          "    if (!cser_xml_nexttag (&tag, ctx))\n"
          "      return false;\n"
          "    if (strcmp (\"i\", tag.name) != 0)\n"
          "      return false;\n"
          , m->member_name, m->base_type, m->opts.variable_array_size_member, m->base_type
          , m->opts.variable_array_size_member
          );
        write_load_member_item (m, fc);
        fputs ("  }\n", fc);
        break;
      case CDN_ZEROTERM_ARRAY:
        fprintf (fc,
          "  {\n"
          "  val->%s = 0;\n"
          "  size_t n = 0;\n"
          "  for (size_t i = 0; tag.has_value; ++i)\n"
          "  {\n"
          "    if (!cser_xml_nexttag (&tag, ctx))\n"
          "      return false;\n"
          "    if (strcmp (\"i\", tag.name) != 0)\n"
          "      return false;\n"
          "    if (i >= n)\n"
          "    {\n"
          "      if (n == 0)\n"
          "        n = 2 * sizeof (%s);\n"
          "      val->%s = (%s *)realloc (val->%s, n *= 2);\n"
          "      if (!val->%s)\n"
          "        return false;\n"
          "      memset (((char *)val->%s) + n/2, n/2, 0);\n"
          "    }\n"
          , m->member_name
          , m->base_type
          , m->member_name, m->base_type, m->member_name
          , m->member_name
          , m->member_name
          );
        write_load_member_item (m, fc);
        fputs (
          "  }\n  }\n", fc);
        break;
      case CDN_FIXED_ARRAY:
        fprintf (fc,
          "  for (size_t i = 0; i < (%s); ++i)\n"
          "  {\n"
          "    if (!cser_xml_nexttag (&tag, ctx))\n"
          "      return false;\n"
          "    if (strcmp (\"i\", tag.name) != 0)\n"
          "      return false;\n"
          , m->opts.arr_sz
          );
        write_load_member_item (m, fc);
        fputs ("  }\n", fc);
        break;
      case CDN_SINGLE:
          write_load_member_item (m, fc);
        break;
    }
  }

  fputs ("  return true;\n}\n\n", fc);

  return true;
}


bool backend_xml (const type_list_t *types, const alias_list_t *aliases, FILE *fh, FILE *fc)
{
  fputs (
"\n\n/* cser xml backend */\n"
"#include <stdbool.h>\n"
"#include <stdint.h>\n"
"#include <stdlib.h>\n"
"#include <stdio.h>\n"
"#include <string.h>\n"
"#include <sys/types.h>\n"
"typedef struct cser_xml_tag\n"
"{\n"
"  const char *name;\n"
"  bool has_value;\n"
"} cser_xml_tag_t;\n"
"// The following glue functions to your XML implementation must be provided:\n"
"extern bool cser_xml_opentag (const cser_xml_tag_t *tag, void *ctx);\n"
"extern bool cser_xml_setvalue (const char *value, void *ctx);\n"
"extern bool cser_xml_closetag (const char *tagname, void *ctx);\n\n"
"extern bool cser_xml_nexttag (cser_xml_tag_t *tag, void *ctx);\n"
"extern char *cser_xml_getvalue (void *ctx);\n"
"// end glue prototypes\n"
"\n"
, fh);

  for (; types; types = types->next)
  {
    if (types->def.csfn == TYPE_NATIVE)
    {
      if (!write_store_native (&types->def, fh, fc) ||
          !write_load_native (&types->def, fh, fc))
        return false;
    }
    else
    {
      if (!write_store_struct (&types->def, fh, fc) ||
          !write_load_struct (&types->def, fh, fc))
        return false;
    }
  }

  for (; aliases; aliases = aliases->next)
  {
    char *ualias = make_cname (aliases->alias_name);
    char *uactual = make_cname (aliases->actual_name);

    fprintf (fh,
     "static inline int cser_xml_store_%s (const %s *val, void *ctx)\n"
     "{ return cser_xml_store_%s (val, ctx); }\n",
      ualias, aliases->alias_name,
      uactual
    );

    free (ualias);
    free (uactual);
  }

  return true;
}

