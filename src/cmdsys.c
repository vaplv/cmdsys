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
#include "stdlib/sl_flat_set.h"
#include "stdlib/sl_hash_table.h"
#include "stdlib/sl_string.h"
#include "sys/list.h"
#include "sys/math.h"
#include "sys/mem_allocator.h"
#include "sys/ref_count.h"
#include "sys/sys.h"
#include "cmdsys.h"
#include <argtable2.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define IS_END_REACHED(desc) \
  (memcmp(&desc, &CMDARG_END, sizeof(struct cmdarg_desc)) == 0)

#define ERRBUF_LEN 1024
#define SCRATCH_LEN 1024

struct errbuf {
  char buffer[ERRBUF_LEN];
  size_t buffer_id;
};

struct cmdsys {
  char scratch[SCRATCH_LEN];
  FILE* stream;
  struct mem_allocator* allocator;
  struct sl_hash_table* htbl; /* hash table [cmd name, struct cmd*] */
  struct sl_flat_set* name_set;  /* set of const char*. Used by completion.*/
  struct errbuf errbuf;
  struct ref ref;
};

struct cmd {
  struct list_node node;
  size_t argc;
  void(*func)(struct cmdsys*, size_t, const struct cmdarg**, void* data);
  void* data;
  void (*completion)
    (struct cmdsys*, const char*, size_t, size_t*, const char**[]);
  struct sl_string* description;
  union cmdarg_domain* arg_domain;
  struct cmdarg** argv;
  void** arg_table;
};

/*******************************************************************************
 *
 * Error management.
 *
 ******************************************************************************/
static void
errbuf_print(struct errbuf* buf, const char* fmt, ...) FORMAT_PRINTF(2, 3);

void
errbuf_print(struct errbuf* buf, const char* fmt, ...)
{
  ASSERT(buf);

  va_list vargs_list;
  const size_t buf_size = sizeof(buf->buffer);
  const size_t buf_size_remaining = buf_size - buf->buffer_id - 1;
  int i = 0;

  if(buf->buffer_id >= buf_size - 1)
    return;

  va_start(vargs_list, fmt);

  i = vsnprintf
    (buf->buffer + buf->buffer_id, buf_size_remaining, fmt, vargs_list);
  ASSERT(i > 0);

  if((size_t)i >= buf_size) {
    buf->buffer_id = buf_size;
    buf->buffer[buf_size - 1] = '\0';
  } else {
    buf->buffer_id += i;
  }

  va_end(vargs_list);
}

static FINLINE void
errbuf_flush(struct errbuf* buf)
{
  ASSERT(buf);
  buf->buffer_id = 0;
  buf->buffer[0] = '\0';
}

static enum cmdsys_error
sl_to_cmdsys_error(enum sl_error sl_err)
{
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  switch(sl_err) {
    case SL_INVALID_ARGUMENT:
      err = CMDSYS_INVALID_ARGUMENT;
      break;
    case SL_MEMORY_ERROR:
      err = CMDSYS_MEMORY_ERROR;
      break;
    case SL_NO_ERROR:
      err = CMDSYS_NO_ERROR;
      break;
    default:
      err = CMDSYS_UNKNOWN_ERROR;
      break;
  }
  return err;
}

/*******************************************************************************
 *
 * Hash table/set functions.
 *
 ******************************************************************************/
static size_t
hash_str(const void* key)
{
  const char* str = *(const char**)key;
  return sl_hash(str, strlen(str));
}

static bool
eqstr(const void* key0, const void* key1)
{
  const char* str0 = *(const char**)key0;
  const char* str1 = *(const char**)key1;
  return strcmp(str0, str1) == 0;
}

static int
cmpstr(const void* a, const void* b)
{
  const char* str0 = *(const char**)a;
  const char* str1 = *(const char**)b;
  return strcmp(str0, str1);
}

/*******************************************************************************
 *
 * Helper function.
 *
 ******************************************************************************/
