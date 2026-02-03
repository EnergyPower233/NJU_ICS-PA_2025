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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format = "#include <stdio.h>\n"
                           "int main() { "
                           "  unsigned result = %s; "
                           "  printf(\"%%u\", result); "
                           "  return 0; "
                           "}";

static int pos = 0;
static int depth = 0;
#define MAX_DEPTH 24

uint32_t choose(uint32_t n) { return rand() % n; }
void gen_num() {
  int num = rand() % 100 + 1; // avoid generate 0
  pos += sprintf(buf + pos, "%du",
                 num); // sprintf returns the number of char it writes
} // GCC use unsigned

void gen_rand_op() {
  char ops[] = {'+', '-', '*', '/'};
  buf[pos++] = ops[rand() % 4];
}
void gen_space() {    // random generate spaces
  int n = rand() % 4; // 0-3 spaces
  for (int i = 0; i < n; i++) {
    buf[pos++] = ' ';
  }
}
void gen(char x) { buf[pos++] = x; }
void gen_rand_expr() {
  if (pos > 60001 || depth > MAX_DEPTH) { // avoid stack overflow
    gen_num();                            // force generate a number
    return;                               // limit recursion depth
  }
  ++depth;
  gen_space(); // Random spaces
  switch (choose(3)) {
  case 0:
    gen_num();
    break;
  case 1:
    gen('(');
    gen_rand_expr();
    gen(')');
    break;
  default:
    gen_rand_expr();
    gen_rand_op();
    gen_rand_expr();
    break;
  }
  gen_space();
  --depth;
} //?
void remove_u(char *s) { // Genius
  char *dst = s;         // write pointer
  while (*s) {           // read pointer
    if (*s != 'u')
      *dst++ = *s; // go through if not suffix'u'
    s++;
  }
  *dst = '\0';
}
int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++) {
    pos = 0;
    depth = 0;

    gen_rand_expr();

    buf[pos] = '\0';

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr -Werror 2>/dev/null");
    // -Werror Warning = Error (0 division causes warning)
    // 2>/dev/null Linux Black Hole
    if (ret != 0)
      continue;
    // popen => pipeopen : open a program and read its stdout
    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result; //?WTF //FIXME:
    ret = fscanf(fp, "%d", &result);
    pclose(fp);
    remove_u(buf); // Remove 'u' before output
    printf("%u %s\n", result, buf);
  }
  return 0;
}
