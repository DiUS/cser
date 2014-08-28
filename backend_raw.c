/* Copyright (C) 2014 DiUS Computing Pty. Ltd.   See LICENSE file. */

#include "backend_raw.h"
#include <string.h>
#include <stdlib.h>

static bool write_store_native (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);

  fprintf (fh,
    "int cser_raw_store_%s (const %s *val, cser_raw_write_fn w, void *q);\n",
    utype, type->type_name);
  fprintf (fc,
    "int cser_raw_store_%s (const %s *val, cser_raw_write_fn w, void *q)\n"
    "{\n"
    "  %s tmp = *val;\n"
    "  uint8_t bytes[sizeof (%s)];\n"
    "  for (unsigned i = 1; i <= sizeof (%s); ++i)\n"
    "  {\n"
    "    bytes[sizeof (%s) - i] = (uint8_t)(tmp & 0xff);\n"
    "    tmp >>= 8;\n"
    "  }\n"
    "  return w (bytes, sizeof (%s), q);\n"
    "}\n",
    utype, type->type_name,
    type->type_name,
    type->type_name,
    type->type_name,
    type->type_name,
    type->type_name
    );
  free (utype);
  return !ferror (fh) && !ferror (fc);
}


static bool write_load_native (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);

  fprintf (fh,"int cser_raw_load_%s (%s *val, cser_raw_read_fn r, void *q);\n",
    utype, type->type_name);

  fprintf (fc,
    "int cser_raw_load_%s (%s *val, cser_raw_read_fn r, void *q)\n"
    "{\n"
    "  uint8_t bytes[sizeof (%s)];\n"
    "  int ret = r (bytes, sizeof (%s), q);\n"
    "  if (ret != 0)\n"
    "    return ret;\n"
    "  %s tmp = 0;\n"
    "  for (unsigned i = 0; i < (sizeof (%s)); ++i)\n"
    "    tmp = (%s)((tmp << 8) | bytes[i]);\n"
    "  *val = tmp;\n"
    "  return 0;\n"
    "}\n",
    utype, type->type_name,
    type->type_name,
    type->type_name,
    type->type_name,
    type->type_name,
    type->type_name
    );
  free (utype);
  return !ferror (fh) && !ferror (fc);
}


static void write_presence (FILE *fc, const char *name, bool arr)
{
  fprintf (fc,
    "   uint8_t present = (val->%s%s != 0);\n"
    "   int ret = w (&present, sizeof (present), q);\n"
    "   if (ret != 0)\n"
    "     return ret;\n"
    "   if (present)\n"
    "   {\n",
    name, arr ? "[i]" : ""
    );
}


static bool write_store_struct (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);
  fprintf (fh,
    "int cser_raw_store_%s (const %s *val, cser_raw_write_fn w, void *q);\n",
    utype, type->type_name);

  fprintf (fc,
    "int cser_raw_store_%s (const %s *val, cser_raw_write_fn w, void *q)\n"
    "{\n",
    utype, type->type_name);

  free (utype);
  utype = 0;

  for (member_t *m = type->composite; m; m = m->next)
  {
    fputs (" {\n", fc);
    bool array = false;
    if (m->opts.variable_array_size_member)
    {
      if (!m->opts.is_ptr)
        abort ();
      write_presence (fc, m->member_name, false);
      fprintf (fc,
        "    for (size_t i = 0; (val->%s) && (i < val->%s); ++i)\n"
        "    {\n",
        m->member_name, m->opts.variable_array_size_member
        );
      array = true;
    }
    else switch (m->opts.cardinality)
    {
      case CDN_ZEROTERM_ARRAY:
        write_presence (fc, m->member_name, false);
        fprintf (fc,
          "    for (size_t i = 0; (val->%s) && ((i == 0) || (val->%s[i-1])); ++i)\n"
          "    {\n",
          m->member_name, m->member_name
          );
        array = true;
        break;
      case CDN_SINGLE:
        if (m->opts.is_ptr)
          write_presence (fc, m->member_name, false);
        else
          fputs ("    {\n", fc);
        break;
      default:
        fprintf (fc,
          "  for (size_t i = 0; i < (%s); ++i)\n"
          "  {\n",
          m->opts.arr_sz);
        if (m->opts.is_ptr)
          write_presence (fc, m->member_name, true);
        else
          fputs ("   {\n", fc);
        array = true;
        break;
    }
    /* !ptr || zeroterm || variable -> &, else "" */
    bool need_amp =
      (!m->opts.is_ptr ||
       m->opts.cardinality == CDN_ZEROTERM_ARRAY ||
       m->opts.variable_array_size_member);
    utype = make_cname (m->base_type);
    fprintf (fc,
      "      int ret = cser_raw_store_%s ((%s*)%sval->%s%s, w, q);\n"
      "      if (ret != 0)\n"
      "        return ret;\n"
      "   }\n"
      "%s\n",
      utype,
      m->base_type,
      need_amp ? "&" : "",
      m->member_name,
      array ? "[i]" : "",
      array ? "  }" : ""
      );
    free (utype);

    fputs (" }\n", fc);
  }

  fputs ("  return 0;\n}\n", fc);

  return !ferror (fh) && !ferror (fc);
}

