#include "ha_simplex.h"

static struct st_mysql_storage_engine simplex_storage_engine= {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

namespace simplex
{
ha_simplex::ha_simplex(handlerton *const hton, TABLE_SHARE *const share)
    : handler(hton, share)
{
}

static handler *create_handler(handlerton *const hton,
                               TABLE_SHARE *const share,
                               MEM_ROOT *const mem_root)
{
  return new (mem_root) ha_simplex(hton, share);
}

static int init_func(void *const p)
{
  handlerton *simplex_hton= (handlerton *) p;

  simplex_hton->create= create_handler;
  simplex_hton->drop_table= [](handlerton *, const char *) { return -1; };

  return 0;
}

static int done_func(void *const p) { return 0; }
} // namespace simplex

maria_declare_plugin(simplex){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &simplex_storage_engine,
    "simplex",
    "Nayuta Yanagisawa",
    "federated storage engine",
    PLUGIN_LICENSE_GPL,
    simplex::init_func,
    simplex::done_func,
    0x0001,
    NULL,
    NULL,
    "1.0",
    MariaDB_PLUGIN_MATURITY_EXPERIMENTAL,
} maria_declare_plugin_end;
