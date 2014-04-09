/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/
#ifndef _FRONTEND_H_
#define _FRONTEND_H_

#include <stdint.h>
#include <stdbool.h>

#define YYSTYPE char *

#define NO_MEMBERS false
#define WITH_MEMBERS true

void yyerror(const char *);
int sym_type(const char *);

void add_typedef_name (const char *name);
void add_enum_constant (const char *name);

typedef struct parse_info
{
  unsigned ptr;

  char *base_type;
  char *name;
  char *arr_sz;
  char *array_def; // null => default, "0"/"1" -> single/zeroterm, other->vararr

  struct parse_info *next;
} parse_info_t;
extern parse_info_t *info;

extern bool struct_ns;

void capture (bool expect_members);
void capture_member (void);
void end_capture (bool end_of_members);

void set_type (const char *base_type);
void set_name (const char *name);
void reset_info (void);
void note_array_size (const char *arr_str);
void note_pointer (void);
void handle_pragma (const char *prag);

char *name_struct (const char *name);

void add_placeholder (const char *name);
bool has_placeholder (const char *name);


#endif
