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

#include <stdlib.h>
#include "common.h"
#include "sdb.h"

#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>

extern CPU_state cpu;
#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint* next;
  char expr[1024];
  word_t val;
  /* TODO: Add more members if necessary */

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
// Refered in init_monitor, just don't care about this
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}
static int wp_no = 0;
/* TODO: Implement the functionality of watchpoint */
void init_wp(WP* wp) {
  wp->expr[0] = '\0';
  wp->val = 0;
}
static WP* new_wp() {
  if (free_ == NULL) {
    printf("There is no more free watchpoint available.\n");
    return NULL;
  } else {
    WP* used = free_;
    free_ = free_->next;
    used->next = head;
    head = used;
    used->NO = ++wp_no;
    init_wp(used);
    return used;
  }
}

static void free_wp(WP* wp) {
  if (wp == NULL) {
    panic("Try to free a NULL watchpoint\n");
  }
  if (head == NULL) {
    printf("Try to free a watchpoint but the list is empty\n");
    return;
  } else if (wp == head) {
    head = head->next;
  } else {
    WP* iter = head;
    while (iter->next != NULL && iter->next != wp) {
      iter = iter->next;
    }
    if (iter->next == NULL) {
      printf("WatchPoint %d Not Found in head linkedlist\n", wp->NO);
      return;
    } else {
      iter->next = wp->next;
    }
  }
  wp->next = free_;
  free_ = wp;
}

bool scan_watchpoint() {
  WP* iter = head;
  while (iter != NULL) {
    bool success = true;
    word_t res = expr(iter->expr, &success);
    if (!success) {
      panic("Bad expression on watchpoint %d", iter->NO);
    }
    if (res != iter->val) {
      printf("Hit watchpoint %d at PC = " FMT_WORD "\n", iter->NO, cpu.pc);
      printf("Expr: %s\n", iter->expr);
      printf("Old value: " FMT_WORD "\n", iter->val);
      printf("New value: " FMT_WORD "\n", res);
      iter->val = res;
      return true;
    }
    iter = iter->next;
  }
  return false;
}

static bool is_enable = true;
void wp_display() {
  if (head == NULL) {
    printf("No watchpoints.\n");
    return;
  }

  printf("%-8s %-14s %-8s %-8s %s\n", "Num", "Type", "Disp", "Enb", "What");

  WP* wp = head;
  while (wp != NULL) {
    printf("%-8d %-14s %-8s %-8s %s\n", wp->NO, "watchpoint", "keep",
           (is_enable ? "y" : "n"), wp->expr);

    wp = wp->next;
  }
}

void create_watchpoint(char* e) {
  WP* wp = new_wp();
  if (wp == NULL) {
    return;
  } else {
    strncpy(wp->expr, e, 1023);
    bool success = true;
    word_t res = expr(e, &success);
    if (success) {
      wp->val = res;
      printf("WatchPoint No.%d Set\n", wp->NO);
      wp_display();
    } else {
      printf("Invalid Expression\n");
      free_wp(wp);
    }
  }
}
//
void delete_watchpoint(word_t N) {
  if (head == NULL) {
    printf("There is no watchpoint\n");
    return;
  }
  WP* iter = head;
  while (iter != NULL) {
    if (iter->NO == N) {
      free_wp(iter);
      printf("Watchpoint No.%d deleted\n", N);
      return;
    }
    iter = iter->next;
  }
  printf("watchpoint No.%d not found\n", N);
}
void delete_all_watchpoint() {
  if (head == NULL) {
    printf("There is no watchpoint\n");
    return;
  }
  WP* prev = head;
  WP* iter = head->next;
  while (iter != NULL) {
    free_wp(prev);
    prev = iter;
    iter = iter->next;
  }
  free_wp(prev);
}