static enum cmdsys_error
init_domain_and_table(struct cmd* cmd, const struct cmdarg_desc argv_desc[])
{
  size_t i = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  ASSERT(cmd);

  if(argv_desc) {
    void* arg = NULL;

    #define ARG(suffix, a)                                                     \
      CONCAT(arg_, suffix)                                                     \
        ((a).short_options, (a).long_options, (a).data_type, (a).glossary)
    #define LIT(suffix, a)                                                     \
      CONCAT(arg_, suffix)                                                     \
        ((a).short_options, (a).long_options, (a).glossary)
    #define ARGN(suffix, a)                                                    \
      CONCAT(CONCAT(arg_, suffix), n)                                          \
        ((a).short_options, (a).long_options, (a).data_type,                   \
         (a).min_count, (a).max_count, (a).glossary)
    #define LITN(suffix, a)                                                    \
      CONCAT(CONCAT(arg_, suffix), n)                                          \
        ((a).short_options, (a).long_options, (a).min_count,                   \
         (a).max_count, (a).glossary)

    for(i = 0; !IS_END_REACHED(argv_desc[i]); ++i) {
     if(argv_desc[i].min_count == 0 && argv_desc[i].max_count == 1) {
        switch(argv_desc[i].type) {
          case CMDARG_INT: arg = ARG(int0, argv_desc[i]); break;
          case CMDARG_FILE: arg = ARG(file0, argv_desc[i]); break;
          case CMDARG_FLOAT: arg = ARG(dbl0, argv_desc[i]); break;
          case CMDARG_STRING: arg = ARG(str0, argv_desc[i]); break;
          case CMDARG_LITERAL: arg = LIT(lit0, argv_desc[i]); break;
          default: ASSERT(0); break;
        }
      } else if(argv_desc[i].max_count == 1) {
        switch(argv_desc[i].type) {
          case CMDARG_INT: arg = ARG(int1, argv_desc[i]); break;
          case CMDARG_FILE: arg = ARG(file1, argv_desc[i]); break;
          case CMDARG_FLOAT: arg = ARG(dbl1, argv_desc[i]); break;
          case CMDARG_STRING: arg = ARG(str1, argv_desc[i]); break;
          case CMDARG_LITERAL: arg = LIT(lit1, argv_desc[i]); break;
          default: ASSERT(0); break;
        }
      } else {
        switch(argv_desc[i].type) {
          case CMDARG_INT: arg = ARGN(int, argv_desc[i]); break;
          case CMDARG_FILE: arg = ARGN(file, argv_desc[i]); break;
          case CMDARG_FLOAT: arg = ARGN(dbl, argv_desc[i]); break;
          case CMDARG_STRING: arg = ARGN(str, argv_desc[i]); break;
          case CMDARG_LITERAL: arg = LITN(lit, argv_desc[i]); break;
          default: ASSERT(0); break;
        }
      }
      cmd->arg_table[i] = arg;
      cmd->arg_domain[i + 1] = argv_desc[i].domain; /* +1 <=> command name. */
    }
  }

  cmd->arg_table[i] = arg_end(16);
  if(arg_nullcheck(cmd->arg_table)) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }

  #undef ARG
  #undef LIT
  #undef ARGN
  #undef LITN

exit:
  return err;
error:
  goto exit;
}

static FINLINE void
set_optvalue_flag(struct cmd* cmd, const bool val)
{
  size_t argv_id = 0;
  size_t tbl_id = 0;
  ASSERT(cmd);

  #define SET_OPTVAL(arg, val)                                                 \
    do {                                                                       \
      if(val)                                                                  \
        (arg)->hdr.flag |= ARG_HASOPTVALUE;                                    \
      else                                                                     \
        (arg)->hdr.flag &= ~ARG_HASOPTVALUE;                                   \
    } while(0)

  for(argv_id = 1, tbl_id = 0; argv_id < cmd->argc; ++argv_id, ++tbl_id) {
    switch(cmd->argv[argv_id]->type) {
      case CMDARG_INT:
        SET_OPTVAL((struct arg_int*)cmd->arg_table[tbl_id], val);
        break;
      case CMDARG_FILE:
        SET_OPTVAL((struct arg_file*)cmd->arg_table[tbl_id], val);
        break;
      case CMDARG_FLOAT:
        SET_OPTVAL((struct arg_dbl*)cmd->arg_table[tbl_id], val);
        break;
      case CMDARG_STRING:
        SET_OPTVAL((struct arg_str*)cmd->arg_table[tbl_id], val);
        break;
      case CMDARG_LITERAL:
        SET_OPTVAL((struct arg_lit*)cmd->arg_table[tbl_id], val);
        break;
      default: ASSERT(0); /* Unreachable code */ break;
    }
  }
  #undef SET_OPTVAL
}

