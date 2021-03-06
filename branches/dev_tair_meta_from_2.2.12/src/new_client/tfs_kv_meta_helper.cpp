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
#include "tfs_kv_meta_helper.h"
#include "common/new_client.h"
#include "common/client_manager.h"
#include "common/base_packet.h"
#include "common/status_message.h"
#include "message/message_factory.h"
#include "message/kv_meta_message.h"
#include "client_config.h"

using namespace tfs::client;
using namespace tfs::common;
using namespace tfs::message;
using namespace std;

int KvMetaHelper::get_table(const uint64_t server_id,
    KvMetaTable &table)
{
  GetTableFromKvRtsMessage req_gtfkr_msg;

  tbnet::Packet* rsp = NULL;
  NewClient *client = NewClientManager::get_instance().create_client();

  int iret = send_msg_to_server(server_id, client, &req_gtfkr_msg, rsp, ClientConfig::wait_timeout_);

  if (TFS_SUCCESS != iret)
  {
    TBSYS_LOG(ERROR, "call get kv table fail,"
        "server_addr: %s, ret: %d",
        tbsys::CNetUtil::addrToString(server_id).c_str(), iret);
    iret = EXIT_NETWORK_ERROR;
  }
  else if (RSP_KV_RT_GET_TABLE_MESSAGE == rsp->getPCode())
  {
    GetTableFromKvRtsResponseMessage* rsp_get_table = dynamic_cast<GetTableFromKvRtsResponseMessage*>(rsp);
    table.v_meta_table_ = rsp_get_table->get_mutable_table().v_meta_table_;
  }
  else
  {
    iret = EXIT_UNKNOWN_MSGTYPE;
    TBSYS_LOG(ERROR, "call get kv table fail,"
        "server_addr: %s, ret: %d: msg type: %d",
        tbsys::CNetUtil::addrToString(server_id).c_str(), iret, rsp->getPCode());
  }
  NewClientManager::get_instance().destroy_client(client);
  return iret;
}

