/*
 * (C) 2007-2013 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id
 *
 * Authors:
 *      mingyan(mingyan.zc@taobao.com)
 *      - initial release
 *
 */

#include "tfs_kv_meta_client_impl.h"
#include "fsname.h"
#include "common/client_manager.h"
#include "message/message_factory.h"
#include "tfs_client_api.h"
#include "tfs_kv_meta_helper.h"


namespace tfs
{
  namespace client
  {
    using namespace tfs::common;
    using namespace std;

    KvMetaClientImpl::KvMetaClientImpl()
    :rs_id_(0), access_count_(0), fail_count_(0)
    {
      packet_factory_ = new message::MessageFactory();
      packet_streamer_ = new common::BasePacketStreamer(packet_factory_);
    }

    KvMetaClientImpl::~KvMetaClientImpl()
    {
      tbsys::gDelete(packet_factory_);
      tbsys::gDelete(packet_streamer_);
      tfs_meta_manager_.destroy();
    }

    int KvMetaClientImpl::initialize(const char *rs_addr)
    {
      int ret = TFS_SUCCESS;
      if (NULL == rs_addr)
      {
        TBSYS_LOG(WARN, "rs_addr is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        ret = initialize(Func::get_host_ip(rs_addr));
      }
      return ret;
    }

    int KvMetaClientImpl::initialize(const uint64_t rs_addr)
    {
      int ret = TFS_SUCCESS;
      ret = NewClientManager::get_instance().initialize(packet_factory_, packet_streamer_);
      if (TFS_SUCCESS == ret)
      {
        rs_id_ = rs_addr;
        ret = update_table_from_rootserver();
      }

      if (TFS_SUCCESS == ret)
      {
        ret = tfs_meta_manager_.initialize();
      }

      return ret;
    }

    bool KvMetaClientImpl::need_update_table(const int ret_status)
    {
      return (++access_count_ >= ClientConfig::update_kmt_interval_count_
         || fail_count_ >= ClientConfig::update_kmt_fail_count_
         || EXIT_INVALID_KV_META_SERVER == ret_status);
    }

    int KvMetaClientImpl::update_table_from_rootserver()
    {
      int ret = TFS_ERROR;
      KvMetaTable new_table;
      if ((ret = KvMetaHelper::get_table(rs_id_, new_table)) != TFS_SUCCESS)
      {
        TBSYS_LOG(ERROR, "get table from rootserver failed. ret: %d", ret);
      }
      else
      {
        if (new_table.v_meta_table_.size() == 0)
        {
          TBSYS_LOG(ERROR, "get empty kv meta table from rootserver");
        }
        else
        {
          tbsys::CWLockGuard guard(meta_table_mutex_);
          meta_table_.v_meta_table_.clear();
          meta_table_.v_meta_table_ = new_table.v_meta_table_;
          meta_table_.dump();
          access_count_ = 0;
          fail_count_ = 0;
          ret = TFS_SUCCESS;
        }
      }

      return ret;
    }

    uint64_t KvMetaClientImpl::get_meta_server_id()
    {
      uint64_t meta_server_id = 0;
      {
        tbsys::CRLockGuard guard(meta_table_mutex_);
        int32_t table_size = meta_table_.v_meta_table_.size();
        if (table_size <= 0)
        {
          TBSYS_LOG(WARN, "no kv meta server is available");
        }
        else
        {
          meta_server_id = meta_table_.v_meta_table_.at(random() % table_size);
          TBSYS_LOG(DEBUG, "select kv meta server: %s",
              tbsys::CNetUtil::addrToString(meta_server_id).c_str());
        }
      }
      return meta_server_id;
    }

    void KvMetaClientImpl::update_fail_info(const int ret)
    {
      if (EXIT_NETWORK_ERROR == ret)
      {
        fail_count_++;
      }
      if (need_update_table(ret))
      {
        update_table_from_rootserver();
      }
    }

    TfsRetType KvMetaClientImpl::put_bucket(const char *bucket_name, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name))
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        BucketMetaInfo bucket_meta_info;

        ret = do_put_bucket(bucket_name, bucket_meta_info, user_info);
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_bucket(const char *bucket_name, const char *prefix,
        const char *start_key, const char delimiter, const int32_t limit,
        vector<ObjectMetaInfo> *v_object_meta_info,
        vector<string> *v_object_name, set<string> *s_common_prefix,
        int8_t *is_truncated, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_SUCCESS;

      if (!is_valid_bucket_name(bucket_name))
      {
        ret = TFS_ERROR;
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_get_bucket(bucket_name, prefix, start_key, delimiter, limit,
            v_object_meta_info, v_object_name, s_common_prefix, is_truncated, user_info);
      }

      TBSYS_LOG(INFO, "%d, %d", static_cast<int32_t>(v_object_name->size()), static_cast<int32_t>(s_common_prefix->size()));

      return ret;
    }