static int
defined_args_count(struct cmd* cmd)
{
  int count = 0;
  size_t argv_id = 0;
  size_t tbl_id = 0;
  ASSERT(cmd);

  for(argv_id = 1, tbl_id = 0; argv_id < cmd->argc; ++argv_id, ++tbl_id) {
    switch(cmd->argv[argv_id]->type) {
      case CMDARG_INT:
        count += ((struct arg_int*)cmd->arg_table[tbl_id])->count;
        break;
      case CMDARG_FILE:
        count += ((struct arg_file*)cmd->arg_table[tbl_id])->count;
        break;
      case CMDARG_FLOAT:
        count += ((struct arg_dbl*)cmd->arg_table[tbl_id])->count;
        break;
      case CMDARG_STRING:
        count += ((struct arg_str*)cmd->arg_table[tbl_id])->count;
        break;
      case CMDARG_LITERAL:
        count += ((struct arg_lit*)cmd->arg_table[tbl_id])->count;
        break;
      default: ASSERT(0); break;
    }
  }
  return count;
}

static enum cmdsys_error
setup_cmd_arg(struct cmdsys* sys, struct cmd* cmd, const char* name)
{
  size_t arg_id = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;

  ASSERT(cmd && cmd->argc && cmd->argv[0]->type == CMDARG_STRING);
  cmd->argv[0]->value_list[0].is_defined = true;
  cmd->argv[0]->value_list[0].data.string = name;

  for(arg_id = 1; arg_id < cmd->argc; ++arg_id) {
    const char** value_list = NULL;
    const char* str = NULL;
    const size_t arg_tbl_id = arg_id - 1; /* -1 <=> arg name. */
    int val_id = 0;
    bool isdef = true;

    for(val_id=0; isdef && (size_t)val_id<cmd->argv[arg_id]->count; ++val_id) {
      switch(cmd->argv[arg_id]->type) {
        case CMDARG_STRING:
          isdef = val_id < ((struct arg_str*)cmd->arg_table[arg_tbl_id])->count;
          if(isdef) {
            str = ((struct arg_str*)(cmd->arg_table[arg_tbl_id]))->sval[val_id];
            /* Check the string domain. */
            value_list = cmd->arg_domain[arg_id].string.value_list;
            if(value_list == NULL) {
              cmd->argv[arg_id]->value_list[val_id].data.string = str;
            } else {
              size_t i = 0;
              for(i = 0; value_list[i] != NULL; ++i) {
                if(strcmp(str, value_list[i]) == 0)
                  break;
              }
              if(value_list[i] != NULL) {
                cmd->argv[arg_id]->value_list[val_id].data.string = str;
              } else {
                errbuf_print
                  (&sys->errbuf, 
                   "%s: unexpected option value `%s'\n",
                   name, str);
                err = CMDSYS_COMMAND_ERROR;
                goto error;
              }
            }
          }
          break;
        case CMDARG_FILE:
          isdef = val_id< ((struct arg_file*)cmd->arg_table[arg_tbl_id])->count;
          if(isdef) {
            cmd->argv[arg_id]->value_list[val_id].data.string =
             ((struct arg_file*)(cmd->arg_table[arg_tbl_id]))->filename[val_id];
          }
          break;
        case CMDARG_INT:
          isdef = val_id < ((struct arg_int*)cmd->arg_table[arg_tbl_id])->count;
          if(isdef) {
            cmd->argv[arg_id]->value_list[val_id].data.integer = MAX(MIN(
              ((struct arg_int*)(cmd->arg_table[arg_tbl_id]))->ival[val_id],
              cmd->arg_domain[arg_id].integer.max),
              cmd->arg_domain[arg_id].integer.min);
          }
          break;
        case CMDARG_FLOAT:
          isdef = val_id < ((struct arg_dbl*)cmd->arg_table[arg_tbl_id])->count;
          if(isdef) {
            cmd->argv[arg_id]->value_list[val_id].data.real = MAX(MIN(
              ((struct arg_dbl*)(cmd->arg_table[arg_tbl_id]))->dval[val_id],
              cmd->arg_domain[arg_id].real.max),
              cmd->arg_domain[arg_id].real.min);
          }
          break;
        case CMDARG_LITERAL:
          isdef = val_id < ((struct arg_lit*)cmd->arg_table[arg_tbl_id])->count;
          break;
        default:
          ASSERT(0);
          break;
      }
      cmd->argv[arg_id]->value_list[val_id].is_defined = isdef;
    }
  }
exit:
  return err;
error:
  goto exit;
}