int KvMetaHelper::do_put_bucket(const uint64_t server_id, const char *bucket_name,
    const BucketMetaInfo& bucket_meta_info, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutBucketMessage req_pb_msg;
    req_pb_msg.set_bucket_name(bucket_name);
    req_pb_msg.set_bucket_meta_info(bucket_meta_info);
    req_pb_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_pb_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put bucket return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_bucket(const uint64_t server_id, const char *bucket_name,
                                const char *prefix, const char *start_key, char delimiter,
                                const int32_t limit, vector<ObjectMetaInfo> *v_object_meta_info,
                                vector<string> *v_object_name, set<string> *s_common_prefix,
                                int8_t *is_truncated, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetBucketMessage req_gb_msg;
    req_gb_msg.set_bucket_name(bucket_name);
    req_gb_msg.set_prefix(NULL == prefix ? "" : string(prefix));
    req_gb_msg.set_start_key(NULL == start_key ? "" : string(start_key));
    req_gb_msg.set_delimiter(delimiter);
    req_gb_msg.set_limit(limit);
    req_gb_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_gb_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_BUCKET_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetBucketMessage* rsp_gb_msg = dynamic_cast<RspKvMetaGetBucketMessage*>(rsp);
      *v_object_meta_info = *(rsp_gb_msg->get_mutable_v_object_meta_info());
      *v_object_name = *(rsp_gb_msg->get_mutable_v_object_name());
      *s_common_prefix = *(rsp_gb_msg->get_mutable_s_common_prefix());
      *is_truncated = *(rsp_gb_msg->get_mutable_truncated());
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "get bucket return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_list_multipart_object(const uint64_t server_id, const char *bucket_name,
                                const char *prefix, const char *start_key, const char *start_id, char delimiter,
                                const int32_t limit, ListMultipartObjectResult *list_multipart_object_result,
                                const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaListMultipartObjectMessage req_lmo_msg;
    req_lmo_msg.set_bucket_name(bucket_name);
    req_lmo_msg.set_prefix(NULL == prefix ? "" : string(prefix));
    req_lmo_msg.set_start_key(NULL == start_key ? "" : string(start_key));
    req_lmo_msg.set_start_id(NULL == start_id ? "" : string(start_id));
    req_lmo_msg.set_delimiter(delimiter);
    req_lmo_msg.set_limit(limit);
    req_lmo_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_lmo_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call list multipart object fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_LIST_MULTIPART_OBJECT_MESSAGE == rsp->getPCode())
    {
      RspKvMetaListMultipartObjectMessage* rsp_lmo_msg = dynamic_cast<RspKvMetaListMultipartObjectMessage*>(rsp);
      list_multipart_object_result->limit_ = *(rsp_lmo_msg->get_limit());
      list_multipart_object_result->v_object_upload_info_ = *(rsp_lmo_msg->get_mutable_v_object_upload_info());
      list_multipart_object_result->is_truncated_ = *(rsp_lmo_msg->get_truncated());
      list_multipart_object_result->s_common_prefix_ = *(rsp_lmo_msg->get_mutable_s_common_prefix());
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "list multipart object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "list multipart object fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_del_bucket(const uint64_t server_id, const char *bucket_name, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaDelBucketMessage req_db_msg;
    req_db_msg.set_bucket_name(bucket_name);
    req_db_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_db_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call del bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "del bucket return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "del bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;

}

int KvMetaHelper::do_head_bucket(const uint64_t server_id, const char *bucket_name, BucketMetaInfo *bucket_meta_info, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaHeadBucketMessage req_hb_msg;
    req_hb_msg.set_bucket_name(bucket_name);
    req_hb_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_hb_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call head bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_HEAD_BUCKET_MESSAGE == rsp->getPCode())
    {
      RspKvMetaHeadBucketMessage *resp_hb_msg = dynamic_cast<RspKvMetaHeadBucketMessage*>(rsp);
      *bucket_meta_info = *(resp_hb_msg->get_bucket_meta_info());
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "head bucket return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "head bucket fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

//put bucket logging
int KvMetaHelper::do_put_bucket_logging(const uint64_t server_id, const char *bucket_name,
    const bool logging_status, const char *target_bucket_name,
    const char *target_prefix, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == target_bucket_name || NULL == target_prefix)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutBucketLoggingMessage req_pbl_msg;
    req_pbl_msg.set_bucket_name(bucket_name);
    req_pbl_msg.set_logging_status(logging_status);
    req_pbl_msg.set_target_bucket_name(target_bucket_name);
    req_pbl_msg.set_target_prefix(target_prefix);
    req_pbl_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_pbl_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put bucket logging fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put bucket logging return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put bucket logging fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_bucket_logging(const uint64_t server_id, const char *bucket_name,
    bool *logging_status, string *target_bucket_name,
    string *target_prefix, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == logging_status
      || NULL == target_bucket_name || NULL == target_prefix)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetBucketLoggingMessage req_gbl_msg;
    req_gbl_msg.set_bucket_name(bucket_name);
    req_gbl_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_gbl_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get bucket logging fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_BUCKET_LOGGING_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetBucketLoggingMessage* rsp_gbl_msg = dynamic_cast<RspKvMetaGetBucketLoggingMessage*>(rsp);
      *logging_status = *(rsp_gbl_msg->get_mutable_logging_status());
      *target_bucket_name = *(rsp_gbl_msg->get_mutable_target_bucket_name());
      *target_prefix = *(rsp_gbl_msg->get_mutable_target_prefix());
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get bucket logging fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_put_bucket_acl(const uint64_t server_id, const char *bucket_name,
    const CANNED_ACL acl, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutBucketAclMessage req_pba_msg;
    req_pba_msg.set_bucket_name(bucket_name);
    req_pba_msg.set_canned_acl(acl);
    req_pba_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_pba_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put bucket acl return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_put_bucket_acl(const uint64_t server_id, const char *bucket_name,
    const MAP_INT64_INT &bucket_acl_map, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutBucketAclMessage req_pba_msg;
    req_pba_msg.set_bucket_name(bucket_name);
    req_pba_msg.set_bucket_acl_map(bucket_acl_map);
    req_pba_msg.set_canned_acl(static_cast<CANNED_ACL>(-1));
    req_pba_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_pba_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put bucket acl return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_bucket_acl(const uint64_t server_id, const char *bucket_name,
    MAP_INT64_INT *bucket_acl_map, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == bucket_acl_map)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetBucketAclMessage req_gba_msg;
    req_gba_msg.set_bucket_name(bucket_name);
    req_gba_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_gba_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_BUCKET_ACL_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetBucketAclMessage* rsp_gba_msg = dynamic_cast<RspKvMetaGetBucketAclMessage*>(rsp);
      *bucket_acl_map = *(rsp_gba_msg->get_bucket_acl_map());
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get bucket acl fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_service(const uint64_t server_id, BucketsResult *buckets_result, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == buckets_result)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetServiceMessage req_gs_msg;
    req_gs_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_gs_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get service fail,"
          "server_addr: %s, owner_id: %"PRI64_PREFIX"d, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), user_info.owner_id_, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_SERVICE_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetServiceMessage* rsp_gs_msg = dynamic_cast<RspKvMetaGetServiceMessage*>(rsp);
      *buckets_result = *(rsp_gs_msg->get_mutable_buckets_result());
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get service fail,"
          "server_addr: %s, owner_id: %"PRI64_PREFIX"d, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), user_info.owner_id_, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

//for bucket tagging
int KvMetaHelper::do_put_bucket_tag(const uint64_t server_id, const char *bucket_name,
    const MAP_STRING &bucket_tag_map)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutBucketTagMessage req_pbt_msg;
    req_pbt_msg.set_bucket_name(bucket_name);
    req_pbt_msg.set_bucket_tag_map(bucket_tag_map);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_pbt_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put bucket tag return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_bucket_tag(const uint64_t server_id, const char *bucket_name,
    MAP_STRING *bucket_tag_map)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == bucket_tag_map)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetBucketTagMessage req_gbt_msg;
    req_gbt_msg.set_bucket_name(bucket_name);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_gbt_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_BUCKET_TAG_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetBucketTagMessage* rsp_gbt_msg = dynamic_cast<RspKvMetaGetBucketTagMessage*>(rsp);
      *bucket_tag_map = *(rsp_gbt_msg->get_bucket_tag_map());
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_del_bucket_tag(const uint64_t server_id, const char *bucket_name)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaDelBucketTagMessage req_dbt_msg;
    req_dbt_msg.set_bucket_name(bucket_name);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_dbt_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call del bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "del bucket tag return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "del bucket tag fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_put_object(const uint64_t server_id, const char *bucket_name,const char *object_name,
    const ObjectInfo &object_info, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaPutObjectMessage req_po_msg;
    req_po_msg.set_bucket_name(bucket_name);
    req_po_msg.set_file_name(object_name);
    req_po_msg.set_object_info(object_info);
    req_po_msg.set_user_info(user_info);


    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_po_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call put object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "put object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "put object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_object(const uint64_t server_id, const char *bucket_name, const char *object_name,
const int64_t offset, const int64_t length, ObjectInfo *object_info, bool *still_have, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaGetObjectMessage req_go_msg;
    req_go_msg.set_bucket_name(bucket_name);
    req_go_msg.set_file_name(object_name);
    req_go_msg.set_offset(offset);
    req_go_msg.set_length(length);
    req_go_msg.set_user_info(user_info);
    TBSYS_LOG(DEBUG, "bucket_name %s object_name %s "
            "offset %ld length %ld still_have %d", bucket_name, object_name, offset, length, *still_have);
    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_go_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_GET_OBJECT_MESSAGE == rsp->getPCode())
    {
      RspKvMetaGetObjectMessage* rsp_go_msg = dynamic_cast<RspKvMetaGetObjectMessage*>(rsp);
      *object_info = rsp_go_msg->get_object_info();
      *still_have = rsp_go_msg->get_still_have();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "get object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get object fail, "
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_del_multi_object(const uint64_t server_id, const char *bucket_name,
    const set<string> &s_object_name, const bool quiet, DeleteResult *delete_result, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || s_object_name.size() <= 0u)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaDelMultiObjectMessage req_dmo_msg;
    req_dmo_msg.set_bucket_name(bucket_name);
    req_dmo_msg.set_s_file_name(s_object_name);
    req_dmo_msg.set_quiet_mode(quiet);
    req_dmo_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_dmo_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call del multi object fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_DEL_MULTI_OBJECT_MESSAGE == rsp->getPCode())
    {
      RspKvMetaDelMultiObjectMessage* rsp_dmo_msg = dynamic_cast<RspKvMetaDelMultiObjectMessage*>(rsp);
      *delete_result = *(rsp_dmo_msg->get_delete_result());
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "del multi object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "del multi object fail,"
          "server_addr: %s, bucket_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_del_object(const uint64_t server_id, const char *bucket_name,
    const char *object_name, ObjectInfo *object_info, bool *still_have, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaDelObjectMessage req_do_msg;
    req_do_msg.set_bucket_name(bucket_name);
    req_do_msg.set_file_name(object_name);
    req_do_msg.set_user_info(user_info);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_do_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call del object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_DEL_OBJECT_MESSAGE == rsp->getPCode())
    {
      RspKvMetaDelObjectMessage* rsp_do_msg = dynamic_cast<RspKvMetaDelObjectMessage*>(rsp);
      *object_info = rsp_do_msg->get_object_info();
      *still_have = rsp_do_msg->get_still_have();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "del object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "del object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_head_object(const uint64_t server_id, const char *bucket_name,
    const char *object_name, ObjectInfo *object_info, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaHeadObjectMessage req_ho_msg;
    req_ho_msg.set_bucket_name(bucket_name);
    req_ho_msg.set_file_name(object_name);
    req_ho_msg.set_user_info(user_info);
    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_ho_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call head object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_HEAD_OBJECT_MESSAGE == rsp->getPCode())
    {
      RspKvMetaHeadObjectMessage* resp_ho_msg = dynamic_cast<RspKvMetaHeadObjectMessage*>(rsp);
      *object_info = *(resp_ho_msg->get_object_info());
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "head object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "head object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_apply_authorize(const uint64_t server_id, const char *user_name,
    char* access_key_id, char *access_secret_key)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == user_name || NULL == access_key_id || NULL == access_secret_key)
  {
    ret = EXIT_INVALID_ARGU_ERROR;
  }
  else
  {
    ReqApplyAuthorizeMessage req_apply_authorize_msg;
    req_apply_authorize_msg.set_user_name(user_name);
    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_apply_authorize_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call apply authorize fail,"
          "server_addr: %s, user_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), user_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KV_APPLY_AUTHORIZE_MESSAGE == rsp->getPCode())
    {
      RspApplyAuthorizeMessage* rsp_apply_authorize_msg = dynamic_cast<RspApplyAuthorizeMessage*>(rsp);
      int32_t access_key_id_len = rsp_apply_authorize_msg->get_access_key_id().size();
      rsp_apply_authorize_msg->get_access_key_id().copy(access_key_id, access_key_id_len, 0);
      access_key_id[access_key_id_len] = '\0';

      int32_t access_secret_key_len = rsp_apply_authorize_msg->get_access_secret_key().size();
      rsp_apply_authorize_msg->get_access_secret_key().copy(access_secret_key, access_secret_key_len, 0);
      access_secret_key[access_secret_key_len] = '\0';
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "apply authorize return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "apply authorize fail,"
          "server_addr: %s, user_name: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          user_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_get_authorize(const uint64_t server_id, const char* access_key_id, AuthorizeValueInfo* authorizeinfo)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == access_key_id)
  {
    ret = EXIT_INVALID_ARGU_ERROR;
  }
  else
  {
    ReqGetAuthorizeMessage req_get_authorize_msg;
    req_get_authorize_msg.set_access_key_id(access_key_id);
    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_get_authorize_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call get authorize fail,"
          "server_addr: %s, access_key_id: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), access_key_id, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KV_GET_AUTHORIZE_MESSAGE == rsp->getPCode())
    {
      RspGetAuthorizeMessage* rsp_get_authorize_msg = dynamic_cast<RspGetAuthorizeMessage*>(rsp);
      *authorizeinfo = rsp_get_authorize_msg->get_authorize_info();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "apply authorize return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "get authorize fail,"
          "server_addr: %s, access_key_id: %s, "
          "ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          access_key_id, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_init_multipart(const uint64_t server_id, const char *bucket_name,
    const char *object_name, std::string* const upload_id)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaInitMulitpartMessage req_init_multi_msg;
    req_init_multi_msg.set_bucket_name(bucket_name);
    req_init_multi_msg.set_file_name(object_name);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_init_multi_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call init multi part fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_INIT_MULTIPART_MESSAGE == rsp->getPCode())
    {
      RspKvMetaInitMulitpartMessage* rsp_init_multi_msg = dynamic_cast<RspKvMetaInitMulitpartMessage*>(rsp);
      *upload_id = rsp_init_multi_msg->get_upload_id();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "init multi part return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "init multi part fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_upload_multipart(const uint64_t server_id, const char *bucket_name,const char *object_name,
    const char *upload_id, const int32_t part_num,
    const ObjectInfo &object_info, const UserInfo &user_info)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaUploadMulitpartMessage req_up_mu_msg;
    req_up_mu_msg.set_bucket_name(bucket_name);
    req_up_mu_msg.set_file_name(object_name);
    req_up_mu_msg.set_upload_id(upload_id);
    req_up_mu_msg.set_part_num(part_num);
    req_up_mu_msg.set_object_info(object_info);
    req_up_mu_msg.set_user_info(user_info);


    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_up_mu_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call upload multipart fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, upload_id:%s part_num:%d ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, upload_id, part_num, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "upload multipart return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "call upload multipart fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, upload_id:%s part_num:%d ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, upload_id, part_num, ret);
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_complete_multipart(const uint64_t server_id, const char *bucket_name,
    const char *object_name, const char* upload_id, const std::vector<int32_t>& v_part_num)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name || NULL == upload_id)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaCompleteMulitpartMessage req_comp_multi_msg;
    req_comp_multi_msg.set_bucket_name(bucket_name);
    req_comp_multi_msg.set_file_name(object_name);
    req_comp_multi_msg.set_upload_id(upload_id);
    req_comp_multi_msg.set_v_part_num(v_part_num);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_comp_multi_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call complete multi part fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, upload_id:%s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, upload_id, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "complete multipart return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "call upload multipart fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, upload_id:%s ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, upload_id, ret);
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_list_multipart(const uint64_t server_id, const char *bucket_name,
    const char *object_name, const char *upload_id, std::vector<int32_t>* const p_v_part_num)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name || NULL == upload_id)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaListMulitpartMessage req_list_multi_msg;
    req_list_multi_msg.set_bucket_name(bucket_name);
    req_list_multi_msg.set_file_name(object_name);
    req_list_multi_msg.set_upload_id(upload_id);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_list_multi_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call list multi part fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, upload_id: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, upload_id, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_LIST_MULTIPART_MESSAGE == rsp->getPCode())
    {
      RspKvMetaListMulitpartMessage* rsp_list_multi_msg = dynamic_cast<RspKvMetaListMulitpartMessage*>(rsp);
      *p_v_part_num = rsp_list_multi_msg->get_v_part_num();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "list multi part return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "list multi part fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

int KvMetaHelper::do_abort_multipart(const uint64_t server_id, const char *bucket_name,
    const char *object_name, const char *upload_id, ObjectInfo *object_info, bool *still_have)
{
  int ret = TFS_SUCCESS;
  if (0 == server_id)
  {
    ret = EXIT_INVALID_KV_META_SERVER;
  }
  else if (NULL == bucket_name || NULL == object_name)
  {
    ret = EXIT_INVALID_FILE_NAME;
  }
  else
  {
    ReqKvMetaAbortMulitpartMessage req_abort_multi_msg;
    req_abort_multi_msg.set_bucket_name(bucket_name);
    req_abort_multi_msg.set_file_name(object_name);
    req_abort_multi_msg.set_upload_id(upload_id);

    tbnet::Packet* rsp = NULL;
    NewClient* client = NewClientManager::get_instance().create_client();
    ret = send_msg_to_server(server_id, client, &req_abort_multi_msg, rsp, ClientConfig::wait_timeout_);
    if (TFS_SUCCESS != ret)
    {
      TBSYS_LOG(ERROR, "call abort multipart fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(), bucket_name, object_name, ret);
      ret = EXIT_NETWORK_ERROR;
    }
    else if (RSP_KVMETA_ABORT_MULTIPART_MESSAGE == rsp->getPCode())
    {
      RspKvMetaAbortMulitpartMessage* rsp_abort_multi_msg = dynamic_cast<RspKvMetaAbortMulitpartMessage*>(rsp);
      *object_info = rsp_abort_multi_msg->get_object_info();
      *still_have = rsp_abort_multi_msg->get_still_have();
    }
    else if (STATUS_MESSAGE == rsp->getPCode())
    {
      StatusMessage* resp_status_msg = dynamic_cast<StatusMessage*>(rsp);
      if ((ret = resp_status_msg->get_status()) != STATUS_MESSAGE_OK)
      {
        TBSYS_LOG(ERROR, "del object return error, ret: %d", ret);
      }
    }
    else
    {
      ret = EXIT_UNKNOWN_MSGTYPE;
      TBSYS_LOG(ERROR, "del object fail,"
          "server_addr: %s, bucket_name: %s, "
          "object_name: %s, ret: %d, msg type: %d",
          tbsys::CNetUtil::addrToString(server_id).c_str(),
          bucket_name, object_name, ret, rsp->getPCode());
    }
    NewClientManager::get_instance().destroy_client(client);
  }
  return ret;
}

