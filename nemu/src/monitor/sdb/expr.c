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

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdint.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_HEX,
  TK_REG,
  TK_INT,
  TK_DEREF,
  TK_LP,
  TK_RP,
  TK_NEG,
  TK_ADD,
  TK_SUB,
  TK_MUL,
  TK_DIV,
  TK_NOTEQ,
  TK_AND,
  TK_OR,
  TK_LGEQ,
  TK_SMEQ,
  TK_LG,
  TK_SM,
  TK_NOT,
  /*TODO: DEREF*/
  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */
    {" +", TK_NOTYPE}, // spaces
    {"\\(", TK_LP},
    {"\\)", TK_RP},
    {"==", TK_EQ}, // equal
    {"!=", TK_NOTEQ},
    {"!", TK_NOT}, // ! must after !=
    {"&&", TK_AND},
    {"\\|\\|", TK_OR},
    {">=", TK_LGEQ},
    {"<=", TK_SMEQ},

    {"\\+", TK_ADD}, // ADD
    {"-", TK_SUB},   // sub OR NEGATIVE
    {"\\*", TK_MUL}, // mul OR DEREFERENCE
    {"/", TK_DIV},   // div
    // Operation Symbol
    {">", TK_LG},
    {"<", TK_SM},

    {"[$rsgta][0-9ap]([01])?", TK_REG}, // REG
    {"0[Xx][0-9a-fA-F]{1,29}", TK_HEX}, // HEX
    {"[0-9]{1,31}", TK_INT},            // INT Ensure that str does not overflow
    /*FIXME: TK_DEREF and the usage of TK_REG*/

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    // regcomp returns 0 if the compilation succeed
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[1024] __attribute__((used)) = {}; // 1024 is more safer
static int nr_token __attribute__((used)) = 0;

static bool is_operand(int type);
static bool is_operator(int type);

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;
  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
            rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */
        tokens[nr_token].type = rules[i].token_type;
        if (tokens[nr_token].type == TK_NOTYPE)
          break; // pass spaces
        switch (rules[i].token_type) {
        case TK_HEX: // operands
        case TK_INT:
        case TK_REG:
          snprintf(tokens[nr_token].str, 32, "%.*s", substr_len, substr_start);
          break;
        default: // operators
          break;
        }
        ++nr_token;
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  // Check if its a negative operator or dereference operator
  for (int j = 0; j < nr_token; ++j) {
    if ((j == 0) ||
        (is_operator(tokens[j - 1].type) && tokens[j - 1].type != TK_RP)) {
      if (tokens[j].type == TK_SUB) {
        tokens[j].type = TK_NEG;
      } else if (tokens[j].type == TK_MUL) {
        tokens[j].type = TK_DEREF;
      }
    }
  }
  return true;
}

static bool check_parentheses(int p, int q);
static bool not_bnf_check_parentheses(int p, int q);
static void pass_all_parentheses(int *iterator);
static uint32_t calculate(uint32_t val1, uint32_t val2, int op);
static int get_precedence(int type);
static uint32_t eval(int p, int q) {
  if (p > q) {
    /* Bad expression */
    panic("Bad expression: The start evaluation position is larger than the "
          "last evaluation position.\n");
  } else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    switch (tokens[p].type) {
    case TK_INT:
      return (uint32_t)strtoul(tokens[p].str, NULL, 10);
    case TK_HEX:
      return (uint32_t)strtoul(tokens[p].str, NULL, 16);
    case TK_REG:
      TODO();
      /*TODO: FIX THIS*/
      return 0;
    default:
      panic("Unknown Oprand");
    }

  } else if (check_parentheses(p, q) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  } else {
    if (!not_bnf_check_parentheses(p, q)) {
      panic();
    }
    int op = -1, op_precedence = 100; // Given an invalid value
    for (int i = p; i <= q; ++i) {    // Search all tokens one by one
      if (tokens[i].type == TK_LP) {  // pass all tokens wrapped in parentheses
        pass_all_parentheses(&i);
        continue;
      }
      if (is_operand(tokens[i].type)) {
        continue; // pass the token which is not operator
      } else if (tokens[i].type == TK_NEG || tokens[i].type == TK_DEREF ||
                 tokens[i].type == TK_NOT) {
        if (i == p) { // single operator becomes main operator if and only if it
                      // is the first operator, else it becomes a part of the
                      // second expression
          op = i;
          break;
        }
      } else {
        int i_precedence = get_precedence(tokens[i].type);
        if (i_precedence <= op_precedence) { // Prefer lower preference and to
                                             // get rightmost operator
          op_precedence = i_precedence;
          op = i;
        }
      }
    }
    // Now op is the target operator
    // since the value of op been calculated,
    /* We should do more things here. */
    uint32_t val1;
    if (tokens[op].type == TK_NEG || tokens[op].type == TK_DEREF ||
        tokens[op].type == TK_NOT) {
      val1 = 0;
    } else {
      val1 = eval(p, op - 1);
    }
    uint32_t val2 = eval(op + 1, q);
    return calculate(val1, val2, op);
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  /* TODO: Insert codes to evaluate the expression. */
  // TODO();
  *success = true;
  return eval(0, nr_token - 1);
}