static enum cmdsys_error
register_command
  (struct cmdsys* sys,
   struct cmd* cmd,
   const char* name)
{
  struct list_node* list = NULL;
  char* cmd_name = NULL;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  enum sl_error sl_err = SL_NO_ERROR;
  bool is_inserted_in_htbl = false;
  bool is_inserted_in_fset = false;
  ASSERT(sys && cmd && name);

  SL(hash_table_find(sys->htbl, &name, (void**)&list));

  /* Register the command against the command system if it does not exist. */
  if(list == NULL) {
    cmd_name = MEM_CALLOC(sys->allocator, strlen(name) + 1, sizeof(char));
    if(NULL == cmd_name) {
      err = CMDSYS_MEMORY_ERROR;
      goto error;
    }
    strcpy(cmd_name, name);

    sl_err = sl_hash_table_insert
      (sys->htbl, &cmd_name, (struct list_node[]){{NULL, NULL}});
    if(SL_NO_ERROR != sl_err) {
      err = sl_to_cmdsys_error(sl_err);
      goto error;
    }
    is_inserted_in_htbl = true;

    SL(hash_table_find(sys->htbl, &cmd_name, (void**)&list));
    ASSERT(list != NULL);
    list_init(list);

    sl_err = sl_flat_set_insert(sys->name_set, &cmd_name, NULL);
    if(SL_NO_ERROR != sl_err) {
      err = sl_to_cmdsys_error(sl_err);
      goto error;
    }
    is_inserted_in_fset = true;
  }

  list_add(list, &cmd->node);

exit:
  return err;
error:
  if(cmd_name) {
    size_t i = 0;

    if(is_inserted_in_htbl) {
      SL(hash_table_erase(sys->htbl, &cmd_name, &i));
      ASSERT(1 == i);
    }
    if(is_inserted_in_fset) {
      SL(flat_set_erase(sys->name_set, &cmd_name, NULL));
    }
    MEM_FREE(sys->allocator, cmd_name);
  }
  goto exit;
}

static void
del_all_commands(struct cmdsys* sys)
{
  struct sl_hash_table_it it;
  bool b = false;
  ASSERT(sys && sys->htbl);

  SL(hash_table_begin(sys->htbl, &it, &b));
  while(!b) {
    struct list_node* list = (struct list_node*)it.pair.data;
    struct list_node* pos = NULL;
    struct list_node* tmp = NULL;
    size_t i = 0;

    ASSERT(is_list_empty(list) == false);

    LIST_FOR_EACH_SAFE(pos, tmp, list) {
      struct cmd* cmd = CONTAINER_OF(pos, struct cmd, node);
      list_del(pos);

      if(cmd->description)
        SL(free_string(cmd->description));
      for(i = 0; i < cmd->argc; ++i) {
        MEM_FREE(sys->allocator, cmd->argv[i]);
      }
      MEM_FREE(sys->allocator, cmd->argv);
      MEM_FREE(sys->allocator, cmd->arg_domain);
      arg_freetable(cmd->arg_table, cmd->argc + 1); /* +1 <=> arg_end. */
      MEM_FREE(sys->allocator, cmd->arg_table);
      MEM_FREE(sys->allocator, cmd);
    }

    MEM_FREE(sys->allocator, (*(char**)it.pair.key));
    SL(hash_table_it_next(&it, &b));
  }
  SL(hash_table_clear(sys->htbl));
}

static void
release_cmdsys(struct ref* ref)
{
  struct cmdsys* sys = NULL;
  ASSERT(ref != NULL);

  sys = CONTAINER_OF(ref, struct cmdsys, ref);

  if(sys->htbl) {
    del_all_commands(sys);
    SL(free_hash_table(sys->htbl));
  }
  if(sys->name_set)
    SL(free_flat_set(sys->name_set));
  if(sys->stream)
    fclose(sys->stream);

  MEM_FREE(sys->allocator, sys);
}

/*******************************************************************************
 *
 * Command functions
 *
 ******************************************************************************/
enum cmdsys_error
cmdsys_create(struct mem_allocator* allocator, struct cmdsys** out_sys)
{
  struct mem_allocator* alloc = allocator ? allocator : &mem_default_allocator;
  struct cmdsys* sys = NULL;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  enum sl_error sl_err = SL_NO_ERROR;

  if(!out_sys) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }

  sys = MEM_CALLOC(alloc, 1, sizeof(struct cmdsys));
  if(!sys) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  sys->allocator = alloc;
  ref_init(&sys->ref);

  sl_err = sl_create_hash_table
    (sizeof(const char*),
     ALIGNOF(const char*),
     sizeof(struct list_node),
     ALIGNOF(struct list_node),
     hash_str,
     eqstr,
     sys->allocator,
     &sys->htbl);
  if(SL_NO_ERROR != sl_err) {
    err = sl_to_cmdsys_error(sl_err);
    goto error;
  }
  sl_err = sl_create_flat_set
    (sizeof(const char*),
     ALIGNOF(const char*),
     cmpstr,
     sys->allocator,
     &sys->name_set);
  if(SL_NO_ERROR != sl_err) {
    err = sl_to_cmdsys_error(sl_err);
    goto error;
  }
  sys->stream = tmpfile();
  if(!sys->stream) {
    err = CMDSYS_IO_ERROR;
    goto error;
  }
