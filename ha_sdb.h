/* Copyright (c) 2018, SequoiaDB and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <handler.h>
#include <client.hpp>
#include <vector>
#include "sdb_def.h"
#include "sdb_cl.h"

struct Sdb_share {
  char *table_name;
  uint table_name_length, use_count;
  mysql_mutex_t mutex;
  THR_LOCK lock;
};

class ha_sdb : public handler {
 public:
  ha_sdb(handlerton *hton, TABLE_SHARE *table_arg);

  ~ha_sdb();

  /** @brief
     The name that will be used for display purposes.
     */
  const char *table_type() const { return "SEQUOIADB"; }

  /** @brief
     The name of the index type that will be used for display.
     Don't implement this method unless you really have indexes.
     */
  const char *index_type(uint key_number) { return ("BTREE"); }

  /** @brief
     The file extensions.
     */
  const char **bas_ext() const;

  /** @brief
     This is a list of flags that indicate what functionality the storage engine
     implements. The current table flags are documented in handler.h
     */
  ulonglong table_flags() const;

  /** @brief
     This is a bitmap of flags that indicates how the storage engine
     implements indexes. The current index flags are documented in
     handler.h. If you do not implement indexes, just return zero here.

     @details
     part is the key part to check. First key part is 0.
     If all_parts is set, MySQL wants to know the flags for the combined
     index, up to and including 'part'.
     */
  ulong index_flags(uint inx, uint part, bool all_parts) const;

  /** @brief
     unireg.cc will call max_supported_record_length(), max_supported_keys(),
     max_supported_key_parts(), uint max_supported_key_length()
     to make sure that the storage engine can handle the data it is about to
     send. Return *real* limits of your storage engine here; MySQL will do
     min(your_limits, MySQL_limits) automatically.
     */
  uint max_supported_record_length() const;

  /** @brief
     unireg.cc will call this to make sure that the storage engine can handle
     the data it is about to send. Return *real* limits of your storage engine
     here; MySQL will do min(your_limits, MySQL_limits) automatically.

     @details
     There is no need to implement ..._key_... methods if your engine doesn't
     support indexes.
     */
  uint max_supported_keys() const;

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_part_length() const;

  /** @brief
    unireg.cc will call this to make sure that the storage engine can handle
    the data it is about to send. Return *real* limits of your storage engine
    here; MySQL will do min(your_limits, MySQL_limits) automatically.

      @details
    There is no need to implement ..._key_... methods if your engine doesn't
    support indexes.
   */
  uint max_supported_key_length() const;

  /** @brief
    Called in test_quick_select to determine if indexes should be used.
  */
  virtual double scan_time();

  /** @brief
    This method will never be called if you do not implement indexes.
  */
  virtual double read_time(uint, uint, ha_rows rows);

  /*
    Everything below are methods that we implement in ha_example.cc.

    Most of these methods are not obligatory, skip them and
    MySQL will treat them as not implemented
  */
  /** @brief
    We implement this in ha_example.cc; it's a required method.
  */
  int open(const char *name, int mode, uint test_if_locked);

  /** @brief
    We implement this in ha_example.cc; it's a required method.
  */
  int close(void);

  int reset();

  /**
    @brief Prepares the storage engine for bulk inserts.

    @param[in] rows       estimated number of rows in bulk insert
                          or 0 if unknown.

    @details Initializes memory structures required for bulk insert.
  */
  void start_bulk_insert(ha_rows rows);

  /**
    @brief End bulk insert.

    @details This method will send any remaining rows to the remote server.
    Finally, it will deinitialize the bulk insert data structure.

    @return Operation status
    @retval       0       No error
    @retval       != 0    Error occured at remote server. Also sets my_errno.
  */
  int end_bulk_insert();

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int write_row(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int update_row(const uchar *old_data, uchar *new_data);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int delete_row(const uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_read_map(uchar *buf, const uchar *key_ptr, key_part_map keypart_map,
                     enum ha_rkey_function find_flag);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_next(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_prev(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_first(uchar *buf);

  /** @brief
    We implement this in ha_example.cc. It's not an obligatory method;
    skip it and and MySQL will treat it as not implemented.
  */
  int index_last(uchar *buf);

  // int index_read(uchar *buf, const uchar *key_ptr, uint key_len,
  //               enum ha_rkey_function find_flage);

  int index_init(uint idx, bool sorted);

  int index_end();

  uint lock_count(void) const { return 0; }

  /** @brief
    Unlike index_init(), rnd_init() can be called two consecutive times
    without rnd_end() in between (it only makes sense if scan=1). In this
    case, the second call should prepare for the new table scan (e.g if
    rnd_init() allocates the cursor, the second call should position the
    cursor to the start of the table; no need to deallocate and allocate
    it again. This is a required method.
  */
  int rnd_init(bool scan);
  int rnd_end();
  int rnd_next(uchar *buf);
  int rnd_pos(uchar *buf, uchar *pos);
  void position(const uchar *record);
  int info(uint);
  int extra(enum ha_extra_function operation);
  int external_lock(THD *thd, int lock_type);
  int start_statement(THD *thd, uint table_count);
  int delete_all_rows(void);
  int truncate();
  ha_rows records_in_range(uint inx, key_range *min_key, key_range *max_key);
  int delete_table(const char *from);
  int rename_table(const char *from, const char *to);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *create_info);

  THR_LOCK_DATA **store_lock(THD *thd, THR_LOCK_DATA **to,
                             enum thr_lock_type lock_type);

  void unlock_row();

  int start_stmt(THD *thd, thr_lock_type lock_type);

  bool prepare_inplace_alter_table(TABLE *altered_table,
                                   Alter_inplace_info *ha_alter_info);

  bool inplace_alter_table(TABLE *altered_table,
                           Alter_inplace_info *ha_alter_info);

  enum_alter_inplace_result check_if_supported_inplace_alter(
      TABLE *altered_table, Alter_inplace_info *ha_alter_info);

  const Item *cond_push(const Item *cond);

  Item *idx_cond_push(uint keyno, Item *idx_cond);

  const char *get_version();

 private:
  int ensure_collection(THD *thd);

  int obj_to_row(bson::BSONObj &obj, uchar *buf);

  int row_to_obj(uchar *buf, bson::BSONObj &obj, bool output_null,
                 bson::BSONObj &null_obj);

  int next_row(bson::BSONObj &obj, uchar *buf);

  int cur_row(uchar *buf);

  int flush_bulk_insert(bool ignore_dup_key);

  int create_index(Sdb_cl &cl, Alter_inplace_info *ha_alter_info);

  int drop_index(Sdb_cl &cl, Alter_inplace_info *ha_alter_info);

  int get_cl_options(TABLE *form, HA_CREATE_INFO *create_info,
                     bson::BSONObj &options, my_bool use_partition);

  int get_sharding_key(TABLE *form, bson::BSONObj &options);

  int index_read_one(bson::BSONObj condition, int order_direction, uchar *buf);

 private:
  THR_LOCK_DATA lock_data;
  Sdb_cl *collection;
  bool first_read;
  bson::BSONObj cur_rec;
  bson::BSONObj condition;
  int keynr;        // use for index_scan
  String conv_str;  // use for converting charset
  Sdb_share *share;
  char db_name[SDB_CS_NAME_MAX_SIZE + 1];
  char table_name[SDB_CL_NAME_MAX_SIZE + 1];
  time_t last_flush_time;
  int used_times;
  MEM_ROOT blobroot;
  int idx_order_direction;
  bool m_use_bulk_insert;
  std::vector<bson::BSONObj> m_bulk_insert_rows;
};
