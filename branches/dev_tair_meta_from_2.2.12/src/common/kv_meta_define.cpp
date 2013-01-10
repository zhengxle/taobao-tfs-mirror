/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id$
 *
 * Authors:
 *   daoan <daoan@taobao.com>
 *      - initial release
 *
 */
#include "kv_meta_define.h"
#include "serialization.h"

namespace tfs
{
  namespace common
  {
    TfsFileInfo::TfsFileInfo()
      :block_id_(0), file_id_(0), cluster_id_(0)
    { }

    int64_t TfsFileInfo::length() const
    {
      return INT64_SIZE * 2 + INT_SIZE;
    }

    void TfsFileInfo::dump() const
    {
      TBSYS_LOG(DEBUG, "TfsFileInfo: [block_id: %"PRI64_PREFIX"d, file_id: %"PRI64_PREFIX"d, "
                "cluster_id: %d]", block_id_, file_id_, cluster_id_);
    }

    int TfsFileInfo::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, block_id_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, file_id_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int32(data, data_len, pos, cluster_id_);
      }
      return ret;
    }

    int TfsFileInfo::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &block_id_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &file_id_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int32(data, data_len, pos, &cluster_id_);
      }
      return ret;
    }

    //object meta info
    ObjectMetaInfo::ObjectMetaInfo()
    :create_time_(0), modify_time_(0), file_size_(0), max_tfs_file_size_(2048)
    {}

    int64_t ObjectMetaInfo::length() const
    {
      return INT_SIZE  + INT64_SIZE * 3 ;
    }

    void ObjectMetaInfo::dump() const
    {
      TBSYS_LOG(DEBUG, "ObjectMetaInfo: [create_time: %"PRI64_PREFIX"d, modify_time: %"PRI64_PREFIX"d, "
                "file_size: %"PRI64_PREFIX"d, max_tfs_file_size: %d]",
                create_time_, modify_time_, file_size_, max_tfs_file_size_);
    }

    int ObjectMetaInfo::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;

      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, create_time_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, modify_time_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, file_size_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int32(data, data_len, pos, max_tfs_file_size_);
      }

      return ret;
    }

    int ObjectMetaInfo::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;

      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &create_time_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &modify_time_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &file_size_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int32(data, data_len, pos, &max_tfs_file_size_);
      }
      return ret;
    }

    //customizeinfo
    CustomizeInfo::CustomizeInfo()
    { }

    int64_t CustomizeInfo::length() const
    {
      return Serialization::get_string_length(otag_);
    }

    int CustomizeInfo::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_string(data, data_len, pos, otag_);
      }
      return ret;
    }

    int CustomizeInfo::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_string(data, data_len, pos, otag_);
      }
      return ret;
    }

    //object meta info
    ObjectInfo::ObjectInfo()
    :offset_(0), has_meta_info_(false), has_customize_info_(false)
    {}

    int64_t ObjectInfo::length() const
    {
      return (INT8_SIZE * 2 + INT64_SIZE + tfs_file_info_.length() +
          (has_meta_info_ ? meta_info_.length() : 0) +
          (has_customize_info_ ? customize_info_.length() : 0));
    }

    void ObjectInfo::dump() const
    {
      TBSYS_LOG(DEBUG, "ObjectInfo: [offset: %"PRI64_PREFIX"d, has_meta_info: %d, "
                "has_customize_info: %d]", offset_, has_meta_info_, has_customize_info_);
      tfs_file_info_.dump();
      meta_info_.dump();
    }

    int ObjectInfo::serialize(char* data, const int64_t data_len, int64_t& pos) const
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;

      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, offset_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = tfs_file_info_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int8(data, data_len, pos, has_meta_info_);
      }
      if (TFS_SUCCESS == ret && has_meta_info_)
      {
        ret = meta_info_.serialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int8(data, data_len, pos, has_customize_info_);
      }
      if (TFS_SUCCESS == ret && has_customize_info_)
      {
        ret = customize_info_.serialize(data, data_len, pos);
      }

      return ret;
    }

    int ObjectInfo::deserialize(const char* data, const int64_t data_len, int64_t& pos)
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;

      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &offset_);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = tfs_file_info_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int8(data, data_len, pos, reinterpret_cast<int8_t*>(&has_meta_info_));
      }
      if (TFS_SUCCESS == ret && has_meta_info_)
      {
        ret = meta_info_.deserialize(data, data_len, pos);
      }
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int8(data, data_len, pos, reinterpret_cast<int8_t*>(&has_customize_info_));
      }
      if (TFS_SUCCESS == ret && has_customize_info_)
      {
        ret = customize_info_.deserialize(data, data_len, pos);
      }

      return ret;
    }

    //bucketmetainfo
    BucketMetaInfo::BucketMetaInfo()
    :create_time_(0)
    {}

    int64_t BucketMetaInfo::length() const
    {
      return INT64_SIZE;
    }

    int BucketMetaInfo::serialize(char *data, const int64_t data_len, int64_t &pos) const
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::set_int64(data, data_len, pos, create_time_);
      }
      return ret;
    }

    int BucketMetaInfo::deserialize(const char *data, const int64_t data_len, int64_t &pos)
    {
      int ret = NULL != data && data_len - pos >= length() ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = Serialization::get_int64(data, data_len, pos, &create_time_);
      }
      return ret;
    }

  }
}