    TfsRetType KvMetaClientImpl::del_bucket(const char *bucket_name, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name))
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_del_bucket(bucket_name, user_info);
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::head_bucket(const char *bucket_name, BucketMetaInfo *bucket_meta_info, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name))
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_head_bucket(bucket_name, bucket_meta_info, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "head bucket failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::put_bucket_acl(const char *bucket_name, const common::CANNED_ACL acl,
        const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) /*|| !is_valid_bucket_acl(acl)*/)
      {
        TBSYS_LOG(ERROR, "bucket name is invalid or acl is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_put_bucket_acl(bucket_name, acl, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "put bucket acl failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::put_bucket_acl(const char *bucket_name, const common::MAP_INT64_INT &bucket_acl_map,
        const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) /*|| !is_valid_bucket_acl(acl)*/)
      {
        TBSYS_LOG(ERROR, "bucket name is invalid or acl is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_put_bucket_acl(bucket_name, bucket_acl_map, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "put bucket acl failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_bucket_acl(const char *bucket_name, common::MAP_INT64_INT *bucket_acl_map,
        const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) /*|| !is_valid_bucket_acl(acl)*/)
      {
        TBSYS_LOG(ERROR, "bucket name is invalid or acl is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_get_bucket_acl(bucket_name, bucket_acl_map, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "get bucket acl failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_service(common::BucketsResult *buckets_result, const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (NULL == buckets_result)
      {
        TBSYS_LOG(ERROR, "param is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_get_service(buckets_result, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "get service failed. owner_id: %"PRI64_PREFIX"d", user_info.owner_id_);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::put_bucket_logging(const char *bucket_name, const bool logging_status,
            const char *target_bucket_name, const char *target_prefix, const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) || NULL == target_bucket_name || NULL == target_prefix)
      {
        TBSYS_LOG(ERROR, "param is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_put_bucket_logging(bucket_name, logging_status, target_bucket_name, target_prefix, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "put bucket logging failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_bucket_logging(const char *bucket_name, bool *logging_status,
            string *target_bucket_name, string *target_prefix, const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) || NULL == logging_status
          || NULL == target_bucket_name || NULL == target_prefix)
      {
        TBSYS_LOG(ERROR, "param is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_get_bucket_logging(bucket_name, logging_status, target_bucket_name, target_prefix, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "get bucket tag failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::put_bucket_tag(const char *bucket_name, const MAP_STRING &bucket_tag_map)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) || !is_valid_bucket_tag(bucket_tag_map))
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_put_bucket_tag(bucket_name, bucket_tag_map);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "put bucket tag failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_bucket_tag(const char *bucket_name, MAP_STRING *bucket_tag_map)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name) || NULL == bucket_tag_map)
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_get_bucket_tag(bucket_name, bucket_tag_map);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "get bucket tag failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::del_bucket_tag(const char *bucket_name)
    {
      TfsRetType ret = TFS_ERROR;

      if (!is_valid_bucket_name(bucket_name))
      {
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }
      else
      {
        ret = TFS_SUCCESS;
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_del_bucket_tag(bucket_name);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "del bucket tag failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::list_multipart_object(const char *bucket_name, const char *prefix,
        const char *start_key, const char *start_id, const char delimiter, const int32_t limit,
        ListMultipartObjectResult *list_multipart_object_result, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_SUCCESS;

      if (!is_valid_bucket_name(bucket_name))
      {
        ret = TFS_ERROR;
        TBSYS_LOG(ERROR, "bucket name is invalid");
      }

      if (TFS_SUCCESS == ret)
      {
        ret = do_list_multipart_object(bucket_name, prefix, start_key, start_id, delimiter, limit,
            list_multipart_object_result, user_info);
      }

      return ret;
    }

    // copy from NameMetaClientImpl::write_data
    int64_t KvMetaClientImpl::write_data(const char *ns_addr,
        const void *buffer, int64_t offset, int64_t length,
        vector<FragMeta> *v_frag_meta)
    {
      int64_t ret = TFS_SUCCESS;
      int64_t write_length = 0;
      int64_t left_length = length;
      int64_t cur_offset = offset;
      int64_t cur_pos = 0;

      do
      {
        // write to tfs, and get frag meta
        write_length = min(left_length, MAX_SEGMENT_SIZE);
        FragMeta frag_meta;
        int64_t real_length = tfs_meta_manager_.write_data(ns_addr,
            reinterpret_cast<const char*>(buffer) + cur_pos, cur_offset, write_length, frag_meta);

        if (real_length != write_length)
        {
          TBSYS_LOG(ERROR, "write fragment failed, cur_pos : %"PRI64_PREFIX"d, "
              "write_length : %"PRI64_PREFIX"d, ret_length: %"PRI64_PREFIX"d, ret: %"PRI64_PREFIX"d",
              cur_pos, write_length, real_length, ret);
          if (real_length < 0)
          {
            ret = real_length;
          }
          break;
        }
        else
        {
          v_frag_meta->push_back(frag_meta);
          cur_offset += (offset == -1 ? 0 : real_length);
          cur_pos += real_length;
          left_length -= real_length;
        }
      }
      while(left_length > 0);
      if (TFS_SUCCESS == ret)
      {
        ret = length - left_length;
      }
      return ret;
    }

    int64_t KvMetaClientImpl::read_data(const vector<TfsFileInfo> &v_tfs_file_info,
        void *buffer, int64_t offset, int64_t length, bool still_have)
    {
      int32_t ret = TFS_SUCCESS;
      int64_t cur_offset = offset;
      int64_t cur_length = 0;
      int64_t left_length = length;
      int64_t cur_pos = 0;
      string ns_addr;
      int ns_get_index = 0;
      int32_t cluster_id = -1;
      vector<TfsFileInfo>::const_iterator iter = v_tfs_file_info.begin();
      for(; iter != v_tfs_file_info.end(); iter++)
      {
        TBSYS_LOG(ERROR, "===============, cur_offset: %"PRI64_PREFIX"d, total: %"PRI64_PREFIX"d",cur_offset, (iter->offset_ + iter->file_size_));
        if (cur_offset > iter->offset_ + iter->file_size_)
        {
          TBSYS_LOG(DEBUG, "skip a seg, cur_offset: %"PRI64_PREFIX"d, total: %"PRI64_PREFIX"d",
              cur_offset, (iter->offset_ + iter->file_size_));
          continue;
        }

        // deal with the front hole or mid hole
        if (cur_offset < iter->offset_)
        {
          int32_t diff = min(offset + length - cur_offset, iter->offset_ - cur_offset);
          left_length -= diff;
          memset(reinterpret_cast<char*>(buffer) + cur_pos, 0, diff);
          if (left_length <= 0)
          {
            left_length = 0;
            break;
          }
          cur_offset += diff;
          cur_pos += diff;
        }

        if (cluster_id != iter->cluster_id_)
        {
          cluster_id = iter->cluster_id_;
          ns_get_index = 0;
          ns_addr.clear();

          ns_addr = tfs_cluster_manager_->get_read_ns_addr_ex(cluster_id, ns_get_index++);

          if (ns_addr.empty())
          {
            TBSYS_LOG(ERROR, "select read ns failed %d",cluster_id);
            ret = EXIT_GENERAL_ERROR;
            break;
          }
        }
        cur_length = min(iter->file_size_ - (cur_offset - iter->offset_), left_length);
        int64_t read_length = 0;
        do
        {
          read_length = tfs_meta_manager_.read_data(ns_addr.c_str(), iter->block_id_, iter->file_id_,
              reinterpret_cast<char*>(buffer) + cur_pos, (cur_offset - iter->offset_), cur_length);
          if (read_length < 0)
          {
            TBSYS_LOG(ERROR, "read tfs data from ns: %s failed, block_id: %"PRI64_PREFIX"d, file_id: %"PRI64_PREFIX"u",
                ns_addr.c_str(), iter->block_id_, iter->file_id_);
            ns_addr = tfs_cluster_manager_->get_read_ns_addr_ex(cluster_id, ns_get_index++);
            if (ns_addr.empty())
            {
              TBSYS_LOG(ERROR, "select read ns failed %d", cluster_id);
              break;
            }
          }
        }
        while (read_length < 0);

        if (read_length < 0)
        {
          ret = read_length;
          break;
        }

        if (read_length != cur_length)
        {
          left_length -= read_length;
          TBSYS_LOG(WARN, "read tfs data return wrong length,"
              " cur_offset: %"PRI64_PREFIX"d, cur_length: %"PRI64_PREFIX"d, "
              "read_length(%"PRI64_PREFIX"d) => cur_length(%"PRI64_PREFIX"d), left_length: %"PRI64_PREFIX"d",
              cur_offset, cur_length, read_length, cur_length, left_length);
          break;
        }
        cur_pos += read_length;
        cur_offset += read_length;
        left_length -= read_length;

        if (left_length <= 0)
        {
          break;
        }
      }
      //deal the hole
      if (left_length > 0 && !still_have)
      {
        memset(reinterpret_cast<char*>(buffer) + cur_pos, 0, left_length);
        cur_offset += left_length;
        left_length = 0;
      }

      return (TFS_SUCCESS == ret) ? (length - left_length) : ret;
    }

    int KvMetaClientImpl::unlink_file(const vector<TfsFileInfo> &v_tfs_file_info)
    {
      int ret = TFS_SUCCESS;
      int tmp_ret = TFS_ERROR;
      int64_t file_size = 0;
      string ns_addr;

      vector<TfsFileInfo>::const_iterator iter = v_tfs_file_info.begin();
      for(; iter != v_tfs_file_info.end(); iter++)
      {
        ns_addr.clear();
        ns_addr = tfs_cluster_manager_->get_unlink_ns_addr_ex(iter->cluster_id_, iter->block_id_, 0);
        if (ns_addr.empty())
        {
          TBSYS_LOG(ERROR, "select unlink ns failed");
          ret = TFS_ERROR;
          continue;
        }
        FSName fsname(iter->block_id_, iter->file_id_, 0);
        if ((tmp_ret = TfsClient::Instance()->unlink(file_size, fsname.get_name(), NULL, ns_addr.c_str())) != TFS_SUCCESS)
        {
          ret = TFS_ERROR;
          TBSYS_LOG(ERROR, "unlink tfs file failed, file: %s, ret: %d", fsname.get_name(), tmp_ret);
        }
      }
      return ret;
    }

    int64_t KvMetaClientImpl::pwrite_object(const char *bucket_name,
        const char *object_name, const void *buffer, int64_t offset,
        int64_t length, const UserInfo &user_info, const CustomizeInfo *customize_info)
    {
      int64_t ret = EXIT_GENERAL_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (buffer == NULL || length < 0)
      {
        TBSYS_LOG(ERROR, "invalid buffer, length: %"PRI64_PREFIX"d", length);
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (offset == 0 && length == 0)
      {
        ObjectInfo object_info_null;
        object_info_null.has_meta_info_ = true;
        object_info_null.has_customize_info_ = false;
        if (NULL != customize_info)
        {
          object_info_null.customize_info_ = *customize_info;
        }
        object_info_null.meta_info_.max_tfs_file_size_ = MAX_SEGMENT_SIZE;

        ret = do_put_object(bucket_name, object_name, object_info_null, user_info);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "do put object fail, bucket: %s, object: %s, ret: %"PRI64_PREFIX"d",
              bucket_name, object_name, ret);
          if (TFS_ERROR == ret)
          {
            ret = EXIT_GENERAL_ERROR;
          }
        }
      }
      else
      {
        int ns_get_index = 0;
        string ns_addr;
        ns_addr = tfs_cluster_manager_->get_write_ns_addr(ns_get_index++);
        if (ns_addr.empty())
        {
          TBSYS_LOG(ERROR, "select write ns failed");
        }
        else
        {
          int32_t cluster_id = tfs_meta_manager_.get_cluster_id(ns_addr.c_str());
          int64_t left_length = length;
          int64_t cur_offset = offset;
          int64_t cur_pos = 0;
          vector<FragMeta> v_frag_meta;
          vector<TfsFileInfo> v_tfs_file_info;
          do
          {
            // write MAX_BATCH_DATA_LENGTH(8M) to tfs cluster
            int64_t write_length = min(left_length, MAX_BATCH_DATA_LENGTH);
            v_frag_meta.clear();

            int64_t real_write_length = write_data(ns_addr.c_str(),
                reinterpret_cast<const char*>(buffer) + cur_pos,
                cur_offset, write_length, &v_frag_meta);
            if (real_write_length != write_length)
            {
              TBSYS_LOG(ERROR, "write ns %s failed error, cur_pos: %"PRI64_PREFIX"d"
                  "write_length(%"PRI64_PREFIX"d) => real_length(%"PRI64_PREFIX"d)",
                  ns_addr.c_str(), cur_pos, write_length, real_write_length);
              ns_addr = tfs_cluster_manager_->get_write_ns_addr(ns_get_index++);
              if (ns_addr.empty())
              {
                TBSYS_LOG(ERROR, "select write ns failed");
                break;
              }
              cluster_id = tfs_meta_manager_.get_cluster_id(ns_addr.c_str());
              continue;
            }

            TBSYS_LOG(DEBUG, "write tfs data, cluster_id: %d, cur_offset: %"PRI64_PREFIX"d, write_length: %"PRI64_PREFIX"d",
                cluster_id, cur_offset, write_length);

            vector<FragMeta>::iterator iter = v_frag_meta.begin();
            v_tfs_file_info.clear();
            for (; v_frag_meta.end() != iter; iter++)
            {
              ObjectInfo object_info;
              if (0 == iter->offset_)
              {
                object_info.has_meta_info_ = true;
                object_info.has_customize_info_ = false;
                if (NULL != customize_info)
                {
                  object_info.customize_info_ = *customize_info;
                }
                object_info.meta_info_.max_tfs_file_size_ = MAX_SEGMENT_SIZE;
                TBSYS_LOG(DEBUG, "first object info, will put meta info.");
                object_info.meta_info_.dump();
              }
              else
              {
                object_info.has_meta_info_ = false;
                object_info.has_customize_info_ = false;
              }
              TfsFileInfo tmp_tfs_info;
              tmp_tfs_info.offset_ = iter->offset_;
              tmp_tfs_info.block_id_ = iter->block_id_;
              tmp_tfs_info.file_id_ = iter->file_id_;
              tmp_tfs_info.cluster_id_ = cluster_id;
              tmp_tfs_info.file_size_ = iter->size_;
              object_info.v_tfs_file_info_.push_back(tmp_tfs_info);
              v_tfs_file_info.push_back(tmp_tfs_info);

              ret = do_put_object(bucket_name, object_name, object_info, user_info);
              if (TFS_SUCCESS != ret)
              {
                TBSYS_LOG(ERROR, "do put object fail, bucket: %s, object: %s, offset: %"PRI64_PREFIX"d, ret: %"PRI64_PREFIX"d",
                    bucket_name, object_name, iter->offset_, ret);
                if (TFS_ERROR == ret)
                {
                  ret = EXIT_GENERAL_ERROR;
                }
                break;
              }
            }

            if (TFS_SUCCESS != ret)
            {
              unlink_file(v_tfs_file_info);
              break;
            }

            left_length -= real_write_length;
            cur_pos += real_write_length;
            cur_offset += real_write_length;
          }
          while (left_length > 0 && TFS_SUCCESS == ret);

          if (TFS_SUCCESS == ret)
          {
            ret = length - left_length;
          }
        }
      }

      return ret;
    }

    int64_t KvMetaClientImpl::pread_object(const char *bucket_name,
        const char *object_name, void *buffer, const int64_t offset,
        int64_t length, ObjectMetaInfo *object_meta_info,
        CustomizeInfo *customize_info, const UserInfo &user_info)
    {
      int64_t ret = EXIT_GENERAL_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name of object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (buffer == NULL || offset < 0 || length < 0)
      {
        TBSYS_LOG(ERROR, "invalid buffer, offset: %"PRI64_PREFIX"d,"
            " length: %"PRI64_PREFIX"d", offset, length);
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        bool still_have = false;
        int64_t left_length = length;
        int64_t read_length = 0;
        int64_t cur_offset = offset;
        int64_t cur_length = 0;
        int64_t cur_pos = 0;

        do
        {
          // get object
          ObjectInfo object_info;
          ret = do_get_object(bucket_name, object_name, cur_offset,
              left_length, &object_info, &still_have, user_info);
          TBSYS_LOG(DEBUG, "bucket_name %s object_name %s "
              "cur_offset %ld left_length %ld still_have %d",
              bucket_name, object_name, cur_offset, left_length, still_have);
          if (TFS_SUCCESS != ret)
          {
            TBSYS_LOG(ERROR, "do get object fail, bucket: %s, object: %s, offset: %"PRI64_PREFIX"d, ret: %"PRI64_PREFIX"d",
                bucket_name, object_name, cur_offset, ret);
            if (TFS_ERROR == ret)
            {
              ret = EXIT_GENERAL_ERROR;
            }
            break;
          }
          if (0 == cur_offset)
          {
            if (!object_info.has_meta_info_)
            {
              TBSYS_LOG(WARN, "invalid object, no meta info, bucket: %s, object: %s",
                  bucket_name, object_name);
              //ret = EXIT_INVALID_OBJECT;
              //break;
            }

            if (NULL != object_meta_info)
            {
              *object_meta_info = object_info.meta_info_;
            }
            if (NULL != customize_info)
            {
              *customize_info = object_info.customize_info_;
            }
          }

          TBSYS_LOG(DEBUG, "vector size ================= is: %lu", object_info.v_tfs_file_info_.size());

          cur_length = min(static_cast<int64_t>(object_info.meta_info_.big_file_size_) - cur_offset, left_length);

          // read tfs
          read_length = read_data(object_info.v_tfs_file_info_,
              reinterpret_cast<char*>(buffer) + cur_pos, cur_offset, cur_length, still_have);
          if (read_length < 0)
          {
            TBSYS_LOG(ERROR, "read data failed, read_length: %"PRI64_PREFIX"d", read_length);
            ret = read_length;
            break;
          }

          if (0 == read_length)
          {
            break;
          }

          left_length -= read_length;
          TBSYS_LOG(DEBUG, "@@ read once, offset: %ld, length %ld, read_length: %ld, left: %ld",
              cur_offset, cur_length, read_length, left_length);
          cur_offset += read_length;
          cur_pos += read_length;
        } while(left_length > 0 && still_have);

        if (TFS_SUCCESS == ret)
        {
          ret = (length - left_length);
        }
      }
      return ret;
    }

    TfsRetType KvMetaClientImpl::put_object(const char *bucket_name,
        const char *object_name, const char* local_file,
        const UserInfo &user_info, const CustomizeInfo &customize_info)
    {
      TfsRetType ret = TFS_SUCCESS;
      int fd = -1;
      if (!is_valid_bucket_name(bucket_name)
          || !is_valid_object_name(object_name)
          || !is_valid_customize_info(customize_info))
      {
        TBSYS_LOG(ERROR, "bucket_name or object_name or customize_info is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (NULL == local_file)
      {
        TBSYS_LOG(ERROR, "local file is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if ((fd = ::open(local_file, O_RDONLY)) < 0)
      {
        TBSYS_LOG(ERROR, "open local file %s fail: %s", local_file, strerror(errno));
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        char* buf = new char[MAX_BATCH_DATA_LENGTH];
        int64_t read_len = 0, write_len = 0;
        int64_t offset = 0;

        while (TFS_SUCCESS == ret)
        {
          if ((read_len = ::read(fd, buf, MAX_BATCH_DATA_LENGTH)) < 0)
          {
            ret = EXIT_INVALID_ARGU_ERROR;
            TBSYS_LOG(ERROR, "read local file %s fail, error: %s", local_file, strerror(errno));
            break;
          }
          // jump for non_local_file
          if (read_len == 0 && offset != 0)
          {
            break;
          }

          do
          {
            if (offset == 0)
            {
              write_len = pwrite_object(bucket_name, object_name, buf, offset, read_len, user_info, &customize_info);
            }
            else
            {
              write_len = pwrite_object(bucket_name, object_name, buf, offset, read_len, user_info, NULL);
            }

            if (write_len < 0)
            {
              TBSYS_LOG(ERROR, "pwrite object fail. bucket: %s, object: %s", bucket_name, object_name);
              ret = TFS_ERROR;
              break;
            }
            offset += write_len;
            read_len -= write_len;
          }while (read_len > 0);

          //jump for empty local_file
          if (offset == 0)
          {
            break;
          }
        }

        tbsys::gDeleteA(buf);
        ::close(fd);
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_object(const char *bucket_name,
        const char *object_name, const char* local_file,
        ObjectMetaInfo *object_meta_info, CustomizeInfo *customize_info,
        const UserInfo &user_info)
    {
      TfsRetType ret = TFS_SUCCESS;
      int fd = -1;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (NULL == local_file)
      {
        TBSYS_LOG(ERROR, "local file is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if ((fd = ::open(local_file, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0)
      {
        TBSYS_LOG(ERROR, "open local file %s to write fail: %s", local_file, strerror(errno));
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        int64_t io_size = MAX_READ_SIZE_KV;

        char* buf = new char[io_size];
        int64_t read_len = 0, write_len = 0;
        int64_t offset = 0;
        int64_t length = io_size;
        int64_t cur_length = 0;
        while (1)
        {
          cur_length = min(io_size, length);
          read_len = pread_object(bucket_name, object_name, buf, offset,
              cur_length, object_meta_info, customize_info, user_info);
          if (0 == offset)
          {
            length = object_meta_info->big_file_size_;
            TBSYS_LOG(DEBUG, "big file size is %"PRI64_PREFIX"d",length);
          }
          if (read_len < 0)
          {
            ret = read_len;
            TBSYS_LOG(ERROR, "get object fail. bucket: %s, object: %s, ret: %d", bucket_name, object_name, ret);
            break;
          }

          if (0 == read_len)
          {
            break;
          }

          if ((write_len = ::write(fd, buf, read_len)) != read_len)
          {
            TBSYS_LOG(ERROR, "write local file %s fail, write len: %"PRI64_PREFIX"d, ret: %"PRI64_PREFIX"d, error: %s",
                local_file, read_len, write_len, strerror(errno));
            ret = TFS_ERROR;
            break;
          }
          offset += read_len;
          length -= read_len;
          if (0 == length)
          {
            break;
          }
          TBSYS_LOG(DEBUG, "@@ out while once, offset: %ld, read_length: %ld",
                     offset, read_len);
        }
        tbsys::gDeleteA(buf);
        ::close(fd);
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::del_multi_object(const char *bucket_name, const std::set<std::string> &s_object_name,
            const bool quiet, common::DeleteResult *delete_result, const common::UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;
      if (!is_valid_bucket_name(bucket_name) || s_object_name.size() <= 0u)
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        ret = do_del_multi_object(bucket_name, s_object_name, quiet, delete_result, user_info);

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "del multi object failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::del_object(const char *bucket_name,
        const char *object_name, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        vector<TfsFileInfo> v_tfs_file_info;
        bool still_have = false;
        do
        {
          ObjectInfo object_info;
          still_have = false;
          ret = do_del_object(bucket_name, object_name, &object_info, &still_have, user_info);
          TBSYS_LOG(DEBUG, "bucket_name %s object_name %s "
              " still_have %d", bucket_name, object_name, still_have);
          TBSYS_LOG(DEBUG, "del vector size ================= is: %lu",
              object_info.v_tfs_file_info_.size());
          if (TFS_SUCCESS == ret)
          {
            v_tfs_file_info.clear();
            size_t i = 0;
            for(; i < object_info.v_tfs_file_info_.size(); ++i)
            {
              TfsFileInfo tfs_info(object_info.v_tfs_file_info_[i].cluster_id_,
              object_info.v_tfs_file_info_[i].block_id_,
              object_info.v_tfs_file_info_[i].file_id_,
              object_info.v_tfs_file_info_[i].offset_,
              object_info.v_tfs_file_info_[i].file_size_);
              v_tfs_file_info.push_back(tfs_info);
            }
          }

          if (TFS_SUCCESS == ret && v_tfs_file_info.size() > 0)
          {
            ret = unlink_file(v_tfs_file_info);
          }

        } while(still_have);

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "del object failed. bucket: %s, object: %s", bucket_name, object_name);
        }
      }
      return ret;
    }

    TfsRetType KvMetaClientImpl::head_object(const char *bucket_name, const char *object_name, ObjectInfo *object_info, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        ret = do_head_object(bucket_name, object_name, object_info, user_info);

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "head object failed. bucket: %s, object: %s", bucket_name, object_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::apply_authorize(const char *user_name, char* access_key_id, char *access_secret_key)
    {
      TfsRetType ret = TFS_ERROR;
      if (!is_valid_bucket_name(user_name))
      {
        TBSYS_LOG(ERROR, "user name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        ret = do_apply_authorize(user_name, access_key_id, access_secret_key);

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "apply authorize failed. username: %s", user_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::get_authorize(const char* access_key_id, char *user_name, char *access_secret_key)
    {
      TfsRetType ret = TFS_ERROR;
      if (NULL == user_name || NULL == access_secret_key)
      {
        TBSYS_LOG(ERROR, "username or access secret key is null ");
        ret = EXIT_INVALID_ARGU;
      }
      else if (!is_valid_bucket_name(access_key_id))
      {
        TBSYS_LOG(ERROR, "access key id is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        common::AuthorizeValueInfo authorizeinfo;
        ret = do_get_authorize(access_key_id, &authorizeinfo);
        if (TFS_SUCCESS == ret)
        {
          int32_t user_name_len = authorizeinfo.user_name_.size();
          authorizeinfo.user_name_.copy(user_name, user_name_len, 0);
          user_name[user_name_len] = '\0';

          int32_t access_secret_key_len = authorizeinfo.access_secret_key_.size();
          authorizeinfo.access_secret_key_.copy(access_secret_key, access_secret_key_len, 0);
          access_secret_key[access_secret_key_len] = '\0';
        }

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "get authorize failed. access_key_id: %s", access_key_id);
        }
      }

      return ret;
    }
    int KvMetaClientImpl::do_put_bucket(const char *bucket_name, const BucketMetaInfo& bucket_meta_info, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_bucket(meta_server_id, bucket_name, bucket_meta_info, user_info);

        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    // TODO
    int KvMetaClientImpl::do_get_bucket(const char *bucket_name, const char *prefix,
        const char *start_key, const char delimiter, const int32_t limit,
        vector<ObjectMetaInfo> *v_object_meta_info,
        vector<string> *v_object_name, set<string> *s_common_prefix,
        int8_t *is_truncated, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_bucket(meta_server_id, bucket_name, prefix, start_key, delimiter, limit,
            v_object_meta_info, v_object_name, s_common_prefix, is_truncated, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_list_multipart_object(const char *bucket_name, const char *prefix,
        const char *start_key, const char *start_id, const char delimiter, const int32_t limit,
        ListMultipartObjectResult *list_multipart_object_result, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_list_multipart_object(meta_server_id, bucket_name, prefix, start_key, start_id, delimiter, limit, list_multipart_object_result, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }


    int KvMetaClientImpl::do_del_bucket(const char *bucket_name, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_del_bucket(meta_server_id, bucket_name, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_head_bucket(const char *bucket_name, BucketMetaInfo *bucket_meta_info, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_head_bucket(meta_server_id, bucket_name, bucket_meta_info, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_put_bucket_acl(const char *bucket_name, const CANNED_ACL acl, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;

      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_bucket_acl(meta_server_id, bucket_name, acl, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_put_bucket_acl(const char *bucket_name, const MAP_INT64_INT &bucket_acl_map,
        const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;

      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_bucket_acl(meta_server_id, bucket_name, bucket_acl_map, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_bucket_acl(const char *bucket_name, MAP_INT64_INT *bucket_acl_map,
        const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_bucket_acl(meta_server_id, bucket_name, bucket_acl_map, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_service(BucketsResult *buckets_result, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_service(meta_server_id, buckets_result, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }


    //bucket logging
    int KvMetaClientImpl::do_put_bucket_logging(const char *bucket_name, const bool logging_status,
        const char *target_bucket_name, const char *target_prefix, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_bucket_logging(meta_server_id, bucket_name, logging_status,
            target_bucket_name, target_prefix, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_bucket_logging(const char *bucket_name, bool *logging_status,
        string *target_bucket_name, string *target_prefix, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_bucket_logging(meta_server_id, bucket_name, logging_status,
            target_bucket_name, target_prefix, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    //bucket tag
    int KvMetaClientImpl::do_put_bucket_tag(const char *bucket_name, const MAP_STRING &bucket_tag_map)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_bucket_tag(meta_server_id, bucket_name, bucket_tag_map);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_bucket_tag(const char *bucket_name, MAP_STRING *bucket_tag_map)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_bucket_tag(meta_server_id, bucket_name, bucket_tag_map);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_del_bucket_tag(const char *bucket_name)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_del_bucket_tag(meta_server_id, bucket_name);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_put_object(const char *bucket_name,
        const char *object_name, const ObjectInfo &object_info, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_put_object(meta_server_id, bucket_name, object_name, object_info, user_info);

        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_object(const char *bucket_name,
        const char *object_name, const int64_t offset, const int64_t length, ObjectInfo *object_info, bool *still_have, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_object(meta_server_id, bucket_name, object_name, offset, length, object_info, still_have, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_del_multi_object(const char *bucket_name, const set<string> &s_object_name,
        const bool quiet, DeleteResult *delete_result, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_del_multi_object(meta_server_id, bucket_name, s_object_name, quiet, delete_result, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_del_object(const char *bucket_name, const char *object_name, ObjectInfo *object_info, bool *still_have, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_del_object(meta_server_id, bucket_name, object_name, object_info, still_have, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_head_object(const char *bucket_name, const char *object_name, ObjectInfo *object_info, const UserInfo &user_info)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_head_object(meta_server_id, bucket_name, object_name, object_info, user_info);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_apply_authorize(const char *user_name, char* access_key_id, char *access_secret_key)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_apply_authorize(meta_server_id, user_name, access_key_id, access_secret_key);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_get_authorize(const char* access_key_id, common::AuthorizeValueInfo* authorizeinfo)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_get_authorize(meta_server_id, access_key_id, authorizeinfo);
        update_fail_info(ret);
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }
    bool KvMetaClientImpl::is_valid_bucket_name(const char *bucket_name)
    {
      bool is_valid = true;

      if (NULL == bucket_name)
      {
        is_valid = false;
      }
      int32_t len = -1;
      // len > 3 && len < 256
      if (is_valid)
      {
        len = static_cast<int32_t>(strlen(bucket_name));

        if (len < MIN_FILE_PATH_LEN || len > MAX_FILE_PATH_LEN)
        {
          is_valid = false;
        }
      }

      // start & end loweralpha or digit, other is loweralpha or digit or '.'
      // my..aws is not permit
      if (is_valid)
      {
        //handle the string like my..aws
        bool is_period_pre = true;
        for (int32_t i = 0; i < len; i++)
        {
          if (is_period_pre)
          {
            if (!islower(bucket_name[i]) && !isdigit(bucket_name[i]))
            {
              is_valid = false;
              break;
            }
            else
            {
              is_period_pre = false;
            }
          }
          else
          {
            if (!islower(bucket_name[i]) && !isdigit(bucket_name[i])
                && PERIOD != bucket_name[i] && DASH != bucket_name[i])
            {
              is_valid = false;
              break;
            }
            else if (PERIOD == bucket_name[i])
            {
              is_period_pre = true;
            }
          }

          if (is_valid && i == len - 1 && PERIOD == bucket_name[i])
          {
            is_valid = false;
          }
        }
      }

      // check the form 192.234.34.45
      if (is_valid)
      {
        int32_t period_size = 0;
        int32_t digit_size = 0;
        for (int i = 0; i < len; i++)
        {
          if (isdigit(bucket_name[i]))
          {
            digit_size++;
          }
          else if (PERIOD == bucket_name[i])
          {
            period_size++;
          }
        }

        if (MIN_FILE_PATH_LEN == period_size && period_size + digit_size == len)
        {
          is_valid = false;
        }
      }
      return is_valid;
    }

    bool KvMetaClientImpl::is_valid_object_name(const char *object_name)
    {
      return ((object_name != NULL) && (strlen(object_name) > 0) && (static_cast<int32_t>(strlen(object_name)) <= MAX_OBJECT_NAME_SIZE) && (strstr(object_name, " ") == NULL));
    }

    bool KvMetaClientImpl::is_valid_string(const string &str)
    {
      bool is_valid = true;
      string tmp = "+-=._:/";
      for (size_t i = 0; i < str.length(); i++)
      {
        if (!isdigit(str[i]) && !isalpha(str[i]) && std::string::npos == tmp.find(str[i]))
        {
          is_valid = false;
          break;
        }
      }
      return is_valid;
    }

    bool KvMetaClientImpl::is_valid_bucket_tag(const MAP_STRING &bucket_tag_map)
    {
      bool is_valid = true;

      if (static_cast<int32_t>(bucket_tag_map.size()) > MAX_BUCKET_TAG_SIZE)
      {
        is_valid = false;
      }

      if (is_valid)
      {
        MAP_STRING_ITER iter = bucket_tag_map.begin();
        for (; iter != bucket_tag_map.end(); iter++)
        {
          const string& key = iter->first;
          const string& value = iter->second;
          if (static_cast<int32_t>(key.length()) > MAX_TAG_KEY_LEN
              || static_cast<int32_t>(value.length()) > MAX_TAG_VALUE_LEN
              || !is_valid_string(key) || !is_valid_string(value))
          {
            is_valid = false;
            break;
          }
        }
      }

      return is_valid;
    }

    TfsRetType KvMetaClientImpl::init_multipart(const char *bucket_name,
        const char *object_name, std::string* const upload_id)
    {
      TfsRetType ret = TFS_SUCCESS;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else
      {
        ret = do_init_multipart(bucket_name, object_name, upload_id);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "init multipart failed. bucket: %s, object: %s", bucket_name, object_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::upload_multipart(const char *bucket_name,
        const char *object_name, const char* local_file, const char* upload_id,
        const int32_t part_num, const UserInfo &user_info)
    {
      TfsRetType ret = TFS_SUCCESS;
      int fd = -1;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (NULL == upload_id)
      {
        TBSYS_LOG(ERROR, "upload id is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (part_num < PARTNUM_MIN && part_num > PARTNUM_MAX)
      {
        TBSYS_LOG(ERROR, "part_num is out limit %d", part_num);
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (NULL == local_file)
      {
        TBSYS_LOG(ERROR, "local file is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if ((fd = ::open(local_file, O_RDONLY)) < 0)
      {
        TBSYS_LOG(ERROR, "open local file %s fail: %s", local_file, strerror(errno));
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        char* buf = new char[MAX_BATCH_DATA_LENGTH];
        int64_t read_len = 0, write_len = 0;
        int64_t offset = 0;

        while (TFS_SUCCESS == ret)
        {
          if ((read_len = ::read(fd, buf, MAX_BATCH_DATA_LENGTH)) < 0)
          {
            ret = EXIT_INVALID_ARGU_ERROR;
            TBSYS_LOG(ERROR, "read local file %s fail, error: %s", local_file, strerror(errno));
            break;
          }
          // jump for non_local_file
          if (read_len == 0 && offset != 0)
          {
            break;
          }
          do
          {
            write_len = pwrite_upload_multipart(bucket_name, object_name, buf, offset, read_len, upload_id, part_num, user_info);
            if (write_len < 0)
            {
              TBSYS_LOG(ERROR, "upload multipart fail. bucket: %s, object: %s", bucket_name, object_name);
              ret = TFS_ERROR;
              break;
            }
            offset += write_len;
            read_len -= write_len;
          }while (read_len > 0);

          //jump for empty local_file
          if (offset == 0)
          {
            break;
          }
        }

        tbsys::gDeleteA(buf);
        ::close(fd);
      }

      return ret;
    }

    int64_t KvMetaClientImpl::pwrite_upload_multipart(const char *bucket_name,
        const char *object_name, const void *buffer, int64_t offset,
        int64_t length, const char* upload_id,
        const int32_t part_num, const UserInfo &user_info)
    {
      int64_t ret = EXIT_GENERAL_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name of object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (buffer == NULL || length < 0)
      {
        TBSYS_LOG(ERROR, "invalid buffer, length: %"PRI64_PREFIX"d", length);
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (NULL == upload_id)
      {
        TBSYS_LOG(ERROR, "upload id is null");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (part_num < PARTNUM_MIN && part_num > PARTNUM_MAX)
      {
        TBSYS_LOG(ERROR, "part_num is out limit");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else if (offset == 0 && length == 0)
      {
        ObjectInfo object_info_null;
        object_info_null.has_meta_info_ = true;
        object_info_null.has_customize_info_ = true;
        object_info_null.meta_info_.max_tfs_file_size_ = MAX_SEGMENT_SIZE;

        ret = do_upload_multipart(bucket_name, object_name, object_info_null, user_info, upload_id, part_num);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "p-upload multipart fail, bucket: %s, object: %s, upload_id: %s, part_num: %d, ret: %"PRI64_PREFIX"d",
              bucket_name, object_name, upload_id, part_num, ret);
          if (TFS_ERROR == ret)
          {
            ret = EXIT_GENERAL_ERROR;
          }
        }
      }
      else
      {
        int ns_get_index = 0;
        string ns_addr;
        ns_addr = tfs_cluster_manager_->get_write_ns_addr(ns_get_index++);
        if (ns_addr.empty())
        {
          TBSYS_LOG(ERROR, "select write ns failed");
        }
        else
        {
          int32_t cluster_id = tfs_meta_manager_.get_cluster_id(ns_addr.c_str());
          int64_t left_length = length;
          int64_t cur_offset = offset;
          int64_t cur_pos = 0;
          vector<FragMeta> v_frag_meta;
          vector<TfsFileInfo> v_tfs_file_info;
          do
          {
            // write MAX_BATCH_DATA_LENGTH(8M) to tfs cluster
            int64_t write_length = min(left_length, MAX_BATCH_DATA_LENGTH);
            v_frag_meta.clear();

            int64_t real_write_length = write_data(ns_addr.c_str(),
                reinterpret_cast<const char*>(buffer) + cur_pos,
                cur_offset, write_length, &v_frag_meta);
            if (real_write_length != write_length)
            {
              TBSYS_LOG(ERROR, "write ns %s failed error, cur_pos: %"PRI64_PREFIX"d"
                  "write_length(%"PRI64_PREFIX"d) => real_length(%"PRI64_PREFIX"d)",
                  ns_addr.c_str(), cur_pos, write_length, real_write_length);
              ns_addr = tfs_cluster_manager_->get_write_ns_addr(ns_get_index++);
              if (ns_addr.empty())
              {
                TBSYS_LOG(ERROR, "select write ns failed");
                break;
              }
              cluster_id = tfs_meta_manager_.get_cluster_id(ns_addr.c_str());
              continue;
            }
            TBSYS_LOG(DEBUG, "write tfs data, cluster_id: %d, cur_offset: %"PRI64_PREFIX"d, write_length: %"PRI64_PREFIX"d",
                cluster_id, cur_offset, write_length);

            vector<FragMeta>::iterator iter = v_frag_meta.begin();
            v_tfs_file_info.clear();
            for (; v_frag_meta.end() != iter; iter++)
            {
              ObjectInfo object_info;
              if (0 == iter->offset_)
              {
                object_info.has_meta_info_ = true;
                object_info.has_customize_info_ = true;
                object_info.meta_info_.max_tfs_file_size_ = MAX_SEGMENT_SIZE;
                TBSYS_LOG(DEBUG, "first object info, will put meta info.");
                object_info.meta_info_.dump();
              }
              else
              {
                object_info.has_meta_info_ = false;
                object_info.has_customize_info_ = false;
              }
              TfsFileInfo tmp_tfs_info;
              tmp_tfs_info.offset_ = iter->offset_;
              tmp_tfs_info.block_id_ = iter->block_id_;
              tmp_tfs_info.file_id_ = iter->file_id_;
              tmp_tfs_info.cluster_id_ = cluster_id;
              tmp_tfs_info.file_size_ = iter->size_;
              object_info.v_tfs_file_info_.push_back(tmp_tfs_info);
              v_tfs_file_info.push_back(tmp_tfs_info);

              ret = do_upload_multipart(bucket_name, object_name, object_info, user_info, upload_id, part_num);

              if (TFS_SUCCESS != ret)
              {
                TBSYS_LOG(ERROR, "upload multipart fail, bucket: %s, object: %s, offset: %"PRI64_PREFIX"d, upload_id: %s, part_num: %d, ret: %"PRI64_PREFIX"d",
                    bucket_name, object_name, iter->offset_, upload_id, part_num, ret);
                if (TFS_ERROR == ret)
                {
                  ret = EXIT_GENERAL_ERROR;
                }
                break;
              }
            }
            if (TFS_SUCCESS != ret)
            {
              unlink_file(v_tfs_file_info);
              break;
            }

            left_length -= real_write_length;
            cur_pos += real_write_length;
            cur_offset += real_write_length;
          }
          while (left_length > 0 && TFS_SUCCESS == ret);

          if (TFS_SUCCESS == ret)
          {
            ret = length - left_length;
          }
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::complete_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id, const std::vector<int32_t>& v_part_num)
    {
      TfsRetType ret = TFS_SUCCESS;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if(upload_id == NULL)
      {
        TBSYS_LOG(ERROR, "upload_id is null ");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        ret = do_complete_multipart(bucket_name, object_name, upload_id, v_part_num);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "complete multipart failed. bucket: %s object: %s", bucket_name, object_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::list_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id, std::vector<int32_t>* const p_v_part_num)
    {
      TfsRetType ret = TFS_SUCCESS;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (NULL == upload_id)
      {
        TBSYS_LOG(ERROR, "upload id is null ");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        ret = do_list_multipart(bucket_name, object_name, upload_id, p_v_part_num);
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "list multipart failed. bucket: %s", bucket_name);
        }
      }

      return ret;
    }

    TfsRetType KvMetaClientImpl::abort_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id)
    {
      TfsRetType ret = TFS_ERROR;
      if (!is_valid_bucket_name(bucket_name) || !is_valid_object_name(object_name))
      {
        TBSYS_LOG(ERROR, "bucket name or object name is invalid ");
        ret = EXIT_INVALID_FILE_NAME;
      }
      else if (NULL == upload_id)
      {
        TBSYS_LOG(ERROR, "upload id is null ");
        ret = EXIT_INVALID_ARGU_ERROR;
      }
      else
      {
        vector<TfsFileInfo> v_tfs_file_info;
        bool still_have = false;
        do
        {
          ObjectInfo object_info;
          still_have = false;
          ret = do_abort_multipart(bucket_name, object_name, upload_id, &object_info, &still_have);
          TBSYS_LOG(DEBUG, "bucket_name %s object_name %s "
              " still_have %d", bucket_name, object_name, still_have);
          TBSYS_LOG(DEBUG, "del multi vector size ================= is: %lu",
              object_info.v_tfs_file_info_.size());
          if (TFS_SUCCESS == ret)
          {
            v_tfs_file_info.clear();
            size_t i = 0;
            for(; i < object_info.v_tfs_file_info_.size(); ++i)
            {
              TfsFileInfo tfs_info(object_info.v_tfs_file_info_[i].cluster_id_,
              object_info.v_tfs_file_info_[i].block_id_,
              object_info.v_tfs_file_info_[i].file_id_,
              object_info.v_tfs_file_info_[i].offset_,
              object_info.v_tfs_file_info_[i].file_size_);
              v_tfs_file_info.push_back(tfs_info);
            }
          }

          if (TFS_SUCCESS == ret && v_tfs_file_info.size() > 0)
          {
            ret = unlink_file(v_tfs_file_info);
          }

        } while(still_have);

        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "del object failed. bucket: %s, object: %s", bucket_name, object_name);
        }
      }
      return ret;
    }
    int KvMetaClientImpl::do_init_multipart(const char *bucket_name, const char *object_name, std::string* const upload_id)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_init_multipart(meta_server_id, bucket_name, object_name, upload_id);

        if (EXIT_NETWORK_ERROR == ret)
        {
          fail_count_++;
        }
        if (need_update_table(ret))
        {
          update_table_from_rootserver();
        }
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_upload_multipart(const char *bucket_name,
        const char *object_name, const ObjectInfo &object_info, const UserInfo &user_info,
        const char* upload_id, const int32_t part_num)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_upload_multipart(meta_server_id, bucket_name, object_name, upload_id, part_num, object_info, user_info);

        if (EXIT_NETWORK_ERROR == ret)
        {
          fail_count_++;
        }
        if (need_update_table(ret))
        {
          update_table_from_rootserver();
        }
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_complete_multipart(const char *bucket_name, const char *object_name,
        const char* upload_id, const std::vector<int32_t>& v_part_num)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_complete_multipart(meta_server_id, bucket_name, object_name, upload_id, v_part_num);

        if (EXIT_NETWORK_ERROR == ret)
        {
          fail_count_++;
        }
        if (need_update_table(ret))
        {
          update_table_from_rootserver();
        }
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }

    int KvMetaClientImpl::do_list_multipart(const char *bucket_name, const char *object_name,
                const char* upload_id, std::vector<int32_t>* const p_v_part_num)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_list_multipart(meta_server_id, bucket_name, object_name, upload_id, p_v_part_num);

        if (EXIT_NETWORK_ERROR == ret)
        {
          fail_count_++;
        }
        if (need_update_table(ret))
        {
          update_table_from_rootserver();
        }
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }
    int KvMetaClientImpl::do_abort_multipart(const char *bucket_name, const char *object_name,
        const char* upload_id, common::ObjectInfo *object_info, bool* still_have)
    {
      int ret = TFS_SUCCESS;
      uint64_t meta_server_id = 0;
      int32_t retry = ClientConfig::meta_retry_count_;
      do
      {
        meta_server_id = get_meta_server_id();
        ret = KvMetaHelper::do_abort_multipart(meta_server_id, bucket_name, object_name, upload_id, object_info, still_have);

        if (EXIT_NETWORK_ERROR == ret)
        {
          fail_count_++;
        }
        if (need_update_table(ret))
        {
          update_table_from_rootserver();
        }
      }
      while ((EXIT_NETWORK_ERROR == ret || EXIT_INVALID_KV_META_SERVER == ret) && --retry);

      return ret;
    }



    bool KvMetaClientImpl::is_valid_customize_info(const CustomizeInfo &customize_info)
    {
      bool is_valid = true;
      int32_t meta_size = customize_info.meta_data_.size();

      int32_t len = 0;
      if (meta_size > 0)
      {
        MAP_STRING_ITER iter = customize_info.meta_data_.begin();
        for (; iter != customize_info.meta_data_.end(); iter++)
        {
          len += iter->first.length();
          len += iter->second.length();

          if (len > MAX_CUSTOMIZE_INFO_SIZE)
          {
            is_valid = false;
            break;
          }
        }
      }

      return is_valid;
    }
  }
}

