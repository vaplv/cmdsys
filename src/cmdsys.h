/*
 * Copyright (c) 2000 Vincent Forest
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef CMDSYS_H
#define CMDSYS_H

#include "sys/sys.h"
#include <stdbool.h>
#include <stddef.h>

#if defined(CMDSYS_STATIC)
# define CMDSYS_API
#elif defined(CMDSYS_SHARED_BUILD)
# define CMDSYS_API EXPORT_SYM
#else
# define CMDSYS_API IMPORT_SYM
#endif

#ifndef NDEBUG
# define CMDSYS(func) ASSERT(CMDSYS_NO_ERROR == cmdsys_##func)
#else
# define CMDSYS(func) cmdsys_##func
#endif /* NDEBUG */

#define C

struct cmdsys;
struct mem_allocator;

/*******************************************************************************
 *
 * Public types definition
 *
 ******************************************************************************/
enum cmdsys_error {
  CMDSYS_COMMAND_ERROR,
  CMDSYS_INVALID_ARGUMENT,
  CMDSYS_IO_ERROR,
  CMDSYS_MEMORY_ERROR,
  CMDSYS_NO_ERROR,
  CMDSYS_UNKNOWN_ERROR
};

enum cmdarg_type {
  CMDARG_INT,
  CMDARG_FLOAT,
  CMDARG_STRING,
  CMDARG_LITERAL,
  CMDARG_FILE,
  CMDARG_TYPES_COUNT
};

/* Descriptor of a command argument. */
struct cmdarg_desc {
  enum cmdarg_type type;
  const char* short_options;
  const char* long_options;
  const char* data_type;
  const char* glossary;
  unsigned int min_count;
  unsigned int max_count;
  union cmdarg_domain {
    struct { int min, max; } integer;
    struct { float min, max; } real;
    struct { const char** value_list; } string;
  } domain;
};

/* Mark the end of cmdarg desc list declaration. */
static const struct cmdarg_desc CMDARG_END = {
  .type = CMDARG_TYPES_COUNT,
  .short_options = (void*)0xDEADBEEF,
  .long_options = (void*)0xDEADBEEF,
  .data_type = (void*)0xDEADBEEF,
  .glossary = (void*)0xDEADBEEF,
  .min_count = 1,
  .max_count = 0,
  .domain = { .integer = { .min = 1, .max = 0 } }
};

struct cmdarg {
  enum cmdarg_type type;
  size_t count;
  struct cmdarg_value {
    /* Used to define if an optionnal arg is setup by the command or not.
     * Required arguments are always defined. */
    bool is_defined;
    union {
      float real;
      int integer;
      const char* string; /* Valid for string, and file arg types. */
    } data;
  } value_list[];
};

/*******************************************************************************
 *
 * Helper Macros.
 *
 ******************************************************************************/
/* Private macro. */
#define CMDARG_SETUP_COMMON__(t, sopt, lopt, data, glos, min, max)             \
  .type = t,                                                                   \
  .short_options = sopt,                                                       \
  .long_options = lopt,                                                        \
  .data_type = data,                                                           \
  .glossary = glos,                                                            \
  .min_count = min,                                                            \
  .max_count = max

#define CMDARGV(...)                                                           \
  (struct cmdarg_desc[]){__VA_ARGS__}

#define CMDARG_APPEND_FLOAT(                                                   \
  sopt, lopt, data, glos, min_count, max_count, min_val, max_val) {            \
    CMDARG_SETUP_COMMON__                                                      \
      (CMDARG_FLOAT, sopt, lopt, data, glos, min_count, max_count),            \
    .domain = { .real = { .min = min_val, .max = max_val }}                    \
  }

#define CMDARG_APPEND_INT(                                                     \
  sopt, lopt, data, glos, min_count, max_count, min_val, max_val) {            \
    CMDARG_SETUP_COMMON__                                                      \
      (CMDARG_INT, sopt, lopt, data, glos, min_count, max_count),              \
    .domain = { .integer = { .min = min_val, .max = max_val }}                 \
  }

#define CMDARG_APPEND_STRING(                                                  \
  sopt, lopt, data, glos, min_count, max_count, list) {                        \
    CMDARG_SETUP_COMMON__                                                      \
      (CMDARG_STRING, sopt, lopt, data, glos, min_count, max_count),           \
    .domain={ .string = { .value_list = list }}                                \
  }

#define CMDARG_APPEND_LITERAL(sopt, lopt, glos, min, max) {                    \
    CMDARG_SETUP_COMMON__                                                      \
      (CMDARG_LITERAL, (sopt), (lopt), NULL, (glos), (min), (max)),            \
    .domain={ .string = { .value_list = NULL }}                                \
  }

#define CMDARG_APPEND_FILE(sopt, lopt, data, glos, min_count, max_count) {     \
    CMDARG_SETUP_COMMON__                                                      \
      (CMDARG_FILE, sopt, lopt, data, glos, min_count, max_count),             \
    .domain={ .string = { .value_list = NULL }}                                \
  }

/*******************************************************************************
 *
 * Command function prototypes.
 *
 ******************************************************************************/
CMDSYS_API enum cmdsys_error
cmdsys_create
  (struct mem_allocator* allocator, /* May be NULL */
   struct cmdsys** cmdsys);

CMDSYS_API enum cmdsys_error
cmdsys_ref_get
  (struct cmdsys* cmdsys);

CMDSYS_API enum cmdsys_error
cmdsys_ref_put
  (struct cmdsys* cmdsys);

/* Multi syntax is supported by adding several commands with the same name. */
CMDSYS_API enum cmdsys_error
cmdsys_add_command
  (struct cmdsys* cmdsys,
   const char* name,
   void (*func)(struct cmdsys*, size_t argc, const struct cmdarg**, void*),
   void *data,
   void (*arg_completion) /* May be NULL.*/
    (struct cmdsys*, const char*, size_t, size_t*, const char**[]),
   const struct cmdarg_desc argv_desc[],
   const char* description); /* May be NULL. */

CMDSYS_API enum cmdsys_error
cmdsys_del_command
  (struct cmdsys* cmdsys,
   const char* name);

CMDSYS_API enum cmdsys_error
cmdsys_has_command
  (struct cmdsys* cmdsys,
   const char* name,
   bool* has_command);

CMDSYS_API enum cmdsys_error
cmdsys_execute_command
  (struct cmdsys* cmdsys,
   const char* command,
   const char* inverse); /* May be NULL */

CMDSYS_API enum cmdsys_error
cmdsys_man_command
  (struct cmdsys* cmdsys,
   const char* command,
   size_t* len, /* May be NULL. */
   size_t max_buf_len,
   char* buffer); /* May be NULL. */

CMDSYS_API enum cmdsys_error
cmdsys_command_arg_completion
  (struct cmdsys* cmdsys,
   const char* cmd_name,
   const char* arg_str,
   size_t arg_str_len,
   size_t hint_argc,
   char* hint_argv[],
   size_t* completion_list_len,
   const char** completion_list[]);

/* The returned list is valid until the add/del command function is called. */
CMDSYS_API enum cmdsys_error
cmdsys_command_name_completion
  (struct cmdsys* cmdsys,
   const char* cmd_name,
   size_t cmd_name_len,
   size_t* completion_list_len,
   const char** completion_list[]);

#endif /* CMDSYS_H */

