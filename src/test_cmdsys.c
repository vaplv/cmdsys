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
#include "cmdsys.h"
#include "sys/mem_allocator.h"
#include "sys/sys.h"
#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define BAD_ARG CMDSYS_INVALID_ARGUMENT
#define CMD_ERR CMDSYS_COMMAND_ERROR
#define OK CMDSYS_NO_ERROR

static void
foo
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  (void)sys;

  CHECK(argc > 0 && argc < 3, true);
  CHECK(data, NULL);
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  if(argc > 1) {
    CHECK(argv[1]->type, CMDARG_LITERAL);
    CHECK(argv[1]->count, 1);
    CHECK(argv[1]->value_list[0].is_defined, true);
  }
  if(argc > 1) {
    printf("verbose %s\n", argv[0]->value_list[0].data.string);
  } else {
    printf("%s\n", argv[0]->value_list[0].data.string);
  }
  CHECK(strcmp(argv[0]->value_list[0].data.string, "__foo"), 0);
}

static const char* load_name__ = NULL;
static const char* load_model__ = NULL;
static bool load_verbose_opt__ = false;
static bool load_name_opt__ = false;

static void
load
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  (void)sys;

  CHECK(argc, 4);
  CHECK(data, NULL);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  /* Verbose option. */
  CHECK(argv[1]->type, CMDARG_LITERAL);
  CHECK(argv[1]->count, 1);
  CHECK(argv[1]->value_list[0].is_defined, load_verbose_opt__);
  /* Model path option. */
  CHECK(argv[2]->type, CMDARG_FILE);
  CHECK(argv[2]->count, 1);
  CHECK(argv[2]->value_list[0].is_defined, true);
  /* Model name option. */
  CHECK(argv[3]->type, CMDARG_STRING);
  CHECK(argv[3]->count, 1);
  CHECK(argv[3]->value_list[0].is_defined, load_name_opt__);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__load"), 0);

  if(argv[1]->value_list[0].is_defined)
    printf("verbose ");

  CHECK(strcmp(argv[2]->value_list[0].data.string, load_model__), 0);
  printf("%s MODEL==%s",
         argv[0]->value_list[0].data.string,
         argv[2]->value_list[0].data.string);

  if(argv[3]->value_list[0].is_defined) {
    CHECK(strcmp(argv[3]->value_list[0].data.string, load_name__), 0);
    printf(" NAME=%s\n", argv[3]->value_list[0].data.string);
  } else {
    printf("\n");
  }
}

static bool setf3_r_opt__ = false;
static bool setf3_g_opt__ = false;
static bool setf3_b_opt__ = false;
static float setf3_r__ = 0.f;
static float setf3_g__ = 0.f;
static float setf3_b__ = 0.f;

static void
setf3
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  (void)sys;

  CHECK(argc, 4);
  CHECK(data, NULL);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  /* Red option. */
  CHECK(argv[1]->type, CMDARG_FLOAT);
  CHECK(argv[1]->count, 1);
  CHECK(argv[1]->value_list[0].is_defined, setf3_r_opt__);
  /* Green option. */
  CHECK(argv[2]->type, CMDARG_FLOAT);
  CHECK(argv[2]->count, 1);
  CHECK(argv[2]->value_list[0].is_defined, setf3_g_opt__);
  /* Blue option. */
  CHECK(argv[3]->type, CMDARG_FLOAT);
  CHECK(argv[3]->count, 1);
  CHECK(argv[3]->value_list[0].is_defined, setf3_b_opt__);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__setf3"), 0);
  if(argv[1]->value_list[0].is_defined)
    CHECK(argv[1]->value_list[0].data.real, setf3_r__);
  if(argv[2]->value_list[0].is_defined)
    CHECK(argv[2]->value_list[0].data.real, setf3_g__);
  if(argv[3]->value_list[0].is_defined)
    CHECK(argv[3]->value_list[0].data.real, setf3_b__);
}


