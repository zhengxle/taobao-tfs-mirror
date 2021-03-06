/*
 * (C) 2007-2013 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: tfs_meta_client_api.cpp 943 2011-10-20 01:40:25Z chuyu $
 *
 * Authors:
 *    mingyan<mingyan.zc@taobao.com>
 *      - initial release
 *
 */

#include "common/define.h"
#include "tfs_kv_meta_client_api.h"
#include "tfs_kv_meta_client_impl.h"

namespace tfs
{
  namespace client
  {
    using namespace std;

    KvMetaClient::KvMetaClient():impl_(NULL)
    {
      impl_ = new KvMetaClientImpl();
    }

    KvMetaClient::~KvMetaClient()
    {
      delete impl_;
      impl_ = NULL;
    }

    int KvMetaClient::initialize(const char *rs_addr)
    {
      return impl_->initialize(rs_addr);
    }

    int KvMetaClient::initialize(const uint64_t rs_addr)
    {
      return impl_->initialize(rs_addr);
    }

    void KvMetaClient::set_tfs_cluster_manager(TfsClusterManager *tfs_cluster_manager)
    {
      impl_->set_tfs_cluster_manager(tfs_cluster_manager);
    }

    TfsRetType KvMetaClient::put_bucket(const char *bucket_name,
        const common::UserInfo &user_info)
    {
      return impl_->put_bucket(bucket_name, user_info);
    }

    TfsRetType KvMetaClient::get_bucket(const char *bucket_name, const char *prefix,
        const char *start_key, const char delimiter, const int32_t limit,
        vector<common::ObjectMetaInfo> *v_object_meta_info,
        vector<string> *v_object_name, set<string> *s_common_prefix,
        int8_t *is_truncated, const common::UserInfo &user_info)
    {
      return impl_->get_bucket(bucket_name, prefix, start_key, delimiter, limit,
          v_object_meta_info, v_object_name, s_common_prefix, is_truncated, user_info);
    }

    TfsRetType KvMetaClient::del_bucket(const char *bucket_name,
        const common::UserInfo &user_info)
    {
      return impl_->del_bucket(bucket_name, user_info);
    }

    TfsRetType KvMetaClient::head_bucket(const char *bucket_name,
        common::BucketMetaInfo *bucket_meta_info, const common::UserInfo &user_info)
    {
      return impl_->head_bucket(bucket_name, bucket_meta_info, user_info);
    }

    TfsRetType KvMetaClient::put_bucket_acl(const char *bucket_name, const common::CANNED_ACL acl,
        const common::UserInfo &user_info)
    {
      return impl_->put_bucket_acl(bucket_name, acl, user_info);
    }

    TfsRetType KvMetaClient::put_bucket_acl(const char *bucket_name, const common::MAP_INT64_INT &bucket_acl_map,
        const common::UserInfo &user_info)
    {
      return impl_->put_bucket_acl(bucket_name, bucket_acl_map, user_info);
    }

    TfsRetType KvMetaClient::get_bucket_acl(const char *bucket_name, common::MAP_INT64_INT *bucket_acl_map,
        const common::UserInfo &user_info)
    {
      return impl_->get_bucket_acl(bucket_name, bucket_acl_map, user_info);
    }

    TfsRetType KvMetaClient::get_service(common::BucketsResult *buckets_result, const common::UserInfo &user_info)
    {
      return impl_->get_service(buckets_result, user_info);
    }

    TfsRetType KvMetaClient::put_bucket_logging(const char *bucket_name, const bool logging_status, const char *target_bucket_name, const char *target_prefix, const common::UserInfo &user_info)
    {
      return impl_->put_bucket_logging(bucket_name, logging_status, target_bucket_name, target_prefix, user_info);
    }

    TfsRetType KvMetaClient::get_bucket_logging(const char *bucket_name, bool *logging_status,
        string *target_bucket_name, string *target_prefix, const common::UserInfo &user_info)
    {
      return impl_->get_bucket_logging(bucket_name, logging_status, target_bucket_name, target_prefix, user_info);
    }

    TfsRetType KvMetaClient::put_bucket_tag(const char *bucket_name, const common::MAP_STRING &bucket_tag_map)
    {
      return impl_->put_bucket_tag(bucket_name, bucket_tag_map);
    }

    TfsRetType KvMetaClient::get_bucket_tag(const char *bucket_name, common::MAP_STRING *bucket_tag_map)
    {
      return impl_->get_bucket_tag(bucket_name, bucket_tag_map);
    }

    TfsRetType KvMetaClient::del_bucket_tag(const char *bucket_name)
    {
      return impl_->del_bucket_tag(bucket_name);
    }

