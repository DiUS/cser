/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/

#ifndef _MODEL_H_
#define _MODEL_H_

// Known limitations:
//  - no more than one level of pointers (but array of pointers ok)
//  - unions not supported
//  - bit fields not properly supported
//  - array typedefs not supported - TODO: can we do this easily?
//  - unnamed-untypedef'd structs not supported
//  - partial function typedefs show up in verbose output
//  - [u]int[n]s, bool, char, char* only supported native types by default

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* A classification of the types we work with in the model.
 * - A native type is a plain, unadorned integer type (signed or unsigned).
 * - A decorated type is a type which is a pointer, array or variable length
 *   array member. This is an intermediary type only.
 * - A composite type is a representation of a struct.
 */
typedef enum {
  TYPE_NATIVE,
  TYPE_DECORATED,
  TYPE_COMPOSITE,
} classification_t;

#define ARRAY_STR_SIZE -2
#define ARRAY_ZEROTERM -1
#define ARRAY_NOT_ARRAY 0

typedef enum
{
  CDN_SINGLE,
  CDN_FIXED_ARRAY,
  CDN_VAR_ARRAY,
  CDN_ZEROTERM_ARRAY,
} cardinality_t;

// Options/decorations on top of a base type, when used as a struct member
typedef struct decorations
{
  size_t is_ptr;

  cardinality_t cardinality;
  char *arr_sz;

  // If this member is a pointer to a variable length array, look at the
  // specified member_name to find the actual length of this member
  char *variable_array_size_member;
  // TODO: ensure we abort if this member is *after* the array, as we
  // can't necessarily restore it in that case...
} decorations_t;


// List node for holding struct members
typedef struct member
{
  char *member_name;
  char *base_type; // only to native/composite

  decorations_t opts;

  struct member *next;
} member_t;


// Intermediate type for holding e.g. pointer typedefs.
// These types are *NOT* to be referenced by a member_t - instead, the
// decorations from here should be folded into the member decorations.
typedef struct decorated_type
{
  char *base_type; // only to native/composite
  decorations_t opts;
} decorated_type_t;


// A type description for a data type (native or composite)
typedef struct type
{
  char *type_name;
  classification_t csfn;
  union {
    decorated_type_t  decorated;
    member_t         *composite;
  };
} type_t;


// List node for holding all types
typedef struct type_list
{
  type_t def;

  bool used; // flag indicating whether store/load fns should be generated

  struct type_list *next;
} type_list_t;


// Typedef resolution list
typedef struct alias_list
{
  char *alias_name;
  char *actual_name;

  bool used; // flag indicating whether store/load fns should be generated

  struct alias_list *next;
} alias_list_t;


void add_type (type_list_t *t);
void add_alias (alias_list_t *a);
const type_t *lookup_type (const char *type_name);

#endif