#define MAX_DAY_COUNT 3
static const char* day_list__[MAX_DAY_COUNT];
static size_t day_count__ = 0;

static const char* days[] = {
  "Friday",
  "Monday",
  "Saturday",
  "Sunday",
  "Thursday",
  "Tuesday",
  "Wednesday",
  NULL
};

static void
day_completion
  (struct cmdsys* sys,
   const char* input,
   size_t input_len,
   size_t* completion_list_len,
   const char** completion_list[])
{
  const size_t len = sizeof(days)/sizeof(const char*) - 1; /* -1 <=> NULL. */
  (void)sys;

  ASSERT
    (  sys
    && (!input_len || input)
    && completion_list_len
    && completion_list);

  if(!input || !input_len) {
    *completion_list_len = len;
    *completion_list = days;
  } else {
    size_t i = 0;
    size_t j = 0;
    for(i = 0; i < len && strncmp(days[i], input, input_len) < 0; ++i);
    for(j = i; j < len && strncmp(days[j], input, input_len) == 0; ++j);
    *completion_list = days + i;
    *completion_list_len = j - i;
  }
}

static const char* monthes[] = {
  "April",
  "August",
  "December"
  "February",
  "January",
  "July",
  "June",
  "March",
  "May",
  "November",
  "October",
  "September",
  NULL
};

static void
month_completion
  (struct cmdsys* sys,
   const char* input,
   size_t input_len,
   size_t* completion_list_len,
   const char** completion_list[])
{
  const size_t len = sizeof(monthes)/sizeof(const char*) - 1; /* -1 <=> NULL. */

  (void)sys;
  ASSERT
    (  sys
    && (!input_len || input)
    && completion_list_len
    && completion_list);

  if(!input || !input_len) {
    *completion_list_len = len;
    *completion_list = monthes;
  } else {
    size_t i = 0;
    size_t j = 0;
    for(i = 0; i < len && strncmp(monthes[i], input, input_len) < 0; ++i);
    for(j = i; j < len && strncmp(monthes[j], input, input_len) == 0; ++j);
    *completion_list = monthes + i;
    *completion_list_len = j - i;
  }
}

static void
day
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  size_t i = 0;

  (void)sys;

  CHECK(argc, 2);
  CHECK(data, NULL);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  /* Filter name. */
  CHECK(argv[1]->type, CMDARG_STRING);
  CHECK(argv[1]->count, MAX_DAY_COUNT);
  CHECK(argv[1]->value_list[0].is_defined, true);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__day"), 0);

  for(i = 0; i < argv[1]->count && argv[1]->value_list[i].is_defined; ++i) {
    size_t j = 0;

    CHECK(strcmp(argv[1]->value_list[i].data.string, day_list__[i]), 0);
    for(j = 0; days[j]; ++j) {
      if(strcmp(days[j], argv[1]->value_list[i].data.string) == 0)
        break;
    }
    NCHECK(days[i], NULL);
  }
  CHECK(i, day_count__);
}

static void
date_day
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  const size_t len = sizeof(days)/sizeof(const char*) - 1; /* -1 <=> NULL. */
  size_t i = 0;

  (void)sys;

  NCHECK(sys, NULL);
  CHECK(argc, 2);
  CHECK(data, NULL);
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);

  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__date"), 0);
  for(i =0; i < len; ++i) {
    if(strcmp(argv[1]->value_list[1].data.string, days[i]) == 0)
       break;
  }
  NCHECK(i, len);
}

static void
date_month
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  const size_t len = sizeof(monthes)/sizeof(const char*) - 1; /* -1 <=> NULL. */
  size_t i = 0;

  (void)sys;

  NCHECK(sys, NULL);
  CHECK(argc, 2);
  CHECK(data, NULL);
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);

  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__date"), 0);
  for(i =0; i < len; ++i) {
    if(strcmp(argv[1]->value_list[1].data.string, monthes[i]) == 0)
       break;
  }
  NCHECK(i, len);
}