exit:
  if(out_sys)
    *out_sys = sys;
  return err;
error:
  if(sys) {
    CMDSYS(ref_put(sys));
    sys = NULL;
  }
  goto exit;
}

enum cmdsys_error
cmdsys_ref_get(struct cmdsys* sys)
{
  if(!sys)
    return CMDSYS_INVALID_ARGUMENT;
  ref_get(&sys->ref);
  return CMDSYS_NO_ERROR;
}

enum cmdsys_error
cmdsys_ref_put(struct cmdsys* sys)
{
  if(!sys)
    return CMDSYS_INVALID_ARGUMENT;
  ref_put(&sys->ref, release_cmdsys);
  return CMDSYS_NO_ERROR;
}

enum cmdsys_error
cmdsys_add_command
  (struct cmdsys* sys,
   const char* name,
   void (*func)(struct cmdsys*, size_t, const struct cmdarg**, void*),
   void* data,
   void (*completion)
    (struct cmdsys*, const char*, size_t, size_t*, const char**[]),
   const struct cmdarg_desc argv_desc[],
   const char* description)
{
  struct cmd* cmd = NULL;
  size_t argc = 0;
  size_t buffer_len = 0;
  size_t arg_id = 0;
  size_t desc_id = 0;
  size_t i = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  enum sl_error sl_err = SL_NO_ERROR;

  if(!sys || !name || !func) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }

  cmd = MEM_CALLOC(sys->allocator, 1, sizeof(struct cmd));
  if(NULL == cmd) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  list_init(&cmd->node);
  cmd->func = func;
  cmd->data = data;

  /* Check arg desc list */
  if(argv_desc != NULL) {
    for(argc = 0; !IS_END_REACHED(argv_desc[argc]); ++argc) {
      if(argv_desc[argc].min_count > argv_desc[argc].max_count
      || argv_desc[argc].max_count == 0
      || argv_desc[argc].type == CMDARG_TYPES_COUNT) {
        err = CMDSYS_INVALID_ARGUMENT;
        goto error;
      }
      buffer_len += argv_desc[argc].max_count;
    }
  }
  ++argc; /* +1 <=> command name. */

  /* Create the command arg table, arg domain,  and argv container. */
  cmd->arg_table = MEM_CALLOC
    (sys->allocator, argc + 1 /* +1 <=> arg_end */, sizeof(void*));
  if(NULL == cmd->arg_table) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  cmd->arg_domain = MEM_CALLOC
    (sys->allocator, argc, sizeof(union cmdarg_domain));
  if(NULL == cmd->arg_domain) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  cmd->argv = MEM_CALLOC(sys->allocator, argc, sizeof(struct cmdarg*));
  if(NULL == cmd->argv) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }

  /* Setup the arg domain and table. */
  err = init_domain_and_table(cmd, argv_desc);
  if(err != CMDSYS_NO_ERROR)
    goto error;

  /* Setup the command name arg. */
  cmd->argv[0] = MEM_CALLOC
    (sys->allocator, 1, sizeof(struct cmdarg) + sizeof(struct cmdarg_value));
  if(NULL == cmd->argv[0]) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  cmd->argv[0]->type = CMDARG_STRING;
  cmd->argv[0]->count = 1;

  /* Setup the remaining args. */
  for(arg_id = 1, desc_id = 0; arg_id < argc; ++arg_id, ++desc_id) {
    cmd->argv[arg_id] = MEM_CALLOC
      (sys->allocator, 1,
       sizeof(struct cmdarg)
       + argv_desc[desc_id].max_count * sizeof(struct cmdarg_value));
    if(NULL == cmd->argv[arg_id]) {
      err = CMDSYS_MEMORY_ERROR;
      goto error;
    }
    cmd->argv[arg_id]->type = argv_desc[desc_id].type;
    cmd->argv[arg_id]->count = argv_desc[desc_id].max_count;
  }
  cmd->argc = argc;

  /* Setup the command description. */
  if(NULL == description) {
    cmd->description = NULL;
  } else {
    sl_err = sl_create_string(description, sys->allocator, &cmd->description);
    if(sl_err != SL_NO_ERROR) {
      err = sl_to_cmdsys_error(sl_err);
      goto error;
    }
  }
  cmd->completion = completion;

  /* Register the command against the command system. */
  err = register_command(sys, cmd, name);
  if(err != CMDSYS_NO_ERROR)
    goto error;

