#pragma onece

#include <my_global.h>
#include <handler.h>

typedef struct st_mysql_res MYSQL_RES;
struct TABLE;

namespace simplex
{
struct LockHandler;

class ha_simplex final : public handler
{
public:
  THR_LOCK_DATA lock;
  LockHandler *lock_handler;
  MYSQL_RES *stored_result;

  ha_simplex(handlerton *const hton, TABLE_SHARE *const share);
  ~ha_simplex() {}

  int create(const char *name, TABLE *form,
             HA_CREATE_INFO *create_info) override
  {
    return 0;
  }
  int open(const char *name, int mode, uint test_if_locked) override;
  int close(void) override;
  int rnd_init(bool scan) override;
  int rnd_end() override;
  int rnd_next(uchar *buf) override;
  int rnd_pos(uchar *buf, uchar *pos) override { return 0; }
  void position(const uchar *record) override {}
  int info(uint) override { return 0; }
  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type) override;
  int external_lock(THD *thd, int lock_type) override;
  ulonglong table_flags() const override { return HA_BINLOG_STMT_CAPABLE; }
  ulong index_flags(uint inx, uint part, bool all_parts) const override
  {
    return 0;
  }
};
} // namespace simplex
