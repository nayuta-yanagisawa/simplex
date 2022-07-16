#include "lock_handler.h"

#include <my_global.h>
#include <thr_lock.h>
#include <hash.h>
#include "mysqld.h" // system_charset_info

namespace simplex
{
static HASH lock_handler_map;
static mysql_mutex_t lock_handler_map_mutex;

static uchar *get_lock_handler_key(LockHandler *lock_handler, size_t *length,
                                   my_bool not_used __attribute__((unused)))
{
  *length= lock_handler->table_name_length;
  return (uchar *) lock_handler->table_name;
}

static void free_lock_handler_key(LockHandler *lock_handler)
{
  thr_lock_delete(&lock_handler->thr_lock);
  my_free(lock_handler);
}

extern PSI_mutex_key psi_mutex_key_simplex;

void init_lock_handler_map()
{
  mysql_mutex_init(psi_mutex_key_simplex, &lock_handler_map_mutex,
                   MY_MUTEX_INIT_FAST);
  (void) my_hash_init(PSI_INSTRUMENT_ME, &lock_handler_map,
                      system_charset_info, 32, 0, 0,
                      (my_hash_get_key) get_lock_handler_key,
                      (my_hash_free_key) free_lock_handler_key, 0);
}

void done_lock_handler_map()
{
  my_hash_free(&lock_handler_map);
  mysql_mutex_destroy(&lock_handler_map_mutex);
}

LockHandler *get_lock_handler(const char *table_name)
{
  LockHandler *lock_handler;

  uint length= (uint) strlen(table_name);

  mysql_mutex_lock(&lock_handler_map_mutex);

  if (!(lock_handler= (LockHandler *) my_hash_search(
            &lock_handler_map, (uchar *) table_name, length)))
  {
    if (!(lock_handler= (LockHandler *) my_malloc(PSI_INSTRUMENT_ME,
                                                  sizeof(LockHandler) + length,
                                                  MYF(MY_WME | MY_ZEROFILL))))
    {
      goto error;
    }

    lock_handler->table_name_length= length;
    strmov(lock_handler->table_name, table_name);

    if (my_hash_insert(&lock_handler_map, (uchar *) lock_handler))
    {
      my_free(lock_handler);
      lock_handler= NULL;
      goto error;
    }
    thr_lock_init(&lock_handler->thr_lock);
  }
  lock_handler->reference_count++;

error:
  mysql_mutex_unlock(&lock_handler_map_mutex);
  return lock_handler;
}

void free_lock_handler(LockHandler *lock_handler)
{
  mysql_mutex_lock(&lock_handler_map_mutex);
  if (!--lock_handler->reference_count)
  {
    my_hash_delete(&lock_handler_map, (uchar *) lock_handler);
  }
  mysql_mutex_unlock(&lock_handler_map_mutex);
}
} // namespace simplex
