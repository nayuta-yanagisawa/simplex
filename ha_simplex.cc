#include "ha_simplex.h"

namespace simplex
{
ha_simplex::ha_simplex(handlerton *const hton, TABLE_SHARE *const share)
    : handler(hton, share)
{
}

int ha_simplex::open(const char *name, int mode, uint test_if_locked)
{
  if (!(lock_handler= get_lock_handler(name)))
  {
    return HA_ERR_OUT_OF_MEM;
  }
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

  The following is a verbatim copy of ha_blackhole::store_lock().
*/
THR_LOCK_DATA **ha_simplex::store_lock(THD *thd, THR_LOCK_DATA **to,
                                       enum thr_lock_type lock_type)
{
  if (lock_type != TL_IGNORE && lock.type == TL_UNLOCK)
  {
    if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) &&
        !thd_in_lock_tables(thd) && !thd_tablespace_op(thd))
    {
      lock_type= TL_WRITE_ALLOW_WRITE;
    }
    if (lock_type == TL_READ_NO_INSERT && !thd_in_lock_tables(thd))
    {
      lock_type= TL_READ;
    }
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

PSI_mutex_key psi_mutex_key_simplex;

static PSI_mutex_info simplex_mutexes[]= {
    {&psi_mutex_key_simplex, "simplex", PSI_FLAG_GLOBAL}};

void init_psi_keys()
{
  if (PSI_server == NULL)
  {
    return;
  }
  int count= array_elements(simplex_mutexes);
  PSI_server->register_mutex("simplex", simplex_mutexes, count);
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

  init_lock_handler_map();

  return 0;
}

/*
  Plugin termination function, only used by the server code
*/
static int done_func(void *const p)
{
  done_lock_handler_map();
  return 0;
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