exit:
  return err;
error:
  /* The command registration is the last action, i.e. the command is
   * registered only if no error occurs. It is thus useless to handle command
   * registration in the error management. */
  if(cmd) {
    if(cmd->description)
      SL(free_string(cmd->description));
    if(cmd->arg_table) {
      for(i = 0; i < argc + 1 /* +1 <=> arg_end */ ; ++i) {
        if(cmd->arg_table[i])
          free(cmd->arg_table[i]);
      }
      MEM_FREE(sys->allocator, cmd->arg_table);
    }
    if(cmd->argv) {
      for(i = 0; i < argc; ++i) {
        if(cmd->argv[i])
          free(cmd->argv[i]);
      }
      MEM_FREE(sys->allocator, cmd->argv);
    }
    MEM_FREE(sys->allocator, cmd);
    cmd = NULL;
  }
  goto exit;
}

enum cmdsys_error
cmdsys_del_command(struct cmdsys* sys, const char* name)
{
  struct sl_pair pair;
  struct list_node* list = NULL;
  struct list_node* pos = NULL;
  struct list_node* tmp = NULL;
  char* cmd_name = NULL;
  size_t i = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;

  if(!sys || !name) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }

  /* Unregister the command. */
  SL(hash_table_find_pair(sys->htbl, &name, &pair));
  if(!SL_IS_PAIR_VALID(&pair)) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }

  /* Free the command syntaxes. */
  list = (struct list_node*)pair.data;
  LIST_FOR_EACH_SAFE(pos, tmp, list) {
    struct cmd* cmd = CONTAINER_OF(pos, struct cmd, node);
    list_del(pos);

    if(cmd->description)
      SL(free_string(cmd->description));
    for(i = 0; i < cmd->argc; ++i) {
      MEM_FREE(sys->allocator, cmd->argv[i]);
    }
    MEM_FREE(sys->allocator, cmd->argv);
    MEM_FREE(sys->allocator, cmd->arg_domain);
    arg_freetable(cmd->arg_table, cmd->argc + 1); /* +1 <=> arg_end. */
    MEM_FREE(sys->allocator, cmd->arg_table);
    MEM_FREE(sys->allocator, cmd);
  }

  /* Free the command name. */
  cmd_name = *(char**)pair.key;
  SL(flat_set_erase(sys->name_set, &cmd_name, NULL));
  SL(hash_table_erase(sys->htbl, &cmd_name, &i));
  ASSERT(1 == i);
  MEM_FREE(sys->allocator, cmd_name);

exit:
  return err;
error:
  goto exit;
}

enum cmdsys_error
cmdsys_has_command(struct cmdsys* sys, const char* name, bool* has_command)
{
  void* ptr = NULL;
  if(!sys || !name || !has_command)
    return CMDSYS_INVALID_ARGUMENT;
  SL(hash_table_find(sys->htbl, &name, &ptr));
  *has_command = (ptr != NULL);
  return CMDSYS_NO_ERROR;
}

