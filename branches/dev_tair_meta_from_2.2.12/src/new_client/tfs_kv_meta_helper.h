/*
 * (C) 2007-2013 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: tfs_meta_helper.cpp 918 2011-10-14 03:16:21Z daoan@taobao.com $
 *
 * Authors:
 *      mingyan(mingyan.zc@taobao.com)
 *      - initial release
 *
 */
#ifndef TFS_CLIENT_KVMETA_HELPER_H_
#define TFS_CLIENT_KVMETA_HELPER_H_

#include <vector>
#include <set>
#include "common/kv_meta_define.h"
#include "common/kv_rts_define.h"

namespace tfs
{
  namespace client
  {
    class KvMetaHelper
    {
    public:
      static int do_put_bucket(const uint64_t server_id, const char *bucket_name, const common::BucketMetaInfo& bucket_meta_info,
                               const common::UserInfo &user_info);
      static int get_table(const uint64_t server_id, common::KvMetaTable& table);
      // TODO: parameter
      static int do_get_bucket(const uint64_t server_id, const char *bucket_name,
                               const char *prefix, const char *start_key, char delimiter, const int32_t limit,
                               std::vector<common::ObjectMetaInfo> *v_object_meta_info,
                               std::vector<std::string> *v_object_name, std::set<std::string> *s_common_prefix,
                               int8_t *is_trucated, const common::UserInfo &user_info);
      static int do_del_bucket(const uint64_t server_id, const char *bucket_name, const common::UserInfo &user_info);
      static int do_head_bucket(const uint64_t server_id, const char *bucket_name, common::BucketMetaInfo *bucket_meta_info,
                                const common::UserInfo &user_info);

      static int do_list_multipart_object(const uint64_t server_id, const char *bucket_name,
                               const char *prefix, const char *start_key, const char *start_id, char delimiter,
                               const int32_t limit, common::ListMultipartObjectResult *list_multipart_object_result,
                               const common::UserInfo &user_info);

      static int do_put_object(const uint64_t server_id,
                               const char *bucket_name, const char *object_name,
                               const common::ObjectInfo &object_info, const common::UserInfo &user_info);
      static int do_get_object(const uint64_t server_id,
                               const char *bucket_name, const char *object_name,
                               const int64_t offset, const int64_t length,
                               common::ObjectInfo *object_info, bool *still_have,
                               const common::UserInfo &user_info);
      static int do_del_multi_object(const uint64_t server_id,
                                     const char *bucket_name,
                                     const std::set<std::string> &s_object_name,
                                     const bool quiet,
                                     common::DeleteResult *delete_result,
                               const common::UserInfo &user_info);
      static int do_del_object(const uint64_t server_id,
                               const char *bucket_name,
                               const char *object_name,
                               common::ObjectInfo *object_info, bool *still_have,
                               const common::UserInfo &user_info);
      static int do_head_object(const uint64_t server_id,
                               const char *bucket_name,
                               const char *object_name,
                               common::ObjectInfo *object_info,
                               const common::UserInfo &user_info);

      static int do_put_bucket_logging(const uint64_t server_id, const char *bucket_name,
          const bool logging_status, const char *target_bucket_name, const char *target_prefix,
          const common::UserInfo &user_info);

      static int do_get_bucket_logging(const uint64_t server_id, const char *bucket_name,
          bool *logging_status, std::string *target_bucket_name, std::string *target_prefix,
          const common::UserInfo &user_info);

      static int do_put_bucket_acl(const uint64_t server_id, const char *bucket_name,
          const common::CANNED_ACL acl, const common::UserInfo &user_info);

      static int do_put_bucket_acl(const uint64_t server_id, const char *bucket_name,
          const common::MAP_INT64_INT &bucket_acl_map, const common::UserInfo &user_info);
      static int do_get_bucket_acl(const uint64_t server_id, const char *bucket_name,
          common::MAP_INT64_INT *bucket_acl_map, const common::UserInfo &user_info);

      static int do_get_service(const uint64_t server_id, common::BucketsResult *buckets_result,
          const common::UserInfo &user_info);

      static int do_put_bucket_tag(const uint64_t server_id, const char *bucket_name,
          const common::MAP_STRING &bucket_tag_map);
      static int do_get_bucket_tag(const uint64_t server_id, const char *bucket_name, common::MAP_STRING *bucket_tag_map);
      static int do_del_bucket_tag(const uint64_t server_id, const char *bucket_name);

      static int do_apply_authorize(const uint64_t server_id, const char *user_name,
                                    char* access_key_id, char *access_secret_key);
      static int do_get_authorize(const uint64_t server_id, const char* access_key_id, common::AuthorizeValueInfo* authorizeinfo);

      static int do_init_multipart(const uint64_t server_id,
                                  const char *bucket_name,
                                  const char *object_name,
                                  std::string* const upload_id);
      static int do_upload_multipart(const uint64_t server_id, const char *bucket_name,const char *object_name,
                                     const char *upload_id, const int32_t part_num,
                                     const common::ObjectInfo &object_info, const common::UserInfo &user_info);
      static int do_complete_multipart(const uint64_t server_id, const char *bucket_name,
                                       const char *object_name, const char* upload_id,
                                       const std::vector<int32_t>& v_part_num);
      static int do_list_multipart(const uint64_t server_id, const char *bucket_name,
                                   const char *object_name, const char *upload_id,
                                   std::vector<int32_t>* const p_v_part_num);
      static int do_abort_multipart(const uint64_t server_id, const char *bucket_name,
                                    const char *object_name, const char *upload_id,
                                    common::ObjectInfo *object_info, bool *still_have);
    };
  }
}

#endif
