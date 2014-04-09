/**********************************************************************

  Copyright 2014 DiUS Computing Pty. Ltd. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification requires the prior written consent of the copyright
  owner.

**********************************************************************/

#ifndef _BACKEND_RAW_H_
#define _BACKEND_RAW_H_

#include "model.h"
#include <stdio.h>

bool backend_raw (const type_list_t *types, const alias_list_t *aliases, FILE *fh, FILE *fc);

#endif