    TfsRetType KvMetaClient::list_multipart_object(const char *bucket_name, const char *prefix,
        const char *start_key, const char *start_id, const char delimiter, const int32_t limit,
        common::ListMultipartObjectResult *list_multipart_object_result, const common::UserInfo &user_info)
    {
      return impl_->list_multipart_object(bucket_name, prefix, start_key, start_id, delimiter, limit,
          list_multipart_object_result, user_info);
    }

    TfsRetType KvMetaClient::put_object(const char *bucket_name,
        const char *object_name, const char* local_file,
        const common::UserInfo &user_info,
        const common::CustomizeInfo &customize_info)
    {
      return impl_->put_object(bucket_name, object_name, local_file, user_info, customize_info);
    }

    int64_t KvMetaClient::pwrite_object(const char *bucket_name,
        const char *object_name, const void *buf, const int64_t offset,
        const int64_t length, const common::UserInfo &user_info)
    {
      return impl_->pwrite_object(bucket_name, object_name, buf, offset,
          length, user_info, NULL);
    }

    int64_t KvMetaClient::pread_object(const char *bucket_name,
        const char *object_name, void *buf, const int64_t offset,
        const int64_t length, common::ObjectMetaInfo *object_meta_info,
        common::CustomizeInfo *customize_info,
        const common::UserInfo &user_info)
    {
      return impl_->pread_object(bucket_name, object_name, buf, offset,
          length, object_meta_info, customize_info, user_info);
    }

    TfsRetType KvMetaClient::get_object(const char *bucket_name,
        const char *object_name, const char* local_file,
        const common::UserInfo &user_info)
    {
      return impl_->get_object(bucket_name, object_name, local_file,
          NULL, NULL, user_info);
    }

    TfsRetType KvMetaClient::del_multi_object(const char *bucket_name, const std::set<std::string> &s_object_name,
        const bool quiet, common::DeleteResult *delete_result, const common::UserInfo &user_info)
    {
      return impl_->del_multi_object(bucket_name, s_object_name, quiet, delete_result, user_info);
    }

    TfsRetType KvMetaClient::del_object(const char *bucket_name,
        const char *object_name, const common::UserInfo &user_info)
    {
      return impl_->del_object(bucket_name, object_name, user_info);
    }

    TfsRetType KvMetaClient::head_object(const char *bucket_name,
        const char *object_name, common::ObjectInfo *object_info,
        const common::UserInfo &user_info)
    {
      return impl_->head_object(bucket_name, object_name, object_info, user_info);
    }

    TfsRetType KvMetaClient::apply_authorize(const char *user_name, char* access_key_id, char *access_secret_key)
    {
      return impl_->apply_authorize(user_name, access_key_id, access_secret_key);
    }

    TfsRetType KvMetaClient::get_authorize(const char* access_key_id, char *user_name, char *access_secret_key)
    {
      return impl_->get_authorize(access_key_id, user_name, access_secret_key);
    }
    TfsRetType KvMetaClient::init_multipart(const char *bucket_name,
        const char *object_name, std::string* const upload_id)
    {
      return impl_->init_multipart(bucket_name, object_name, upload_id);
    }

    TfsRetType KvMetaClient::upload_multipart(const char *bucket_name,
        const char *object_name, const char* local_file, const char* upload_id,
        const int32_t part_num, const common::UserInfo &user_info)
    {
      return impl_->upload_multipart(bucket_name, object_name, local_file, upload_id,
        part_num, user_info);
    }

    int64_t KvMetaClient::pwrite_upload_multipart(const char *bucket_name,
        const char *object_name, const void *buffer, int64_t offset,
        int64_t length, const char* upload_id,
        const int32_t part_num, const common::UserInfo &user_info)
    {
      return impl_->pwrite_upload_multipart(bucket_name,
             object_name, buffer, offset,
             length, upload_id, part_num, user_info);
    }

    TfsRetType KvMetaClient::complete_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id,
        const std::vector<int32_t>& v_part_num)
    {
      return impl_->complete_multipart(bucket_name, object_name, upload_id, v_part_num);
    }

    TfsRetType KvMetaClient::list_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id,
        std::vector<int32_t>* const p_v_part_num)
    {
      return impl_->list_multipart(bucket_name, object_name, upload_id, p_v_part_num);
    }

    TfsRetType KvMetaClient::abort_multipart(const char *bucket_name,
        const char *object_name, const char* upload_id)
    {
      return impl_->abort_multipart(bucket_name, object_name, upload_id);
    }
  }
}

