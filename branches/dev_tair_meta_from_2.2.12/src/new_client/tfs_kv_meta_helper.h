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
#include "common/kv_meta_define.h"

namespace tfs
{
  namespace client
  {
    class KvMetaHelper
    {
    public:
      static int do_put_bucket(const uint64_t server_id, const char *bucket_name);
      // TODO: parameter
      static int do_get_bucket(const uint64_t server_id, const char *bucket_name,
                               const char* prefix, const char* start_key,
                               const int32_t limit, std::vector<std::string>& v_object_name);
      static int do_del_bucket(const uint64_t server_id, const char *bucket_name);

      static int do_put_object(const uint64_t server_id,
                               const char *bucket_name, const char *object_name,
                               const common::ObjectInfo &object_info);
      static int do_get_object(const uint64_t server_id,
                               const char *bucket_name, const char *object_name,
                               common::ObjectInfo *object_info, bool *still_have);
      static int do_del_object(const uint64_t server_id,
                               const char *bucket_name,
                               const char *object_name);
    };
  }
}

#endif
