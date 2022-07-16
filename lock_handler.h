#pragma once

#include <my_global.h>
#include <thr_lock.h>

namespace simplex
{
/*
  Shared structure for server-level table locking

  The code in lock_handler.h and lock_handler.cc is almost a verbatim copy of
  the code regarding st_blackhole_share. Thus, it is open to refactoring.
*/
struct LockHandler
{
  THR_LOCK thr_lock;
  uint reference_count;
  uint table_name_length;
  char table_name[1];
};

void init_lock_handler_map();
void done_lock_handler_map();

LockHandler *get_lock_handler(const char *table_name);
void free_lock_handler(LockHandler *lock_handler);
} // namespace simplex