#define MAX_FILE_COUNT 4
static const char* cat_file_list__[MAX_FILE_COUNT];
static size_t cat_file_count__ = 0;

static void
cat
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  size_t i = 0;
  (void)sys;

  CHECK(argc, 2);
  CHECK(data, NULL);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  /* Filter name. */
  CHECK(argv[1]->type, CMDARG_FILE);
  CHECK(argv[1]->count, MAX_FILE_COUNT);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__cat"), 0);
  for(i = 0; i < argv[1]->count && argv[1]->value_list[i].is_defined; ++i) {
    CHECK(strcmp(argv[1]->value_list[i].data.string, cat_file_list__[i]), 0);
  }
  CHECK(i, cat_file_count__);
}

#define MAX_INT_COUNT 3
static int seti_int_list__[MAX_INT_COUNT];
static int seti_int_count__ = 0;

static void
seti
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  int i = 0;
  (void)sys;

  CHECK(argc, 2);
  CHECK(data, NULL);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);
  /* Filter name. */
  CHECK(argv[1]->type, CMDARG_INT);
  CHECK(argv[1]->count, MAX_INT_COUNT);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__seti"), 0);
  for(i=0; (size_t)i<argv[1]->count && argv[1]->value_list[i].is_defined; ++i) {
    CHECK(argv[1]->value_list[i].data.integer, seti_int_list__[i]);
  }
  CHECK(i, seti_int_count__);
}

static void
print
  (struct cmdsys* sys,
   size_t argc,
   const struct cmdarg** argv,
   void* data)
{
  (void)sys;

  CHECK(argc, 1);
  NCHECK(data, NULL);
  CHECK(strcmp((char*)data, "hello world!"), 0);
  /* Command name. */
  CHECK(argv[0]->type, CMDARG_STRING);
  CHECK(argv[0]->count, 1);
  CHECK(argv[0]->value_list[0].is_defined, true);

  CHECK(strcmp(argv[0]->value_list[0].data.string, "__print"), 0);
}