static void write_load_item (
  const char *target, const char *base_type, const char *indent, bool pointer,
  FILE *fc)
{
  char *utype = make_cname (base_type);

  if (pointer)
  {
    fprintf (fc,
      "%s%s *tmp_item = calloc (1, sizeof (%s));\n"
      "%sif (!tmp_item)\n"
      "%s  return -ENOMEM;\n"
      "%sint ret = cser_raw_load_%s (tmp_item, r, q);\n"
      "%sif (ret == 0)\n"
      "%s  %s = tmp_item;\n"
      "%selse\n"
      "%s  free (tmp_item);\n",
      indent, base_type, base_type,
      indent,
      indent,
      indent, utype,
      indent,
      indent, target,
      indent,
      indent
      );
  }
  else
    fprintf (fc,
      "%sint ret = cser_raw_load_%s ((%s*)&%s, r, q);\n",
      indent, utype, base_type, target
      );

  free (utype);
}


static void write_presence_check (FILE *fc)
{
  fprintf (fc,
    "  uint8_t present;\n"
    "  int ret = r (&present, sizeof (present), q);\n"
    "  if (ret != 0)\n"
    "    return ret;\n"
    "  if (present)\n"
    "  {\n"
    );
}


static bool write_load_struct (const type_t *type, FILE *fh, FILE *fc)
{
  char *utype = make_cname (type->type_name);

  fprintf (fh,
    "int cser_raw_load_%s (%s *val, cser_raw_read_fn r, void *q);\n",
    utype, type->type_name);

  fprintf (fc,
    "int cser_raw_load_%s (%s *val, cser_raw_read_fn r, void *q)\n"
    "{\n",
    utype, type->type_name);

  free (utype);
  utype = 0;

  for (member_t *m = type->composite; m; m = m->next)
  {
    fputs (" {\n", fc);
    switch (m->opts.cardinality)
    {
      case CDN_VAR_ARRAY:
        write_presence_check (fc);
        fprintf (fc,
          "    %s *items = calloc (val->%s, sizeof (%s));\n"
          "    if (!items)\n"
          "      return -ENOMEM;\n",
          m->base_type, m->opts.variable_array_size_member, m->base_type
          );
        fprintf (fc,
          "    for (unsigned i = 0; i < val->%s; ++i)\n"
          "    {\n",
          m->opts.variable_array_size_member
          );
        write_load_item ("items[i]", m->base_type, "      ", false, fc);
        fprintf (fc,
          "      if (ret != 0)\n"
          "        return ret;\n"
          "      val->%s = items;\n"
          "    }\n"
          "  }\n",
          m->member_name
          );
        break;
      case CDN_ZEROTERM_ARRAY:
        // This looks nothing like a normal array load, as it needs to
        // incrementally load until it finds the end marker. A bog standard
        // 2*n alloc/copy/retry approach is used for now.

        write_presence_check (fc);
        fprintf (fc,
          "    %s *tmp = 0;\n"
          "    size_t n = 0;\n"
          "    size_t offs = 0;\n"
          "    do {\n"
          "      if (offs >= n)\n"
          "      {\n"
          "        if (n == 0)\n"
          "          n = 2 * sizeof(%s);\n"
          "        tmp = (%s *)realloc (tmp, n *= 2);\n"
          "        if (!tmp)\n"
          "          return -ENOMEM;\n"
          "        memset ((char *)tmp + n/2, n/2, 0);\n"
          "      }\n"
          , m->base_type
          , m->base_type
          , m->base_type
          );
        write_load_item (
          "tmp[offs++]", m->base_type, "      ", false, fc);
        fprintf (fc,
          "      if (ret != 0)\n"
          "      {\n"
          "        free (tmp);\n"
          "        return ret;\n"
          "      }\n"
          "    } while (tmp[offs - 1]);\n"
          "    val->%s = tmp;\n"
          "  }\n",
          m->member_name
          );
        break;
      case CDN_SINGLE:
      {
        if (m->opts.is_ptr)
          write_presence_check (fc);
        char *target;
        if (asprintf (&target, "val->%s", m->member_name) < 0)
          return false;
        write_load_item (target, m->base_type, "      ", m->opts.is_ptr, fc);
        free (target);
        fprintf (fc,
          "    if (ret != 0)\n"
          "      return ret;\n"
          );
        if (m->opts.is_ptr)
          fputs ("  }\n", fc); // close presence check scope
        break;
      }
      default:
      {
        fprintf (fc,
          "  for (unsigned i = 0; i < (%s); ++i)\n"
          "  {\n",
          m->opts.arr_sz
          );
        if (m->opts.is_ptr)
          write_presence_check (fc);
        char *target;
        if (asprintf (&target, "%sval->%s[i]",
                      /*m->opts.is_ptr ? "" : "&"*/"", m->member_name) < 0)
          return false;
        write_load_item (target, m->base_type, "    ", m->opts.is_ptr, fc);
        free (target);
        fprintf (fc,
          "    if (ret != 0)\n"
          "      return ret;\n"
          );
        if (m->opts.is_ptr)
          fputs ("  }\n", fc); // close presence check scope
        fputs ("  }\n", fc); // close loop scope
        break;
      }
    }

    fputs (" }\n", fc);
  }

  fputs ("  return 0;\n}\n", fc);

  return !ferror (fh) && !ferror (fc);
}


