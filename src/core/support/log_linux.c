/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <grpc/support/port_platform.h>

#ifdef GPR_LINUX

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/sync.h>
#include <grpc/support/string_util.h>
#include <grpc/support/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <signal.h>

static long gettid(void) { return syscall(__NR_gettid); }

struct log_entry {
  char* prefix;
  char* msg;
  struct log_entry* next;
};
typedef struct log_entry log_entry;

static log_entry* g_logs_head = NULL;
static log_entry* g_logs_tail = NULL;

static void write_log_to_buffer(const char* prefix, const char* message) {
  log_entry* new_entry = gpr_malloc(sizeof(log_entry));
  new_entry->prefix = gpr_strdup(prefix);
  new_entry->msg = gpr_strdup(message);
  new_entry->next = NULL;
  if (g_logs_tail == NULL) {
    GPR_ASSERT(g_logs_head == NULL);
    g_logs_head = g_logs_tail = new_entry;
  } else {
    g_logs_tail->next = new_entry;
    g_logs_tail = new_entry;
  }
}

static void dump_logs() {
  log_entry* prev;
  log_entry* current = g_logs_head;
  while (current) {
    fprintf(stderr, "%-60s %s\n", current->prefix, current->msg);
    prev = current;
    current = current->next;
    gpr_free(prev);
  }
}

static gpr_once g_sigint_handler = GPR_ONCE_INIT;

static void catch_sigint(int sig) {
  if (sig == SIGINT) {
    dump_logs();
  }
  abort();
}

static void register_handler() {
  signal(SIGINT, catch_sigint);
}

void gpr_log(const char *file, int line, gpr_log_severity severity,
             const char *format, ...) {
  gpr_once_init(&g_sigint_handler, register_handler);

  char *message = NULL;
  va_list args;
  va_start(args, format);
  if (vasprintf(&message, format, args) == -1) {
    va_end(args);
    return;
  }
  va_end(args);
  gpr_log_message(file, line, severity, message);
  free(message);
}

void gpr_default_log(gpr_log_func_args *args) {
  char *final_slash;
  char *prefix;
  const char *display_file;
  char time_buffer[64];
  time_t timer;
  gpr_timespec now = gpr_now(GPR_CLOCK_REALTIME);
  struct tm tm;

  timer = (time_t)now.tv_sec;
  final_slash = strrchr(args->file, '/');
  if (final_slash == NULL)
    display_file = args->file;
  else
    display_file = final_slash + 1;

  if (!localtime_r(&timer, &tm)) {
    strcpy(time_buffer, "error:localtime");
  } else if (0 ==
             strftime(time_buffer, sizeof(time_buffer), "%m%d %H:%M:%S", &tm)) {
    strcpy(time_buffer, "error:strftime");
  }

  gpr_asprintf(&prefix, "%s%s.%09d %7tu %s:%d]",
               gpr_log_severity_string(args->severity), time_buffer,
               (int)(now.tv_nsec), gettid(), display_file, args->line);

  // fprintf(stderr, "%-60s %s\n", prefix, args->message);
  write_log_to_buffer(prefix, args->message);
  gpr_free(prefix);
}

#endif