int
main(int argc, char **argv)
{
  char buf[16] = { [0] = '\0' };
  struct cmdsys* sys = NULL;
  const char** lst = NULL;
  size_t len = 0;
  bool b = false;

  (void)argc;
  (void)argv;

  CHECK(cmdsys_create(NULL, NULL), BAD_ARG);
  CHECK(cmdsys_create(NULL, &sys), OK);

  CHECK(cmdsys_add_command(NULL, NULL, NULL, NULL, NULL, NULL, NULL),BAD_ARG);
  CHECK(cmdsys_add_command(sys, NULL, NULL, NULL, NULL, NULL, NULL),BAD_ARG);
  CHECK(cmdsys_add_command(NULL, NULL, NULL, NULL, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_add_command(sys, "__foo", NULL, NULL, NULL, NULL, NULL),BAD_ARG);
  CHECK(cmdsys_add_command(NULL, NULL, foo, NULL, NULL, NULL, NULL),BAD_ARG);
  CHECK(cmdsys_add_command(sys, NULL, foo, NULL, NULL, NULL, NULL),BAD_ARG);
  CHECK(cmdsys_add_command(NULL, NULL, foo, NULL, NULL, NULL, NULL),BAD_ARG);

  CHECK(cmdsys_add_command(sys, "__foo", foo, NULL, NULL, NULL, NULL), OK);
  CHECK(cmdsys_add_command(sys, "__foo", foo, NULL, NULL, NULL, NULL), OK);
  CHECK(cmdsys_add_command
    (sys, "__foo", foo, NULL, NULL, CMDARGV(
      CMDARG_APPEND_LITERAL("vV", "verbose, verb", NULL, 1, 1),
      CMDARG_END),
     NULL),
    OK);

  CHECK(cmdsys_execute_command(NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_execute_command(sys, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_execute_command(NULL, "__foox", NULL), BAD_ARG);
  CHECK(cmdsys_execute_command(sys, "__foox", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__foo", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__foo -v", NULL), OK);

  CHECK(cmdsys_del_command(NULL, NULL), BAD_ARG);
  CHECK(cmdsys_del_command(sys, NULL), BAD_ARG);
  CHECK(cmdsys_del_command(NULL, "__foox"), BAD_ARG);
  CHECK(cmdsys_del_command(sys, "__foox"), BAD_ARG);
  CHECK(cmdsys_del_command(sys, "__foo"), OK);
  CHECK(cmdsys_execute_command(sys, "__foo", NULL), CMD_ERR);

  CHECK(cmdsys_add_command
    (sys, "__foo", foo, NULL, NULL,
     CMDARGV
      (CMDARG_APPEND_LITERAL("vV", "verb,verbose", NULL, 0, 1),
       CMDARG_END),
      NULL),
    OK);
  CHECK(cmdsys_execute_command(sys, "__foo -v", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__foo --verbose", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__foo -verbose", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__foo -V", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__foo --verb", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__foo -vV", NULL), CMD_ERR);
  CHECK(cmdsys_del_command(sys, "__foo"), OK);

  CHECK(cmdsys_add_command
    (sys, "__load", load, NULL, NULL,
     CMDARGV
      (CMDARG_APPEND_LITERAL("vV", "verbose,verb", "verbosity", 0, 1),
       CMDARG_APPEND_FILE("mM", "model,mdl", "<model-path>", NULL, 1, 1),
       CMDARG_APPEND_STRING("n", "name", "<model-name>", NULL,0,1, NULL),
       CMDARG_END),
      "load a model"),
    OK);

  load_verbose_opt__ = false;
  load_name_opt__ = false;
  load_model__ = "\"my_model.obj\"";
  CHECK(cmdsys_execute_command(sys, "__load", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__load -m \"my_model.obj\"", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__load -M \"my_model.obj\"", NULL), OK);

  load_verbose_opt__ = true;
  CHECK(cmdsys_execute_command
    (sys, "__load --verbose -m \"my_model.obj\"", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__load -vm \"my_model.obj\"", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__load -m \"my_model.obj\" -V", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__load -mv \"my_model.obj\"", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command
    (sys, "__load -vm \"my_model.obj\"", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__load -vVm \"my_model.obj\"", NULL), CMD_ERR);

  load_name_opt__ = true;
  load_name__ = "\"foo\"";
  CHECK(cmdsys_execute_command
    (sys, "__load -m \"my_model.obj\" -V --name=\"foo\"", NULL), OK);
  load_name__ = "\"HelloWorld\"";
  CHECK(cmdsys_execute_command
    (sys, "__load --model \"my_model.obj\" --verb -n \"HelloWorld\"", NULL),OK);

  load_verbose_opt__ = false;
  CHECK(cmdsys_execute_command
    (sys, "__load --model \"my_model.obj\" -n ", NULL), CMD_ERR);
  load_name__ = "=my_name";
  CHECK(cmdsys_execute_command
    (sys, "__load -n=my_name --model \"my_model.obj\"", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__load -n=my_name --model \"my_model.obj\" --name my_name2", NULL),
     CMD_ERR);
  CHECK(cmdsys_execute_command
    (sys, "__load -n=my_name --model \"my_model.obj\" -m model0", NULL),
     CMD_ERR);
  CHECK(cmdsys_execute_command
    (sys, "__load -n=my_name --model \"my_model.obj\"", NULL), OK);

  CHECK(cmdsys_man_command(NULL, NULL, NULL, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, NULL, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load", NULL, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", NULL, 0, NULL), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", NULL, 0, NULL), OK);
  CHECK(cmdsys_man_command(NULL, NULL, &len, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, &len, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load_", &len, 0, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", &len, 0, NULL), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", &len, 0, NULL), OK);
  CHECK(cmdsys_man_command(NULL, NULL, NULL, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, NULL, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load", NULL, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", NULL, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "__load", NULL, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, NULL, &len, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, &len, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load_", &len, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", &len, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "__load", &len, 8, NULL), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, NULL, NULL, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, NULL, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load", NULL, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", NULL, 0, buf), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", NULL, 0, buf), OK);
  CHECK(cmdsys_man_command(NULL, NULL, &len, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, &len, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load_", &len, 0, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", &len, 0, buf), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", &len, 0, buf), OK);
  printf("%s", buf);
  CHECK(strlen(buf) <= len, true);
  CHECK(cmdsys_man_command(NULL, NULL, NULL, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, NULL, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load", NULL, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", NULL, 8, buf), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", NULL, 8, buf), OK);
  printf("%s\n", buf);
  CHECK(strlen(buf) <= len, true);
  CHECK(cmdsys_man_command(NULL, NULL, &len, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, NULL, &len, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(NULL, "_load_", &len, 8, buf), BAD_ARG);
  CHECK(cmdsys_man_command(sys, "_load_", &len, 8, buf), CMD_ERR);
  CHECK(cmdsys_man_command(sys, "__load", &len, 8, buf), OK);
  printf("%s\n", buf);
  CHECK(strlen(buf) <= len, true);
  CHECK(cmdsys_man_command(sys, "__load", &len, 16, buf), OK);
  printf("%s\n", buf);
  CHECK(strlen(buf) <= len, true);


  CHECK(cmdsys_add_command
    (sys, "__setf3", setf3, NULL, NULL,
     CMDARGV
      (CMDARG_APPEND_FLOAT
        ("r", "red", "<real>", "red value", 0, 1, 0.f, 1.f),
       CMDARG_APPEND_FLOAT
        ("g", "green", "<real>", "green value", 0, 1, 0.f, 1.f),
       CMDARG_APPEND_FLOAT
        ("b", "blue", "<real>", "blue value", 0, 1, 0.f, 1.f),
       CMDARG_END
      ),
      NULL),
    OK);

  setf3_r_opt__ = false;
  setf3_g_opt__ = false;
  setf3_b_opt__ = false;
  setf3_r__ = 0.f;
  setf3_g__ = 0.f;
  setf3_b__ = 0.f;
  CHECK(cmdsys_execute_command(sys, "__setf3", NULL), OK);
  setf3_g_opt__ = true;
  setf3_g__ = 1.0f;
  CHECK(cmdsys_execute_command(sys, "__setf3 -g 5.1", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__setf3 -g 1.0", NULL), OK);
  setf3_g__ = 0.78f;
  CHECK(cmdsys_execute_command(sys, "__setf3 -g 0.78", NULL), OK);
  setf3_r_opt__ = true;
  setf3_r__ = 0.f;
  CHECK(cmdsys_execute_command(sys, "__setf3 -g 0.78 -r -1.5", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__setf3 --red=-1.5 -g 0.78", NULL), OK);
  setf3_b_opt__ = true;
  setf3_b__ = 0.5f;
  CHECK(cmdsys_execute_command
    (sys, "__setf3 --red=-1.5 --blue 0.5 -g 0.78", NULL), OK);
  CHECK(cmdsys_execute_command
    (sys, "__setf3 -r -1.5 -b 0.5e0 -g 0.78 -g 1", NULL), CMD_ERR);

  CHECK(cmdsys_add_command
    (sys, "__day", day, NULL, day_completion,
     CMDARGV
      (CMDARG_APPEND_STRING(NULL, NULL, "<day>", "day name", 1, 3, days),
       CMDARG_END),
      NULL),
    OK);
  day_count__ = 0;
  memset(day_list__, 0, sizeof(day_list__));

  day_count__ = 1;
  day_list__[0] = "foo";
  CHECK(cmdsys_execute_command(sys, "day", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__day", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__day foo", NULL), CMD_ERR);
  day_list__[0] = "Monday";
  CHECK(cmdsys_execute_command(sys, "__day Monday", NULL), OK);
  day_list__[0] = "Tuesday";
  CHECK(cmdsys_execute_command(sys, "__day Tuesday", NULL), OK);
  day_list__[0] = "Wednesday";
  CHECK(cmdsys_execute_command(sys, "__day Wednesday", NULL), OK);
  day_list__[0] = "Thursday";
  CHECK(cmdsys_execute_command(sys, "__day Thursday", NULL), OK);
  day_list__[0] = "Friday";
  CHECK(cmdsys_execute_command(sys, "__day Friday", NULL), OK);
  day_list__[0] = "Saturday";
  CHECK(cmdsys_execute_command(sys, "__day Saturday", NULL), OK);
  day_list__[0] = "Sunday";
  CHECK(cmdsys_execute_command(sys, "__day Sunday", NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__day sunday", NULL), CMD_ERR);
  day_count__++;
  day_list__[1] = "foo";
  CHECK(cmdsys_execute_command(sys, "__day Sunday foo", NULL), CMD_ERR);
  day_list__[0] = "foo0";
  day_list__[1] = "foo1";
  CHECK(cmdsys_execute_command(sys, "__day foo0 foo1", NULL), CMD_ERR);
  day_list__[0] = "Monday";
  day_list__[1] = "Friday";
  CHECK(cmdsys_execute_command(sys, "__day Monday Friday", NULL), OK);
  day_list__[day_count__++] = "Saturday";
  CHECK(cmdsys_execute_command(sys, "__day Monday Friday Saturday", NULL), OK);

  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, NULL, 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, NULL, 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", NULL, 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", NULL, 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, "S", 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, "S", 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", "S", 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "S", 0, 0, NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, NULL, 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, NULL, 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", NULL, 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", NULL, 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, "S", 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, "S", 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", "S", 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "S", 0, 0, NULL, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, NULL, 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, NULL, 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", NULL, 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", NULL, 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, "S", 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, "S", 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", "S", 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "S", 0, 0, NULL, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, NULL, 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, NULL, 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", NULL, 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", NULL, 0, 0, NULL, &len, &lst), OK);
  CHECK(len, 7);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Friday"), 0);
  CHECK(strcmp(lst[1], "Monday"), 0);
  CHECK(strcmp(lst[2], "Saturday"), 0);
  CHECK(strcmp(lst[3], "Sunday"), 0);
  CHECK(strcmp(lst[4], "Thursday"), 0);
  CHECK(strcmp(lst[5], "Tuesday"), 0);
  CHECK(strcmp(lst[6], "Wednesday"), 0);
  CHECK(cmdsys_command_arg_completion
    (NULL, NULL, "S", 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, NULL, "S", 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (NULL, "__day", "S", 0, 0, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "S", 0, 0, NULL, &len, &lst), OK);
  CHECK(len, 7);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Friday"), 0);
  CHECK(strcmp(lst[1], "Monday"), 0);
  CHECK(strcmp(lst[2], "Saturday"), 0);
  CHECK(strcmp(lst[3], "Sunday"), 0);
  CHECK(strcmp(lst[4], "Thursday"), 0);
  CHECK(strcmp(lst[5], "Tuesday"), 0);
  CHECK(strcmp(lst[6], "Wednesday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "Saxxx", 1, 0, NULL, &len, &lst), OK);
  CHECK(len, 2);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Saturday"), 0);
  CHECK(strcmp(lst[1], "Sunday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "Saxxx", 2, 0, NULL, &len, &lst), OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Saturday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "Saxxx", 3, 0, NULL, &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "Wed", 1, 0, NULL, &len, &lst), OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Wednesday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__day", "wed", 3, 0, NULL, &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);

  /* Multi syntax. */
  CHECK(cmdsys_add_command
    (sys, "__date", date_day, NULL, day_completion,
     CMDARGV
     (CMDARG_APPEND_STRING("d", "day", "<day>", NULL, 1, 1, NULL),
      CMDARG_END),
     NULL), OK);
  CHECK(cmdsys_add_command
    (sys, "__date", date_month, NULL, month_completion,
     CMDARGV
     (CMDARG_APPEND_STRING("m", "month", "<month>", NULL, 1, 1, NULL),
      CMDARG_END),
     NULL), OK);

  CHECK(cmdsys_command_arg_completion
    (sys, "__date", NULL, 0, 5, NULL, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date",
     NULL, 0,
     0, (char*[]){"foo"},
     &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);

  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 1,  1, (char*[]){"__date"}, &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 1, 1, (char*[]){"-d"}, &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 1, 2, (char*[]){"__date", "-d"}, &len, &lst), OK);
  CHECK(len, 2);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Thursday"), 0);
  CHECK(strcmp(lst[1], "Tuesday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 2, 2, (char*[]){"__date", "-d"}, &len, &lst), OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Thursday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 2, 3, (char*[]){"__date", "-d", "foo"}, &len, &lst),
     OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "Thursday"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Thu", 2, 2, (char*[]){"__date", "-m"} , &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Ju", 1, 2, (char*[]){"__date", "-m"} , &len, &lst), OK);
  CHECK(len, 3);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "January"), 0);
  CHECK(strcmp(lst[1], "July"), 0);
  CHECK(strcmp(lst[2], "June"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Ju", 2, 2, (char*[]){"__date", "-m"} , &len, &lst), OK);
  CHECK(len, 2);
  CHECK(strcmp(lst[0], "July"), 0);
  CHECK(strcmp(lst[1], "June"), 0);

  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Sep", 3, 2, (char*[]){"__date", "-m"} , &len, &lst), OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "September"), 0);
  CHECK(cmdsys_command_arg_completion
    (sys, "__date", "Sep", 3, 2, (char*[]){"__date", "-d"} , &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);

  CHECK(cmdsys_del_command(sys, "__date"), OK);

  CHECK(cmdsys_add_command
    (sys, "__cat", cat, NULL, NULL,
     CMDARGV
      (CMDARG_APPEND_FILE
       (NULL, NULL, "<file> ...", "files", 1, MAX_FILE_COUNT),
       CMDARG_END),
      NULL),
    OK);
  cat_file_count__ = 0;
  memset(cat_file_list__, 0, sizeof(cat_file_list__));

  CHECK(cmdsys_execute_command(sys, "__cat", NULL), CMD_ERR);
  cat_file_list__[cat_file_count__++] = "foo";
  CHECK(cmdsys_execute_command(sys, "__cat foo", NULL), OK);
  cat_file_list__[cat_file_count__++] = "hello";
  CHECK(cmdsys_execute_command(sys, "__cat foo hello", NULL), OK);
  cat_file_list__[cat_file_count__++] = "world";
  CHECK(cmdsys_execute_command(sys, "__cat foo hello world", NULL), OK);
  cat_file_list__[cat_file_count__++] = "test";
  CHECK(cmdsys_execute_command(sys, "__cat foo hello world test", NULL), OK);

  CHECK(cmdsys_add_command
    (sys, "__seti", seti, NULL, NULL,
     CMDARGV
      (CMDARG_APPEND_INT("-i", NULL, NULL, NULL, 1, MAX_INT_COUNT, 5, 7),
       CMDARG_END),
      "set a list of integer"),
    OK);
  seti_int_count__ = 0;
  memset(seti_int_list__, 0, sizeof(seti_int_list__));

  seti_int_list__[seti_int_count__++] = 5;
  CHECK(cmdsys_execute_command(sys, "__seti 0", NULL), CMD_ERR);
  CHECK(cmdsys_execute_command(sys, "__seti -i 0", NULL), OK);
  seti_int_list__[seti_int_count__++] = 5;
  CHECK(cmdsys_execute_command(sys, "__seti -i 0 -i -8", NULL), OK);
  seti_int_list__[seti_int_count__ - 1] = 6;
  CHECK(cmdsys_execute_command(sys, "__seti -i 0 -i 6", NULL), OK);
  seti_int_list__[seti_int_count__++] = 7;
  CHECK(cmdsys_execute_command(sys, "__seti -i 0 -i 6 -i 9", NULL), OK);

  CHECK(cmdsys_command_name_completion(NULL, NULL, 0, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, NULL, 0, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, "_", 0, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, "_", 0, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, NULL, 0, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, NULL, 0, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, "_", 0, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, "_", 0, &len, NULL), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, NULL, 0, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, NULL, 0, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, "_", 0, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, "_", 0, NULL, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, NULL, 0, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, NULL, 0, &len, &lst), OK);
  NCHECK(len, 0);
  NCHECK(lst, NULL);
  CHECK(cmdsys_command_name_completion(sys, NULL, 1, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(NULL, "_", 0, &len, &lst), BAD_ARG);
  CHECK(cmdsys_command_name_completion(sys, "_", 0, &len, &lst), OK);
  NCHECK(len, 0);
  NCHECK(lst, NULL);
  CHECK(cmdsys_command_name_completion(sys, "_", 1, &len, &lst), OK);
  CHECK(len, 5);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "__cat"), 0);
  CHECK(strcmp(lst[1], "__day"), 0);
  CHECK(strcmp(lst[2], "__load"), 0);
  CHECK(strcmp(lst[3], "__setf3"), 0);
  CHECK(strcmp(lst[4], "__seti"), 0);
  CHECK(cmdsys_command_name_completion(sys, "__s", 2, &len, &lst), OK);
  CHECK(len, 5);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "__cat"), 0);
  CHECK(strcmp(lst[1], "__day"), 0);
  CHECK(strcmp(lst[2], "__load"), 0);
  CHECK(strcmp(lst[3], "__setf3"), 0);
  CHECK(strcmp(lst[4], "__seti"), 0);
  CHECK(cmdsys_command_name_completion(sys, "__s", 3, &len, &lst), OK);
  CHECK(len, 2);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "__setf3"), 0);
  CHECK(strcmp(lst[1], "__seti"), 0);
  CHECK(cmdsys_command_name_completion(sys, "__lxxxx", 3, &len, &lst), OK);
  CHECK(len, 1);
  NCHECK(lst, NULL);
  CHECK(strcmp(lst[0], "__load"), 0);
  CHECK(cmdsys_command_name_completion(sys, "__lxxxx", 4, &len, &lst), OK);
  CHECK(len, 0);
  CHECK(lst, NULL);

  CHECK(cmdsys_has_command(NULL, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_has_command(sys, NULL, NULL), BAD_ARG);
  CHECK(cmdsys_has_command(NULL, "__seti", NULL), BAD_ARG);
  CHECK(cmdsys_has_command(sys, "__seti", NULL), BAD_ARG);
  CHECK(cmdsys_has_command(NULL, NULL, &b), BAD_ARG);
  CHECK(cmdsys_has_command(sys, NULL, &b), BAD_ARG);
  CHECK(cmdsys_has_command(NULL, "__seti", &b), BAD_ARG);
  CHECK(cmdsys_has_command(sys, "__seti", &b), OK);
  CHECK(b, true);

  CHECK(cmdsys_del_command(sys, "__seti"), OK);
  CHECK(cmdsys_has_command(sys, "__seti", &b), OK);
  CHECK(b, false);

  CHECK(cmdsys_add_command
    (sys, "__print", print, "hello world!", NULL, NULL, NULL), OK);
  CHECK(cmdsys_execute_command(sys, "__print", NULL), OK);

  CHECK(cmdsys_ref_put(sys), CMDSYS_NO_ERROR);

  CHECK(MEM_ALLOCATED_SIZE(&mem_default_allocator), 0);
  return 0;
}

