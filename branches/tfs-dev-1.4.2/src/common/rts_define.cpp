/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: rts_define.h 311 2011-08-23 10:38:41Z duanfei@taobao.com $
 *
 * Authors:
 *   duanfei <duanfei@taobao.com>
 *      - initial release
 *
 */

#include "internal.h"
#include "rts_define.h"
#include "serialization.h"

namespace tfs
{
  namespace common
  {
    bool MetaServerLease::has_valid_lease(const int64_t now) 
    {
      return ((lease_id_ != INVALID_LEASE_ID) && lease_expired_time_ > now);
    }

    bool MetaServerLease::renew(const int32_t step, const int64_t now)
    {
      bool bret = has_valid_lease(now);
      if (bret)
      {
        lease_expired_time_ = now + step;
      }
      return bret;
    }

    int MetaServerLease::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, reinterpret_cast<int64_t*>(&lease_id_));
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &lease_expired_time_);
      }
      return iret;
    }

    int MetaServerLease::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, lease_id_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, lease_expired_time_);
      }
      return iret;
    }

    int64_t MetaServerLease::length() const
    {
      return INT64_SIZE + INT64_SIZE;
    }

    int MetaServerTables::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &version_);
      }
      return iret;
    }

    int MetaServerTables::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, version_);
      }
      return iret;
    }

    int64_t MetaServerTables::length() const
    {
      return INT64_SIZE;
    }

    int MetaServerThroughput::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &read_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &write_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &cache_dir_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &cache_dir_hit_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &cache_file_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &cache_file_hit_count_);
      }
      return iret;
    }

    int MetaServerThroughput::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, read_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, write_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, cache_dir_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, cache_dir_hit_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, cache_file_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, cache_file_hit_count_);
      }
      return iret;
    }

    int64_t MetaServerThroughput::length() const
    {
      return INT64_SIZE * 6;
    }

    int NetWorkStatInformation::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &recv_packet_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &recv_bytes_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &send_packet_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &send_bytes_count_);
      }
      return iret;
    }

    int NetWorkStatInformation::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, recv_packet_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, recv_bytes_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, send_packet_count_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, send_bytes_count_);
      }
      return iret;
    }

    int64_t NetWorkStatInformation::length() const
    {
      return INT64_SIZE * 4;
    }


    int MetaServerCapacity::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &mem_capacity_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &use_mem_capacity_);
      }
      return iret;
    }

    int MetaServerCapacity::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, mem_capacity_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, use_mem_capacity_);
      }
      return iret;
    }

    int64_t MetaServerCapacity::length() const
    {
      return INT64_SIZE * 2;
    }

    int MetaServerBaseInformation::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, reinterpret_cast<int64_t*>(&id_));
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &start_time_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int64(data, data_len, pos, &last_update_time_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int32(data, data_len, pos, &current_load_);
      }

      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::get_int8(data, data_len, pos, &status_);
      }
      if (TFS_SUCCESS == iret)
      {
        pos += INT8_SIZE * 3;
      }
      return iret;
    }

    int MetaServerBaseInformation::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, id_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, start_time_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int64(data, data_len, pos, last_update_time_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int32(data, data_len, pos, current_load_);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = Serialization::set_int8(data, data_len, pos, status_);
      }
      if (TFS_SUCCESS == iret)
      {
        pos += INT8_SIZE * 3;
      }
      return iret;

    }
    int64_t MetaServerBaseInformation::length() const
    {
      return INT64_SIZE * 3 + INT_SIZE * 2;
    }

    void MetaServerBaseInformation::update(const MetaServerBaseInformation& info, const int64_t now)
    {
      last_update_time_ = now;
      current_load_ = info.current_load_;
      status_ = info.status_;
    }

    int MetaServer::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int32_t iret = lease_.deserialize(data, data_len, pos);
      if (TFS_SUCCESS == iret)
      {
        iret = tables_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = throughput_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = net_work_stat_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = capacity_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = base_info_.deserialize(data, data_len, pos);
      }
      return iret;
    }

    int MetaServer::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int32_t iret = lease_.serialize(data, data_len, pos);
      if (TFS_SUCCESS == iret)
      {
        iret = tables_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = throughput_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = net_work_stat_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = capacity_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == iret)
      {
        iret = base_info_.serialize(data, data_len, pos);
      }
      return iret;
    }

    int64_t MetaServer::length() const
    {
      return lease_.length() + tables_.length() + throughput_.length()
        + net_work_stat_.length() + capacity_.length() + base_info_.length();
    }

    RsRuntimeGlobalInformation RsRuntimeGlobalInformation::instance_; 

    RsRuntimeGlobalInformation& RsRuntimeGlobalInformation::instance()
    {
      return instance_;
    }
  } /** nameserver **/
}/** tfs **/