bool backend_raw (const type_list_t *types, const alias_list_t *aliases, FILE *fh, FILE *fc)
{
  // Callback definitions
  fputs ("#include <stdint.h>\n", fh);
  fputs ("#include <stdlib.h>\n", fh);
  fputs ("#include <sys/types.h>\n", fh);
  fputs ("#include <errno.h>\n\n", fh);
  fputs ("/* The callback functions take a buffer, a length, and an opaque */\n"
         "/* pointer which is passed through. They MUST return zero (0) on */\n"
         "/* success. Any non-zero value is treated as an error and bubbled*/\n"
         "/* back up to the caller. Note that \"short\" reads and writes   */\n"
         "/* are NOT used or supported in this interface, unlike that of   */\n"
         "/* read(2)/write(2).                                             */\n"
         , fh);
  fputs ("typedef int (*cser_raw_write_fn) (const uint8_t *bytes, size_t n, void *q);\n", fh);
  fputs ("typedef int (*cser_raw_read_fn) (uint8_t *bytes, size_t n, void *q);\n\n", fh);

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
     "static inline int cser_raw_store_%s (const %s *val, cser_raw_write_fn w, void *q)\n"
     "{ return cser_raw_store_%s (val, w, q); }\n",
      ualias, aliases->alias_name,
      uactual
    );

    free (ualias);
    free (uactual);
  }

  return true;
}