enum cmdsys_error
cmdsys_execute_command
  (struct cmdsys* sys,
   const char* command,
   const char* inverse)
{
  (void)inverse;

  #define MAX_ARG_COUNT 128
  char* argv[MAX_ARG_COUNT];
  struct list_node* command_list = NULL;
  struct list_node* node = NULL;
  struct cmd* valid_cmd = NULL;
  char* name = NULL;
  char* ptr = NULL;
  size_t argc = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  int min_nerror = 0;

  if(!sys || !command) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }
  if(strlen(command) + 1 > sizeof(sys->scratch) / sizeof(char)) {
    err = CMDSYS_MEMORY_ERROR;
    goto error;
  }
  /* Copy the command into the mutable scratch buffer of the command system. */
  strcpy(sys->scratch, command);

  /* Retrieve the first token <=> command name. */
  name = strtok(sys->scratch, " \t");
  SL(hash_table_find(sys->htbl, &name, (void**)&command_list));
  if(!command_list) {
    errbuf_print(&sys->errbuf, "%s: command not found\n", name);
    err = CMDSYS_COMMAND_ERROR;
    goto error;
  }
  argv[0] = name;
  for(argc = 1; NULL != (ptr = strtok(NULL, " \t")); ++argc) {
    if(argc >= MAX_ARG_COUNT) {
      err = CMDSYS_MEMORY_ERROR;
      goto error;
    }
    argv[argc] = ptr;
  }

  min_nerror = INT_MAX;
  LIST_FOR_EACH(node, command_list) {
    struct cmd* cmd = CONTAINER_OF(node, struct cmd, node);
    int nerror = 0;

    ASSERT(cmd->argc > 0);
    nerror = arg_parse(argc, argv, cmd->arg_table);

    if(nerror <= min_nerror) {
      if(nerror < min_nerror) {
        min_nerror = nerror;
        rewind(sys->stream);
      }
      fprintf(sys->stream, "\n%s", name);
      arg_print_syntaxv(sys->stream, cmd->arg_table, "\n");
      arg_print_errors
        (sys->stream, (struct arg_end*)cmd->arg_table[cmd->argc - 1], name);
    }
    if(nerror == 0) {
      valid_cmd = cmd;
      break;
    }
  }

  if(min_nerror != 0) {
    long fpos = 0;
    size_t size =0;
    size_t nb = 0;
    (void)nb;

    fpos = ftell(sys->stream);
    size = MIN((size_t)fpos, sizeof(sys->scratch)/sizeof(char) - 1);
    rewind(sys->stream);
    nb = fread(sys->scratch, size, 1, sys->stream);
    ASSERT(nb == 1);
    sys->scratch[size] = '\0';
    errbuf_print(&sys->errbuf, "%s", sys->scratch);
    err = CMDSYS_COMMAND_ERROR;
    goto error;
  }

  /* Setup the args and invoke the commands. */
  err = setup_cmd_arg(sys, valid_cmd, name);
  if(err != CMDSYS_NO_ERROR)
    goto error;

  valid_cmd->func
    (sys,
     valid_cmd->argc,
     (const struct cmdarg**)valid_cmd->argv,
     valid_cmd->data);

  #undef MAX_ARG_COUNT
exit:
  return err;
error:
  goto exit;
}

enum cmdsys_error
cmdsys_man_command
  (struct cmdsys* sys,
   const char* name,
   size_t* len,
   size_t max_buf_len,
   char* buffer)
{
  struct list_node* cmd_list = NULL;
  struct list_node* node = NULL;
  enum cmdsys_error err = CMDSYS_NO_ERROR;
  long fpos = 0;

  if(!sys || !name || (max_buf_len && !buffer)) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }

  SL(hash_table_find(sys->htbl, &name, (void**)&cmd_list));
  if(!cmd_list) {
    errbuf_print(&sys->errbuf, "%s: command not found\n", name);
    err = CMDSYS_COMMAND_ERROR;
    goto error;
  }
  ASSERT(is_list_empty(cmd_list) == false);

  rewind(sys->stream);
  LIST_FOR_EACH(node, cmd_list) {
    struct cmd* cmd = CONTAINER_OF(node, struct cmd, node);

    if(node != list_head(cmd_list))
       fprintf(sys->stream, "\n");

    fprintf(sys->stream, "%s", name);
    arg_print_syntaxv(sys->stream, cmd->arg_table, "\n");
    if(cmd->description) {
      const char* cstr = NULL;
      SL(string_get(cmd->description, &cstr));
      fprintf(sys->stream, "%s\n", cstr);
    }
    arg_print_glossary(sys->stream, cmd->arg_table, NULL);
    fpos = ftell(sys->stream);
    ASSERT(fpos > 0);
  }

  if(len)
    *len = fpos / sizeof(char);
  if(buffer && max_buf_len) {
    const size_t size = MIN((max_buf_len - 1) * sizeof(char), (size_t)fpos);
    size_t nb = 0;
    (void)nb;

    fflush(sys->stream);
    rewind(sys->stream);
    nb = fread(buffer, size, 1, sys->stream);
    ASSERT(nb == 1);
    buffer[size/sizeof(char)] = '\0';
  }
exit:
  return err;
error:
  goto exit;
}