// used functions
static bool is_operator(int type) { return !is_operand(type); }
static bool is_operand(int type) {
  return (type == TK_HEX || type == TK_INT || type == TK_REG);
}

static void pass_all_parentheses(int *iterator) {
  int cnt = 1;
  while (cnt) {
    ++(*iterator);
    if (tokens[*iterator].type == TK_LP) {
      ++cnt;
    } else if (tokens[*iterator].type == TK_RP) {
      --cnt;
    }
  }
}

static uint32_t calculate(uint32_t val1, uint32_t val2, int op) {
  switch (tokens[op].type) {
  case TK_NEG:
    return -val2;
  case TK_NOT:
    return !val2;
  case TK_DEREF:
    TODO();
    // TODO: fix this DEREF
    return 0;
  case TK_OR:
    return val1 || val2;
  case TK_LG:
    return val1 > val2;
  case TK_SM:
    return val1 < val2;
  case TK_LGEQ:
    return val1 >= val2;
  case TK_SMEQ:
    return val1 <= val2;
  case TK_AND:
    return val1 && val2;
  case TK_EQ:
    return val1 == val2;
  case TK_NOTEQ:
    return val1 != val2;
  case TK_ADD:
    return val1 + val2;
  case TK_SUB:
    return val1 - val2;
  case TK_MUL:
    return val1 * val2;
  case TK_DIV:
    if (val2 == 0)
      panic("Zero Division\n");
    return val1 / val2;
  default:
    panic("Unknown operator");
  }
}
/*Parenthese parsing*/
/*We use a counter to simulate a stack*/
static bool not_bnf_check_parentheses(int p, int q) {
  int cnt = 0;
  for (int i = p; i <= q; ++i) {
    if (cnt < 0) {
      return false;
    } else if (tokens[i].type == TK_LP) {
      ++cnt;
    } else if (tokens[i].type == TK_RP) {
      --cnt;
    }
  }
  return cnt == 0;
}

static bool check_parentheses(int p, int q) {
  if (tokens[p].type != TK_LP || tokens[q].type != TK_RP) {
    return false;
  }
  int cnt = 0;
  for (int i = p; i <= q; ++i) {
    if (tokens[i].type == TK_LP)
      ++cnt;
    else if (tokens[i].type == TK_RP)
      --cnt;

    if (cnt == 0 && i < q) {
      return false;
    }
  }
  return cnt == 0;
}
static int get_precedence(int type) {
  switch (type) {
  case TK_OR:
    return 0;
  case TK_AND:
    return 1;
  case TK_EQ:
  case TK_NOTEQ:
    return 2;
  case TK_SM:
  case TK_SMEQ:
  case TK_LG:
  case TK_LGEQ:
    return 3;
  case TK_ADD:
  case TK_SUB:
    return 4;
  case TK_MUL:
  case TK_DIV:
    return 5;
  case TK_DEREF:
  case TK_NOT:
  case TK_NEG:
    return 6;
  default:
    panic("Unkown Operator Preference");
  }
}