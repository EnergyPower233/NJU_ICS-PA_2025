/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include "sdb.h"
#include <cpu/cpu.h>
#include <isa.h>
#include <readline/history.h>
#include <readline/readline.h>
// #include <stdlib.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the 'readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1; // cause the exit of mainloop
}

static int cmd_help(char *args);

static int cmd_si(char *args) {
  if (args == NULL) {
    cpu_exec(1);
    return 0;
  }
  uint64_t steps = (uint64_t)(strtol(args, NULL, 10));
  if (steps)
    cpu_exec(steps);
  else
    printf("Invalid arguments: '%s'\n", args);
  return 0;
}

static int cmd_info(char *args) {
  if (args == NULL) {
    printf("Too few arguments: Command lose\nUsage:\ninfo r //check register "
           "info\ninfo w //check watchpoint info\n");
    return 0;
  }
  // Log Register status
  if (strcmp(args, "r") == 0)
    isa_reg_display();
  // Log Watchpoint status
  else if (strcmp(args, "w") == 0) {

  } else
    printf("Unknown command '%s'\n", args);
  return 0;
}

static int cmd_x(char *args) {
  /*
  uint64_t N = (uint64_t)(strtol(args, NULL, 10));
  char* expr = strtok(args, " ");
  */
  return 0;
}
static int cmd_p(char *args) {
  if (args == NULL) {
    printf("Usage: p EXPR\n");
    return 0;
  }
  bool success = true;
  word_t result = expr(args, &success);
  if (success) {
    printf("%u (0x%8x)\n", result, result);
  } else {
    printf("Invalid expression\n");
  }

  return 0;
}
static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},
    {"si",
     "Step forward N instructions, then suspend execution. If N is not "
     "specified, it defaults to 1.",
     cmd_si},
    {"info", "Print status of Register or Watchpoint", cmd_info},
    {"x", "Scan memory", cmd_x},
    {"p", "Evaluate the expression", cmd_p},
    /* TODO: Add more commands */
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        // return if handler returns negative value
        if (cmd_table[i].handler(args) < 0) {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}

/*test the expression evaluation */
void test_expression() {
  FILE *fp = fopen("/home/epower/ics2025/nemu/tools/gen-expr/input", "r");
  assert(fp != NULL);
  char expression[65536];
  unsigned expected;
  int cnt = 0;
  int pass = 0;
  bool AP = true;                             // All Passed
  while (fscanf(fp, "%u ", &expected) == 1) { // read the result
    if (fgets(expression, sizeof(expression), fp) == NULL)
      break; // The End of the file
    expression[strcspn(expression, "\n")] =
        '\0'; // Replace the \n to \0 to make sure the expression string is
              // valid
    bool success = true;
    unsigned result = expr(expression, &success);
    ++cnt;
    if (success && result == expected) {
      ++pass;
    } else {
      printf("FAILED on expression: %s\nExpected: %u  But got: %u", expression,
             expected, result);
      AP = false;
    }
  }
  fclose(fp);
  if (AP)
    printf("%d test cases passed. No cases failed.", pass);
  else
    printf("Test: %d/%d passed\n", pass, cnt);
}