enum cmdsys_error
cmdsys_command_arg_completion
  (struct cmdsys* sys,
   const char* cmd_name,
   const char* arg_str,
   size_t arg_str_len,
   size_t hint_argc,
   char* hint_argv[],
   size_t* completion_list_len,
   const char** completion_list[])
{
  struct list_node* cmd_list = NULL;
  enum cmdsys_error err = CMDSYS_NO_ERROR;

  if(!sys
  || !cmd_name
  || (arg_str_len && !arg_str)
  || (hint_argc && !hint_argv)
  || !completion_list_len
  || !completion_list) {
    err =  CMDSYS_INVALID_ARGUMENT;
    goto error;
  }
  /* Default completion list value. */
  *completion_list_len = 0;
  *completion_list = NULL;

  SL(hash_table_find(sys->htbl, &cmd_name, (void**)&cmd_list));
  if(cmd_list != NULL) {
    struct cmd* cmd = NULL;
    ASSERT(is_list_empty(cmd_list) == false);

    /* No multi syntax. */
    if(list_head(cmd_list) == list_tail(cmd_list)) {
      cmd = CONTAINER_OF(list_head(cmd_list), struct cmd, node);
      if(cmd->completion) {
        cmd->completion
          (sys, arg_str, arg_str_len, completion_list_len, completion_list);
      }
    /* Multi syntax. */
    } else {
      struct cmd* valid_cmd = NULL;
      struct list_node* node = NULL;
      size_t nb_valid_cmd = 0;
      int min_nerror = INT_MAX;
      int max_ndefargs = INT_MIN;

      LIST_FOR_EACH(node, cmd_list) {
        int nerror = 0;
        int ndefargs = 0;

        cmd = CONTAINER_OF(node, struct cmd, node);
        set_optvalue_flag(cmd, true);
        nerror = arg_parse(hint_argc, hint_argv, cmd->arg_table);
        ndefargs = defined_args_count(cmd);

        /* Define as the completion function the one defined by the command
         * syntax which match the best the hint arguments. If the minimal
         * number of parsing error is obtained by several syntaxes, we select
         * the syntax which have the maximum of its argument defined by the
         * hint command. */
        if(nerror < min_nerror
        || (nerror == min_nerror && ndefargs > max_ndefargs)) {
          valid_cmd = cmd;
          nb_valid_cmd = 0;
        }
        min_nerror = MIN(nerror, min_nerror);
        max_ndefargs = MAX(ndefargs, max_ndefargs);
        nb_valid_cmd += ((ndefargs == max_ndefargs) & (nerror == min_nerror));
        set_optvalue_flag(cmd, false);
      }
      /* Perform the completion only if an unique syntax match the previous
       * completion heuristic and and if its completion process is defined. */
      if(nb_valid_cmd == 1 && valid_cmd->completion) {
        valid_cmd->completion
          (sys, arg_str, arg_str_len, completion_list_len, completion_list);
      }
    }
  }
  if(*completion_list_len == 0)
    *completion_list = NULL;
exit:
  return err;
error:
  if(completion_list_len)
    *completion_list_len = 0;
  if(completion_list)
    *completion_list = NULL;
  goto exit;
}

enum cmdsys_error
cmdsys_command_name_completion
  (struct cmdsys* sys,
   const char* cmd_name,
   size_t cmd_name_len,
   size_t* completion_list_len,
   const char** completion_list[])
{
  const char** name_list = NULL;
  size_t len = 0;
  enum cmdsys_error err = CMDSYS_NO_ERROR;

  if(!sys
  || (cmd_name_len && !cmd_name)
  || !completion_list_len
  || !completion_list) {
    err = CMDSYS_INVALID_ARGUMENT;
    goto error;
  }
  SL(flat_set_buffer(sys->name_set, &len, NULL, NULL, (void**)&name_list));
  if(0 == cmd_name_len) {
    *completion_list_len = len;
    *completion_list = name_list;
  } else {
    #define CHARBUF_SIZE 32
    char buf[CHARBUF_SIZE];
    const char* ptr = buf;
    size_t begin = 0;
    size_t end = 0;

    if(cmd_name_len > CHARBUF_SIZE - 1) {
      err = CMDSYS_MEMORY_ERROR;
      goto error;
    }
    strncpy(buf, cmd_name, cmd_name_len);
    buf[cmd_name_len] = '\0';
    SL(flat_set_lower_bound(sys->name_set, &ptr, &begin));
    buf[cmd_name_len] = 127;
    buf[cmd_name_len + 1] = '\0';
    SL(flat_set_upper_bound(sys->name_set, &ptr, &end));
    *completion_list = name_list + begin;
    *completion_list_len = (name_list + end) - (*completion_list);
    if(0 == *completion_list_len)
      *completion_list = NULL;
    #undef CHARBUF_SIZE
  }
exit:
  return err;
error:
  goto exit;
}
