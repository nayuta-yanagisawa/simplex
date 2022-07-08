#include "ha_simplex.h"

namespace simplex
{
ha_simplex::ha_simplex(handlerton *const hton, TABLE_SHARE *const share)
    : handler(hton, share)
{
}

static LockHandler *get_lock_handler(const char *table_name);
static void free_lock_handler(LockHandler *lock_handler);

int ha_simplex::open(const char *name, int mode, uint test_if_locked)
{
  if (!(lock_handler= get_lock_handler(name)))
    return HA_ERR_OUT_OF_MEM;

  thr_lock_data_init(&lock_handler->thr_lock, &lock, nullptr);

  return 0;
}

int ha_simplex::close()
{
  free_lock_handler(lock_handler);
  return 0;
}

/*
  Even though SIMPLEX do nothing by external_lock(), we still need to handle
  table locks. That is because the server code assumes that a storage engine
  create some lock at the call of store_lock().
*/
THR_LOCK_DATA **ha_simplex::store_lock(THD *thd, THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    /*
      Here is where we get into the guts of a row level lock.
      If TL_UNLOCK is set
      If we are not doing a LOCK TABLE or DISCARD/IMPORT
      TABLESPACE, then allow multiple writers
    */

    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
        !thd_in_lock_tables(thd) && !thd_tablespace_op(thd))
      lock_type= TL_WRITE_ALLOW_WRITE;

    /*
      In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
      MySQL would use the lock TL_READ_NO_INSERT on t2, and that
      would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
      to t2. Convert the lock to a normal read lock to allow
      concurrent inserts to t2.
    */

    if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd))
      lock_type= TL_READ;

    lock.type= lock_type;
  }
  *to++= &lock;
  return to;
}

int ha_simplex::external_lock(THD *thd, int lock_type) { return 0; }

static struct st_mysql_storage_engine storage_engine= {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

/*
  Allocate ha_simplex on given MEM_ROOT, only used by init_func()
*/
static handler *create_handler(handlerton *const hton,
                               TABLE_SHARE *const share,
                               MEM_ROOT *const mem_root)
{
  return new (mem_root) ha_simplex(hton, share);
}

/*
  Engine-defined table option

  CREATE TABLE (...) ENGINE=SIMPLEX REMOTE_SERVER='s';
*/
struct ha_table_option_struct
{
  char *remote_server;
};

ha_create_table_option table_option_list[]= {
    HA_TOPTION_STRING("REMOTE_SERVER", remote_server), HA_TOPTION_END};

static PSI_mutex_key psi_mutex_key_simplex;

static PSI_mutex_info all_simplex_mutexes[]= {
    {&psi_mutex_key_simplex, "simplex", PSI_FLAG_GLOBAL}};

void init_psi_keys()
{
  const char *category= "simplex";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_simplex_mutexes);
  PSI_server->register_mutex(category, all_simplex_mutexes, count);
}

/*
  NOTE: The current implementation of table lock handling is almost a verbatim
  copy of that of ha_blackhole. Thus, it is open to refactoring.
*/
static HASH lock_handler_map;
static mysql_mutex_t lock_handler_map_mutex;

static uchar *get_lock_handler_key(LockHandler *share, size_t *length,
                                   my_bool not_used __attribute__((unused)))
{
  *length= share->table_name_length;
  return (uchar *) share->table_name;
}

static void free_lock_handler_key(LockHandler *share)
{
  thr_lock_delete(&share->thr_lock);
  my_free(share);
}

/*
  Plugin initialization function, only used by the server code
*/
static int init_func(void *const p)
{
  handlerton *simplex_hton= (handlerton *) p;

  init_psi_keys();

  simplex_hton->create= create_handler;
  simplex_hton->drop_table= [](handlerton *, const char *) { return -1; };

  simplex_hton->table_options= table_option_list;

  mysql_mutex_init(psi_mutex_key_simplex, &lock_handler_map_mutex,
                   MY_MUTEX_INIT_FAST);
  (void) my_hash_init(PSI_INSTRUMENT_ME, &lock_handler_map,
                      system_charset_info, 32, 0, 0,
                      (my_hash_get_key) get_lock_handler_key,
                      (my_hash_free_key) free_lock_handler_key, 0);

  return 0;
}

/*
  Plugin termination function, only used by the server code
*/
static int done_func(void *const p)
{
  my_hash_free(&lock_handler_map);
  mysql_mutex_destroy(&lock_handler_map_mutex);
  return 0;
}

static LockHandler *get_lock_handler(const char *table_name)
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
      goto error;

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

static void free_lock_handler(LockHandler *lock_handler)
{
  mysql_mutex_lock(&lock_handler_map_mutex);
  if (!--lock_handler->reference_count)
  {
    my_hash_delete(&lock_handler_map, (uchar *) lock_handler);
  }
  mysql_mutex_unlock(&lock_handler_map_mutex);
}

maria_declare_plugin(simplex){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &storage_engine,
    "SIMPLEX",
    "Nayuta Yanagisawa",
    "federated storage engine",
    PLUGIN_LICENSE_GPL,
    init_func,
    done_func,
    0x0001,
    NULL,
    NULL,
    "1.0",
    MariaDB_PLUGIN_MATURITY_EXPERIMENTAL,
} maria_declare_plugin_end;
} // namespace simplex
