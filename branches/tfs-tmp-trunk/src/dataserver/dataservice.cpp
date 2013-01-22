/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: dataservice.cpp 1000 2011-11-03 02:40:09Z mingyan.zc@taobao.com $
 *
 * Authors:
 *   duolong <duolong@taobao.com>
 *      - initial release
 *   qushan<qushan@taobao.com>
 *      - modify 2009-03-27
 *   linqing <linqing.zyd@taobao.com>
 *      - modify 2012-12-12
 *
 */
#include "dataservice.h"
#include "ds_define.h"

#include <Memory.hpp>
#include "common/new_client.h"
#include "common/client_manager.h"
#include "common/func.h"
#include "common/config_item.h"
#include "common/directory_op.h"
#include "new_client/fsname.h"

namespace tfs
{
  namespace dataserver
  {
    using namespace std;
    using namespace tfs::common;
    using namespace tfs::client;
    using namespace tfs::message;
    DataService::DataService():
        data_manager_(*this),
        data_helper_(*this),
        task_manager_(*this),
        block_manager_(NULL),
        data_management_(*this),
        heart_manager_(NULL),
        client_request_server_(*this),
        timeout_thread_(0),
        task_thread_(0)
    {

    }

    DataService::~DataService()
    {
      vector<SyncBase*>::iterator iter = sync_mirror_.begin();
      for (; iter != sync_mirror_.end(); ++iter)
        tbsys::gDelete((*iter));
      tbsys::gDelete(block_manager_);
      tbsys::gDelete(heart_manager_);
      timeout_thread_ = 0;
      task_thread_ = 0;
    }

    int DataService::parse_common_line_args(int argc, char* argv[], std::string& errmsg)
    {
      char buf[256] = {'\0'};
      int32_t index = 0;
      while ((index = getopt(argc, argv, "i:")) != EOF)
      {
        switch (index)
        {
          case 'i':
            server_index_ = optarg;
            break;
          default:
            snprintf(buf, 256, "%c invalid parameter", index);
            break;
        }
      }
      int32_t ret = server_index_.empty() ? TFS_ERROR : TFS_SUCCESS;
      if (TFS_SUCCESS != ret)
      {
        snprintf(buf, 256, "server index in empty, invalid parameter");
      }
      errmsg.assign(buf);
      return ret;
    }

    int DataService::get_listen_port() const
    {
      int32_t port = -1;
      int32_t base_port = TBSYS_CONFIG.getInt(CONF_SN_PUBLIC, CONF_PORT, 0);
      if (base_port >= 1024 || base_port <= 65535)
      {
        port = SYSPARAM_DATASERVER.get_real_ds_port(base_port, server_index_);
        if (port < 1024 || base_port > 65535)
        {
          port = -1;
        }
      }
      return port;
    }

    const char* DataService::get_log_file_path()
    {
      const char* log_file_path = NULL;
      const char* work_dir = get_work_dir();
      if (work_dir != NULL)
      {
        log_file_path_ = work_dir;
        log_file_path_ += "/logs/dataserver";
        log_file_path_ = SYSPARAM_DATASERVER.get_real_file_name(log_file_path_, server_index_, "log");
        log_file_path = log_file_path_.c_str();
      }
      return log_file_path;
    }

    const char* DataService::get_pid_file_path()
    {
      const char* pid_file_path = NULL;
      const char* work_dir = get_work_dir();
      if (work_dir != NULL)
      {
        pid_file_path_ = work_dir;
        pid_file_path_ += "/logs/dataserver";
        pid_file_path_ = SYSPARAM_DATASERVER.get_real_file_name(pid_file_path_, server_index_, "pid");
        pid_file_path = pid_file_path_.c_str();
      }
      return pid_file_path;
    }

    string DataService::get_real_work_dir()
    {
      const char* work_dir = get_work_dir();
      string real_work_dir = "";
      if (NULL != work_dir)
      {
        real_work_dir = string(work_dir) + "/dataserver_" + server_index_;
      }
      return real_work_dir;
    }

    int DataService::initialize(int argc, char* argv[])
    {
      UNUSED(argc);
      int32_t ret = SYSPARAM_DATASERVER.initialize(config_file_, server_index_);
      if (TFS_SUCCESS != ret)
      {
        TBSYS_LOG(ERROR, "load dataserver parameter failed: %d", ret);
        ret = EXIT_GENERAL_ERROR;
      }

      if (TFS_SUCCESS == ret)
      {
        //create work directory
        string work_dir = get_real_work_dir();
        ret = work_dir.empty() ? EXIT_GENERAL_ERROR: TFS_SUCCESS;
        if (TFS_SUCCESS != ret)
        {
          TBSYS_LOG(ERROR, "%s not set work dir, must be exist", argv[0]);
        }
        if (TFS_SUCCESS == ret)
        {
          string storage_dir = work_dir + string("/storage");
          if (!DirectoryOp::create_full_path(storage_dir.c_str()))
          {
            ret = EXIT_GENERAL_ERROR;
            TBSYS_LOG(ERROR, "create directory %s error: %s", storage_dir.c_str(), strerror(errno));
          }
          if ( TFS_SUCCESS == ret)
          {
            storage_dir = work_dir + string("/tmp");
            if (!DirectoryOp::create_full_path(storage_dir.c_str()))
            {
              ret = EXIT_GENERAL_ERROR;
              TBSYS_LOG(ERROR, "create directory %s error: %s", storage_dir.c_str(), strerror(errno));
            }
          }
          if (TFS_SUCCESS == ret)
          {
            storage_dir = work_dir + string("/mirror");
            if (!DirectoryOp::create_full_path(storage_dir.c_str()))
            {
              ret = EXIT_GENERAL_ERROR;
              TBSYS_LOG(ERROR, "create directory %s error: %s", storage_dir.c_str(), strerror(errno));
            }
          }
        }
      }

      //set name server ip
      std::vector<uint64_t> ns_ip_port;
      if (TFS_SUCCESS == ret)
      {
        ret = initialize_nameserver_ip_addr_(ns_ip_port);
      }

      //check dev & ip
      if (TFS_SUCCESS == ret)
      {
        const char* ip_addr = get_ip_addr();
        if (NULL == ip_addr)//get ip addr
        {
          ret =  EXIT_CONFIG_ERROR;
          TBSYS_LOG(ERROR, "%s", "dataserver not set ip_addr");
        }

        if (TFS_SUCCESS == ret)
        {
          const char *dev_name = get_dev();
          if (NULL == dev_name)//get dev name
          {
            ret =  EXIT_CONFIG_ERROR;
            TBSYS_LOG(ERROR, "%s","dataserver not set dev_name");
          }
          else
          {
            uint32_t ip_addr_id = tbsys::CNetUtil::getAddr(ip_addr);
            uint32_t local_ip   = Func::get_local_addr(dev_name);
            if (local_ip != ip_addr_id)
            {
              TBSYS_LOG(WARN, "ip '%s' is not local ip, local ip: %s",ip_addr, tbsys::CNetUtil::addrToString(local_ip).c_str());
              ret = EXIT_CONFIG_ERROR;
            }
          }
        }
      }

      if (TFS_SUCCESS == ret)
      {
        //init file number to management
        IpAddr adr;
        adr.ip_ = tbsys::CNetUtil::getAddr(get_ip_addr());
        adr.port_ = get_listen_port();
        uint64_t file_number = ((adr.ip_ & 0xFFFFFF00) | (adr.port_ & 0xFF));
        file_number = file_number << 32;
        data_management_.set_file_number(file_number);
        //init block manager
        string sb_path = string(SYSPARAM_FILESYSPARAM.mount_name_) + SUPERBLOCK_NAME;
        block_manager_ = new (std::nothrow) BlockManager(sb_path);
        assert(NULL != block_manager_);
      }

      //load all blocks
      if (TFS_SUCCESS == ret)
      {
        ret = block_manager().bootstrap(SYSPARAM_FILESYSPARAM);
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(ERROR, "load all blocks failed, ret: %d", ret);
      }

      //init gcobject manager
      if (TFS_SUCCESS == ret)
      {
        ret = GCObjectManager::instance().initialize(get_timer());
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(ERROR, "%s", "initialize gcobject manager fail");
      }

      // sync mirror should init after bootstrap
      if (TFS_SUCCESS == ret)
      {
        ret = initialize_sync_mirror_();
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(ERROR, "dataservice::start, init sync mirror fail!, ret: %d", ret);
      }
      // init heartbeat
      if (TFS_SUCCESS == ret)
      {
        heart_manager_    = new (std::nothrow)DataServerHeartManager(*this, ns_ip_port);
        assert(NULL != heart_manager_);
        task_thread_      = new (std::nothrow)RunTaskThreadHelper(*this);
        assert(0 != task_thread_);
        timeout_thread_  = new (std::nothrow)TimeoutThreadHelper(*this);
        assert(0 != timeout_thread_);
      }
      return ret;
    }

    int DataService::initialize_sync_mirror_()
    {
      int32_t ret = (!SYSPARAM_DATASERVER.local_ns_ip_.empty()
                    && !SYSPARAM_DATASERVER.slave_ns_ip_.empty()
                    &&  SYSPARAM_DATASERVER.local_ns_port_ > 1024
                    &&  SYSPARAM_DATASERVER.local_ns_port_ < 65535) ? TFS_SUCCESS : EXIT_SYSTEM_PARAMETER_ERROR;
      if (TFS_SUCCESS == ret)
      {
        char src_addr[common::MAX_SYNC_IPADDR_LENGTH] = {'\0'};
        char dest_addr[common::MAX_SYNC_IPADDR_LENGTH] = {'\0'};
        snprintf(src_addr, MAX_SYNC_IPADDR_LENGTH, "%s:%d",
            SYSPARAM_DATASERVER.local_ns_ip_.c_str(), SYSPARAM_DATASERVER.local_ns_port_);
        std::vector<std::string> slave_ns_ip;
        common::Func::split_string(SYSPARAM_DATASERVER.slave_ns_ip_.c_str(), '|', slave_ns_ip);
        std::vector<std::string>::iterator iter = slave_ns_ip.begin();
        for (int32_t index = 0; iter != slave_ns_ip.end() && TFS_SUCCESS == ret; ++iter, index++)
        {
          snprintf(dest_addr, MAX_SYNC_IPADDR_LENGTH, "%s", (*iter).c_str());
          SyncBase* sync_base = new (std::nothrow)SyncBase(*this, SYNC_TO_TFS_MIRROR, index, src_addr, dest_addr);
          assert(NULL != sync_base);
          ret = sync_base->init();
          if (TFS_SUCCESS != ret)
            TBSYS_LOG(ERROR, "initialize sync mirror failed, local cluster ns ip addr: %s, remote cluster ns ip addr: %s",src_addr, dest_addr);
          else
            sync_mirror_.push_back(sync_base);
        }
      }
      return ret;
    }

    int DataService::initialize_nameserver_ip_addr_(std::vector<uint64_t>& ns_ip_port)
    {
      IpAddr* addr = NULL;
      int32_t ret = (SYSPARAM_DATASERVER.local_ns_ip_.empty()) ? EXIT_SYSTEM_PARAMETER_ERROR : TFS_SUCCESS;
      if (TFS_SUCCESS == ret)
      {

        DsRuntimeGlobalInformation& instance = DsRuntimeGlobalInformation::instance();
        addr = reinterpret_cast<IpAddr*> (&instance.ns_vip_port_);
        addr->ip_ =  Func::get_addr(SYSPARAM_DATASERVER.local_ns_ip_.c_str());
        addr->port_ =  SYSPARAM_DATASERVER.local_ns_port_;
        ret = (0 == addr->ip_) ? EXIT_SYSTEM_PARAMETER_ERROR : TFS_SUCCESS;
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(ERROR, "nameserver ip addrr is invalid, %s", SYSPARAM_DATASERVER.local_ns_ip_.c_str());
      }

      if (TFS_SUCCESS == ret)
      {
        ret = SYSPARAM_DATASERVER.ns_addr_list_.empty() ? EXIT_SYSTEM_PARAMETER_ERROR: TFS_SUCCESS;
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(ERROR, "nameserver real ip addrr is invalid, %s", SYSPARAM_DATASERVER.ns_addr_list_.c_str());
      }

      if (TFS_SUCCESS == ret)
      {
        const int32_t BUF_LEN = 512;
        char buffer[BUF_LEN] = {'\0'};
        strncpy(buffer, SYSPARAM_DATASERVER.ns_addr_list_.c_str(), BUF_LEN);
        char* t = NULL, *s = buffer;
        while (NULL != (t = strsep(&s, "|")))
        {
          uint32_t ip = Func::get_addr(t);
          ret = (0 == ip) ? EXIT_SYSTEM_PARAMETER_ERROR : TFS_SUCCESS;
          if (TFS_SUCCESS != ret)
            TBSYS_LOG(ERROR, "nameserver real ip addrr: %s is invalid...", t);
          else
            ns_ip_port.push_back(Func::str_to_addr(t, addr->port_));
        }
      }

      if (TFS_SUCCESS == ret)
      {
        int32_t size = ns_ip_port.size();
        ret = ns_ip_port.empty() || size > MAX_SINGLE_CLUSTER_NS_NUM ? EXIT_SYSTEM_PARAMETER_ERROR : TFS_SUCCESS;
        if (TFS_SUCCESS != ret)
          TBSYS_LOG(WARN, "must have one ns, check your ns' list, %s",  SYSPARAM_DATASERVER.ns_addr_list_.c_str());
      }
      return ret;
    }

    int DataService::destroy_service()
    {
      DsRuntimeGlobalInformation::instance().destroy();

      vector<SyncBase*>::iterator iter = sync_mirror_.begin();
      for (; iter != sync_mirror_.end(); iter++)
        (*iter)->stop();

      if (NULL != heart_manager_)
         heart_manager_->wait_for_shut_down();

      if (0 != task_thread_)
        task_thread_->join();

      if (0 != timeout_thread_)
        timeout_thread_->join();

      GCObjectManager::instance().destroy();
      GCObjectManager::instance().wait_for_shut_down();
      return TFS_SUCCESS;
    }

    void DataService::rotate_(time_t& last_rotate_log_time, time_t now, time_t zonesec)
    {
      if ((now % 86400 >= zonesec)
          && (now % 86400 < zonesec + 300)
          && (last_rotate_log_time < now - 600))
      {
        last_rotate_log_time = now;
        TBSYS_LOGGER.rotateLog(NULL);
      }
    }

    void DataService::timeout_()
    {
      tzset();
      const int32_t MAX_SLEEP_TIME_US = 1000 * 1000;//1s
      time_t zonesec = 86400 + timezone, now = 0, last_rotate_log_time = 0;
      while (!DsRuntimeGlobalInformation::instance().is_destroyed())
      {
        now = time(NULL);
        //rotate log
        rotate_(last_rotate_log_time, now, zonesec);

        //check datafile
        data_management_.gc_data_file();
        data_manager_.timeout(Func::get_monotonic_time());
        if (NULL != block_manager_)
          block_manager_->timeout(Func::get_monotonic_time());
        task_manager_.expire_task();
        usleep(MAX_SLEEP_TIME_US);
      }
    }

    void DataService::run_task_()
    {
      task_manager_.run_task();
    }

    bool DataService::check_response(common::NewClient* client)
    {
      bool all_success = (NULL != client);
      NewClient::RESPONSE_MSG_MAP* sresponse = NULL;
      NewClient::RESPONSE_MSG_MAP* fresponse = NULL;
      if (all_success)
      {
        sresponse = client->get_success_response();
        fresponse = client->get_fail_response();
        all_success = ((NULL != sresponse) && (NULL != fresponse));
      }

      if (all_success)
      {
        all_success = (sresponse->size() == client->get_send_id_sign().size());
        NewClient::RESPONSE_MSG_MAP::iterator iter = sresponse->begin();
        for ( ; all_success && (iter != sresponse->end()); iter++)
        {
          tbnet::Packet* rmsg = iter->second.second;
          all_success = (NULL != rmsg);
          if (all_success)
          {
            all_success = (STATUS_MESSAGE == rmsg->getPCode());
          }
          if (all_success)
          {
            StatusMessage* smsg = dynamic_cast<StatusMessage*>(rmsg);
            all_success = STATUS_MESSAGE_OK == smsg->get_status();
          }
        }
      }

      return all_success;
    }

    int DataService::callback(common::NewClient* client)
    {
      int32_t ret = NULL != client ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        tbnet::Packet* packet = client->get_source_msg();
        ret = (NULL != packet)? TFS_SUCCESS: TFS_ERROR;
        if (TFS_SUCCESS == ret)
        {
          bool all_success = check_response(client);
          int32_t pcode = packet->getPCode();
          common::BasePacket* bpacket= dynamic_cast<BasePacket*>(packet);
          if (WRITE_DATA_MESSAGE == pcode)
          {
            WriteDataMessage* message = dynamic_cast<WriteDataMessage*>(bpacket);
            WriteDataInfo write_info = message->get_write_info();
            int32_t lease_id = message->get_lease_id();
            int32_t version = message->get_block_version();
            if (!all_success)
            {
              TBSYS_LOG(ERROR,
                  "write data fail. chid: %d, blockid: %u, fileid: %" PRI64_PREFIX "u, number: %" PRI64_PREFIX "u\n",
                  message->getChannelId(), message->get_block_id(), message->get_file_id(), message->get_file_number());
              data_management_.erase_data_file(message->get_file_number());
              traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, TFS_ERROR, 0 == write_info.offset_,write_info.length_);
            }
            else
            {
              TBSYS_LOG(
                  INFO,
                  "write data success. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, version: %u, leaseid: %u, role: master",
                  write_info.file_number_, write_info.block_id_, write_info.file_id_, version, lease_id);
            }
            StatusMessage* reply_msg =  all_success ?
              new StatusMessage(STATUS_MESSAGE_OK, "write data complete"):
              new StatusMessage(STATUS_MESSAGE_ERROR, "write data fail");
            ret = bpacket->reply(reply_msg);
            if (TFS_SUCCESS != ret)
            {
              traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, 0 == write_info.offset_,write_info.length_);
              TBSYS_LOG(
                  ERROR,
                  "write data fail. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, version: %u, leaseid: %u, role: master",
                  write_info.file_number_, write_info.block_id_, write_info.file_id_, version, lease_id);
            }
          }
          else if (RENAME_FILE_MESSAGE == pcode)
          {
            RenameFileMessage* message = dynamic_cast<RenameFileMessage*> (bpacket);
            uint32_t block_id = message->get_block_id();
            uint64_t file_id = message->get_file_id();
            uint64_t new_file_id = message->get_new_file_id();
            int32_t option_flag = message->get_option_flag();
            if (all_success && 0 == (option_flag & TFS_FILE_NO_SYNC_LOG))
            {
              if (sync_mirror_.size() > 0)
              {
                for (uint32_t i = 0; i < sync_mirror_.size(); i++)
                {
                  // ignore return value, just print error log for rename
                  int tmp_ret = sync_mirror_.at(i)->write_sync_log(OPLOG_RENAME, block_id, new_file_id, file_id);
                  if (TFS_SUCCESS != tmp_ret)
                  {
                    TBSYS_LOG(ERROR, " write sync log fail (id:%d), blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                      i, block_id, file_id, tmp_ret);
                  }
                }
              }
            }
            else if (!all_success)
            {
              TBSYS_LOG(
                  ERROR,
                  "RenameFileMessage fail. chid: %d, blockid: %u, fileid: %" PRI64_PREFIX "u, new fileid: %" PRI64_PREFIX "u\n",
                  message->getChannelId(), block_id, file_id, new_file_id);
            }
            StatusMessage* reply_msg =  all_success ?
              new StatusMessage(STATUS_MESSAGE_OK, "rename complete"):
              new StatusMessage(STATUS_MESSAGE_ERROR, "rename fail");
            bpacket->reply(reply_msg);
          }
          else if (UNLINK_FILE_MESSAGE == pcode)
          {
            // do nothing. unlink don't care other ds response
            UnlinkFileMessage* message = dynamic_cast<UnlinkFileMessage*> (bpacket);
            if (!all_success)
            {
              TBSYS_LOG(WARN, "UnlinkFileMessage fail. chid:%d, blockid: %u, fileid: %" PRI64_PREFIX "u\n",
                  message->getChannelId(), message->get_block_id(), message->get_file_id());
            }
          }
          else if (CLOSE_FILE_MESSAGE == pcode)
          {
            CloseFileMessage* message = dynamic_cast<CloseFileMessage*> (bpacket);
            CloseFileInfo close_file_info = message->get_close_file_info();
            int32_t lease_id = message->get_lease_id();
            uint64_t peer_id = message->get_connection()->getPeerId();

            //commit
            int32_t status = all_success ? TFS_SUCCESS : TFS_ERROR;
            int32_t ret = ds_requester_.req_block_write_complete(close_file_info.block_id_, lease_id, status);
            if (TFS_SUCCESS == status)
            {
              if (TFS_SUCCESS == ret)
              {
                //sync to mirror
                if (sync_mirror_.size() > 0)
                {
                  int option_flag = message->get_option_flag();
                  if (0 == (option_flag & TFS_FILE_NO_SYNC_LOG))
                  {
                    TBSYS_LOG(INFO, " write sync log, blockid: %u, fileid: %" PRI64_PREFIX "u", close_file_info.block_id_,
                        close_file_info.file_id_);
                    for (uint32_t i = 0; i < sync_mirror_.size(); i++)
                    {
                      ret = sync_mirror_.at(i)->write_sync_log(OPLOG_INSERT, close_file_info.block_id_,
                          close_file_info.file_id_);
                      if (TFS_SUCCESS != ret)
                      {
                        TBSYS_LOG(ERROR, " write sync log fail (id:%d), blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                            i, close_file_info.block_id_, close_file_info.file_id_, ret);
                        break;
                      }
                    }
                  }
                }
              }
              if (TFS_SUCCESS == ret)
              {
                message->reply(new StatusMessage(STATUS_MESSAGE_OK));
              }
              else
              {
                TBSYS_LOG(ERROR,
                    "rep block write complete or write sync log fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                    close_file_info.block_id_, close_file_info.file_id_, ret);
                message->reply(new StatusMessage(STATUS_MESSAGE_ERROR));
              }
            }
            else
            {
              message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
                  "close write file to other dataserver fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                  close_file_info.block_id_, close_file_info.file_id_, ret);
            }
            TBSYS_LOG(INFO, "close file %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, peerip: %s, role: master",
                TFS_SUCCESS == ret ? "success" : "fail", close_file_info.file_number_, close_file_info.block_id_,
                close_file_info.file_id_, tbsys::CNetUtil::addrToString(peer_id).c_str());
            traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, true, 0);
          }
          else if (WRITE_FILE_MESSAGE_V2 == pcode ||
                   CLOSE_FILE_MESSAGE_V2 == pcode ||
                   UNLINK_FILE_MESSAGE_V2 == pcode)
          {
            ret = client_request_server_.callback(client);
          }
          else
          {
            TBSYS_LOG(ERROR, "callback handle error message pcode: %d", pcode);
          }
        }
      }
      return ret;
    }

    bool DataService::access_deny(BasePacket* message)
    {
      tbnet::Connection* conn = message->get_connection();
      if (!conn)
        return false;
      /*uint64_t peer_id = conn->getPeerId();
      int32_t type = message->getPCode();
      if (type == READ_DATA_MESSAGE || type == READ_DATA_MESSAGE_V2 ||
          type == READ_DATA_MESSAGE_V3)
        return acl_.deny(peer_id, AccessControl::READ);
      if (type == WRITE_DATA_MESSAGE || type == CLOSE_FILE_MESSAGE)
        return acl_.deny(peer_id, AccessControl::WRITE);
      if (type == UNLINK_FILE_MESSAGE)
        return acl_.deny(peer_id, AccessControl::UNLINK);*/
      return false;
    }

    tbnet::IPacketHandler::HPRetCode DataService::handlePacket(tbnet::Connection *connection, tbnet::Packet *packet)
    {
      tbnet::IPacketHandler::HPRetCode hret = tbnet::IPacketHandler::FREE_CHANNEL;
      bool bret = NULL != connection && NULL != packet;
      if (bret)
      {
        TBSYS_LOG(DEBUG, "receive pcode : %d", packet->getPCode());
        if (!packet->isRegularPacket())
        {
          bret = false;
          TBSYS_LOG(WARN, "control packet, pcode: %d", dynamic_cast<tbnet::ControlPacket*>(packet)->getCommand());
        }
        if (bret)
        {
          BasePacket* bpacket = dynamic_cast<BasePacket*>(packet);
          bpacket->set_connection(connection);
          bpacket->setExpireTime(MAX_RESPONSE_TIME);
          bpacket->set_direction(static_cast<DirectionStatus>(bpacket->get_direction()|DIRECTION_RECEIVE));

          if (bpacket->is_enable_dump())
          {
            bpacket->dump();
          }
          // add access control by message type
          if ((!access_deny(bpacket)) && !DsRuntimeGlobalInformation::instance().is_destroyed())
          {
            bret = push(bpacket, false);
            if (bret)
              hret = tbnet::IPacketHandler::KEEP_CHANNEL;
            else
            {
              bpacket->reply_error_packet(TBSYS_LOG_LEVEL(ERROR),STATUS_MESSAGE_ERROR, "%s, task message beyond max queue size, discard", get_ip_addr());
              bpacket->free();
            }
          }
          else
          {
            bpacket->reply_error_packet(TBSYS_LOG_LEVEL(WARN), STATUS_MESSAGE_ACCESS_DENIED,
                "you client %s access been denied. msgtype: %d", tbsys::CNetUtil::addrToString(
                  connection->getPeerId()).c_str(), packet->getPCode());
            // packet denied, must free
            bpacket->free();
          }
        }
      }
      return hret;
    }

    bool DataService::handlePacketQueue(tbnet::Packet* packet, void* args)
    {
      bool bret = BaseService::handlePacketQueue(packet, args);
      if (bret)
      {
        int32_t pcode = packet->getPCode();
        int32_t ret = LOCAL_PACKET == pcode ? TFS_ERROR : TFS_SUCCESS;
        if (TFS_SUCCESS == ret)
        {
          switch (pcode)
          {
            case CREATE_FILENAME_MESSAGE:
              ret = create_file_number(dynamic_cast<CreateFilenameMessage*>(packet));
              break;
            case WRITE_DATA_MESSAGE:
              ret = write_data(dynamic_cast<WriteDataMessage*>(packet));
              break;
            case CLOSE_FILE_MESSAGE:
              ret = close_write_file(dynamic_cast<CloseFileMessage*>(packet));
              break;
            case WRITE_RAW_DATA_MESSAGE:
              ret = write_raw_data(dynamic_cast<WriteRawDataMessage*>(packet));
              break;
            case WRITE_INFO_BATCH_MESSAGE:
              ret = batch_write_info(dynamic_cast<WriteInfoBatchMessage*>(packet));
              break;
            case READ_DATA_MESSAGE_V2:
              ret = read_data_extra(dynamic_cast<ReadDataMessageV2*>(packet), READ_VERSION_2);
              break;
            case READ_DATA_MESSAGE_V3:
              ret = read_data_extra(dynamic_cast<ReadDataMessageV3*>(packet), READ_VERSION_3);
              break;
            case READ_DATA_MESSAGE:
              ret = read_data(dynamic_cast<ReadDataMessage*>(packet));
              break;
            case READ_RAW_DATA_MESSAGE:
              ret = read_raw_data(dynamic_cast<ReadRawDataMessage*>(packet));
              break;
            case FILE_INFO_MESSAGE:
              ret = read_file_info(dynamic_cast<FileInfoMessage*>(packet));
              break;
            case UNLINK_FILE_MESSAGE:
              ret = unlink_file(dynamic_cast<UnlinkFileMessage*>(packet));
              break;
            case RENAME_FILE_MESSAGE:
              ret = rename_file(dynamic_cast<RenameFileMessage*>(packet));
              break;
            case NEW_BLOCK_MESSAGE:
              ret = new_block(dynamic_cast<NewBlockMessage*>(packet));
              break;
            case REMOVE_BLOCK_MESSAGE:
              ret = remove_block(dynamic_cast<RemoveBlockMessage*>(packet));
              break;
            case LIST_BLOCK_MESSAGE:
              ret = list_blocks(dynamic_cast<ListBlockMessage*>(packet));
              break;
           // TODO
           // case REPLICATE_BLOCK_MESSAGE:
           //   ret = replicate_block_cmd(dynamic_cast<ReplicateBlockMessage*>(packet));
           //  break;
           // case COMPACT_BLOCK_MESSAGE:
           //   ret = compact_block_cmd(dynamic_cast<CompactBlockMessage*>(packet));
           //   break;
            case GET_BLOCK_INFO_MESSAGE:
              ret = get_block_info(dynamic_cast<GetBlockInfoMessage*>(packet));
              break;
            case GET_SERVER_STATUS_MESSAGE:
              ret = get_server_status(dynamic_cast<GetServerStatusMessage*>(packet));
              break;
            case RELOAD_CONFIG_MESSAGE:
              break;
            case STATUS_MESSAGE:
              ret = get_ping_status(dynamic_cast<StatusMessage*>(packet));
              break;
            case CLIENT_CMD_MESSAGE:
              ret = client_command(dynamic_cast<ClientCmdMessage*>(packet));
              break;
            case GET_DATASERVER_INFORMATION_MESSAGE:
              ret = get_dataserver_information(dynamic_cast<BasePacket*>(packet));
              break;
            case REQ_CHECK_BLOCK_MESSAGE:
              ret = check_blocks(dynamic_cast<BasePacket*>(packet));
              break;
            case REQ_CALL_DS_REPORT_BLOCK_MESSAGE:
            case STAT_FILE_MESSAGE_V2:
            case READ_FILE_MESSAGE_V2:
            case WRITE_FILE_MESSAGE_V2:
            case CLOSE_FILE_MESSAGE_V2:
            case UNLINK_FILE_MESSAGE_V2:
            case NEW_BLOCK_MESSAGE_V2:
            case REMOVE_BLOCK_MESSAGE_V2:
              ret = client_request_server_.handle(packet);
              break;
            default:
              TBSYS_LOG(ERROR, "process packet pcode: %d\n", pcode);
              ret = TFS_ERROR;
              break;
          }
          if (common::TFS_SUCCESS != ret)
          {
            common::BasePacket* msg = dynamic_cast<common::BasePacket*>(packet);
            msg->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret, "execute message failed");
          }
        }
      }
      return bret;
    }

    int DataService::create_file_number(CreateFilenameMessage* message)
    {
      TIMER_START();
      uint32_t block_id = message->get_block_id();
      uint64_t file_id = message->get_file_id();

      TBSYS_LOG(DEBUG, "create file: blockid: %u, fileid: %" PRI64_PREFIX "u", block_id, file_id);
      uint64_t file_number = 0;
      int ret = data_management_.create_file(block_id, file_id, file_number);
      if (TFS_SUCCESS != ret)
      {
        if (EXIT_NO_LOGICBLOCK_ERROR == ret) //need to update BlockInfo
        {
          TBSYS_LOG(ERROR, "create file: blockid: %u is lost. ask master to update.", block_id);
          if (TFS_SUCCESS != ds_requester_.req_update_block_info(block_id, UPDATE_BLOCK_MISSING))
          {
            TBSYS_LOG(ERROR, "create file: blockid: %u is null. req update BlockInfo failed", block_id);
          }
        }
        message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "create file failed. blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d.", block_id, file_id, ret);
      }
      else
      {
        RespCreateFilenameMessage* resp_cfn_msg = new RespCreateFilenameMessage();
        resp_cfn_msg->set_block_id(block_id);
        resp_cfn_msg->set_file_id(file_id);
        resp_cfn_msg->set_file_number(file_number);
        message->reply(resp_cfn_msg);
      }

      TIMER_END();
      TBSYS_LOG(INFO,
          "create file %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %"PRI64_PREFIX"u, cost: %"PRI64_PREFIX"d",
          TFS_SUCCESS == ret ? "success" : "fail", file_number, block_id, file_id, TIMER_DURATION());

      if (TFS_SUCCESS != ret)
      {
        traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, true, 0);
      }
      return TFS_SUCCESS;
    }

    int DataService::write_data(WriteDataMessage* message)
    {
      TIMER_START();
      WriteDataInfo write_info = message->get_write_info();
      int32_t lease_id = message->get_lease_id();
      int32_t version = message->get_block_version();
      const char* msg_data = message->get_data();

      TBSYS_LOG(
          DEBUG,
          "write data start, blockid: %u, fileid: %" PRI64_PREFIX "u, filenumber: %" PRI64_PREFIX "u, version: %u, leaseid: %u, isserver: %d\n",
          write_info.block_id_, write_info.file_id_, write_info.file_number_, version, lease_id, write_info.is_server_);

      UpdateBlockType repair = UPDATE_BLOCK_NORMAL;
      int ret = data_management_.write_data(write_info, lease_id, version, msg_data, repair);
      if (EXIT_NO_LOGICBLOCK_ERROR == ret)
      {
        message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "write data failed. block is not exist. blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
            write_info.block_id_, write_info.file_id_, ret);
      }
      else if (EXIT_BLOCK_DS_VERSION_ERROR == ret || EXIT_BLOCK_NS_VERSION_ERROR == ret ||
        EXIT_VERSION_CONFLICT_ERROR == ret)
      {
        message->reply_error_packet(
            TBSYS_LOG_LEVEL(ERROR), ret,
            "write data failed. block version error. blockid: %u, fileid: %" PRI64_PREFIX "u, error ret: %d, repair: %d",
            write_info.block_id_, write_info.file_id_, ret, repair);
        if (TFS_SUCCESS != ds_requester_.req_update_block_info(write_info.block_id_, repair))
        {
          TBSYS_LOG(ERROR, "req update block info failed. blockid: %u, repair: %d", write_info.block_id_, repair);
        }
      }
      else if (EXIT_DATAFILE_OVERLOAD == ret || EXIT_DATA_FILE_ERROR == ret)
      {
        if (Master_Server_Role == write_info.is_server_)
        {
          ds_requester_.req_block_write_complete(write_info.block_id_, lease_id, TFS_ERROR);
        }
        message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "write data fail. blockid: %u, fileid: %" PRI64_PREFIX "u. ret: %d", write_info.block_id_,
            write_info.file_id_, ret);
      }
      else
      {
        // if master ds, write data to other slave ds
        // == Write_Master_Server is master
        if (Master_Server_Role == write_info.is_server_)
        {
          message->set_server(Slave_Server_Role);
          message->set_lease_id(lease_id);
          message->set_block_version(version);
          ret = post_message_to_server(message, message->get_ds_list());
          if (0 == ret)
          {
            //no slave
            message->reply(new StatusMessage(STATUS_MESSAGE_OK));
            TIMER_END();
            TBSYS_LOG(
                INFO,
                "write data %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, version: %u, leaseid: %u, role: %s, cost time: %" PRI64_PREFIX "d",
                ret >= 0 ? "success": "fail", write_info.file_number_, write_info.block_id_, write_info.file_id_, version, lease_id, Master_Server_Role == write_info.is_server_ ? "master" : "slave", TIMER_DURATION());

            if (TFS_SUCCESS != ret)
            {
              traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, 0 == write_info.offset_,write_info.length_);
            }
          }
          else if (ret < 0)
          {
            ds_requester_.req_block_write_complete(write_info.block_id_, lease_id, EXIT_SENDMSG_ERROR);
            message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
                "write data fail to other dataserver (send): blockid: %u, fileid: %" PRI64_PREFIX "u, datalen: %d",
                write_info.block_id_, write_info.file_id_, write_info.length_);
            traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, 0 == write_info.offset_,write_info.length_);
          }
        }
        else
        {
          TBSYS_LOG(DEBUG, "====================================");
          message->reply(new StatusMessage(STATUS_MESSAGE_OK));
          TBSYS_LOG(DEBUG, "====================================");
          TIMER_END();
          TBSYS_LOG(
              INFO,
              "write data %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, version: %u, leaseid: %u, role: %s, cost time: %" PRI64_PREFIX "d",
              ret >= 0 ? "success": "fail", write_info.file_number_, write_info.block_id_, write_info.file_id_, version, lease_id, Master_Server_Role == write_info.is_server_ ? "master" : "slave", TIMER_DURATION());

          if (TFS_SUCCESS != ret)
          {
            traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, 0 == write_info.offset_,write_info.length_);
          }
        }
      }
      return TFS_SUCCESS;
    }

    int DataService::close_write_file(CloseFileMessage* message)
    {
      TIMER_START();
      CloseFileInfo close_file_info = message->get_close_file_info();
      int32_t lease_id = message->get_lease_id();
      uint64_t peer_id = message->get_connection()->getPeerId();
      int32_t option_flag = message->get_option_flag();

      TBSYS_LOG(
          DEBUG,
          "close write file, blockid: %u, fileid: %" PRI64_PREFIX "u, filenumber: %" PRI64_PREFIX "u, leaseid: %u, from: %s\n",
          close_file_info.block_id_, close_file_info.file_id_, close_file_info.file_number_, lease_id,
          tbsys::CNetUtil::addrToString(peer_id).c_str());

      int ret = TFS_ERROR;
      if ((option_flag & TFS_FILE_CLOSE_FLAG_WRITE_DATA_FAILED))
      {
        if (CLOSE_FILE_SLAVER != close_file_info.mode_)
        {
          //commit
          ret = ds_requester_.req_block_write_complete(close_file_info.block_id_, lease_id, TFS_ERROR);
          if (TFS_SUCCESS == ret)
          {
            ret = message->reply(new StatusMessage(STATUS_MESSAGE_OK));
          }
          else
          {
            TBSYS_LOG(ERROR, "rep block write complete fail, blockid: %u, fileid: %"PRI64_PREFIX"d, ret: %d",
              close_file_info.block_id_, close_file_info.file_id_, ret);
            ret = message->reply(new StatusMessage(STATUS_MESSAGE_ERROR));
          }
        }
      }
      else
      {
        int32_t write_file_size = 0;
        ret = data_management_.close_write_file(close_file_info, write_file_size);
        if (TFS_SUCCESS != ret)
        {
          if (EXIT_DATAFILE_EXPIRE_ERROR == ret)
          {
            ret = message->reply_error_packet(
                TBSYS_LOG_LEVEL(ERROR), ret,
                "datafile is null(maybe expired). blockid: %u, fileid: %" PRI64_PREFIX "u, filenumber: %" PRI64_PREFIX "u, ret: %d",
                close_file_info.block_id_, close_file_info.file_id_, close_file_info.file_number_, ret);
          }
          else if (EXIT_NO_LOGICBLOCK_ERROR == ret)
          {
            ret = message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
                "close write file failed. block is not exist. blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                close_file_info.block_id_, close_file_info.file_id_, ret);
          }
          else if (TFS_SUCCESS != ret)
          {
            if (CLOSE_FILE_SLAVER != close_file_info.mode_)
            {
              ds_requester_.req_block_write_complete(close_file_info.block_id_, lease_id, ret);
            }
            ret = message->reply_error_packet(
                TBSYS_LOG_LEVEL(ERROR), ret,
                "close write file error. blockid: %u, fileid : %" PRI64_PREFIX "u, filenumber: %" PRI64_PREFIX "u. ret: %d",
                close_file_info.block_id_, close_file_info.file_id_, close_file_info.file_number_, ret);
          }
        }
        else
        {
          BlockInfo blk;
          int32_t visit_count = 0;
          ret = data_management_.get_block_info(close_file_info.block_id_, blk, visit_count);
          if (TFS_SUCCESS != ret)
          {
            ret = message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
                "close write file failed. block is not exist. blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                close_file_info.block_id_, close_file_info.file_id_, ret);
          }
          else
          {
            //if it is master DS. Send to other slave ds
            if (CLOSE_FILE_SLAVER != close_file_info.mode_)
            {
              traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, true, write_file_size);

              message->set_mode(CLOSE_FILE_SLAVER);
              message->set_block(&blk);

              ret = post_message_to_server(message, message->get_ds_list());
              if (ret < 0)
              {
                // other ds failed, release lease
                ds_requester_.req_block_write_complete(close_file_info.block_id_, lease_id, TFS_ERROR);
                ret = message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
                    "close write file to other dataserver fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                    close_file_info.block_id_, close_file_info.file_id_, ret);
              }
              else if (ret == 0)//only self
              {
                //commit
                ret = ds_requester_.req_block_write_complete(close_file_info.block_id_, lease_id, TFS_SUCCESS);
                if (TFS_SUCCESS == ret)
                {
                  //sync to mirror
                  if (sync_mirror_.size() > 0)
                  {
                    if (0 == (option_flag & TFS_FILE_NO_SYNC_LOG))
                    {
                      TBSYS_LOG(INFO, " write sync log, blockid: %u, fileid: %" PRI64_PREFIX "u", close_file_info.block_id_,
                          close_file_info.file_id_);
                      for (uint32_t i = 0; i < sync_mirror_.size(); i++)
                      {
                        ret = sync_mirror_.at(i)->write_sync_log(OPLOG_INSERT, close_file_info.block_id_,
                          close_file_info.file_id_);
                        if (TFS_SUCCESS != ret)
                        {
                          TBSYS_LOG(ERROR, " write sync log fail (id:%d), blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                              i, close_file_info.block_id_, close_file_info.file_id_, ret);
                          break;
                        }
                      }
                    }
                  }
                }

                if (TFS_SUCCESS == ret)
                {
                  ret = message->reply(new StatusMessage(STATUS_MESSAGE_OK));
                }
                else
                {
                  TBSYS_LOG(ERROR,
                      "rep block write complete or write sync log fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                      close_file_info.block_id_, close_file_info.file_id_, ret);
                  ret = message->reply(new StatusMessage(STATUS_MESSAGE_ERROR));
                }
                TIMER_END();
                TBSYS_LOG(INFO, "close file %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, peerip: %s, role: %s, cost time: %" PRI64_PREFIX "d",
                    TFS_SUCCESS == ret ? "success" : "fail", close_file_info.file_number_, close_file_info.block_id_,
                    close_file_info.file_id_, tbsys::CNetUtil::addrToString(peer_id).c_str(),
                    CLOSE_FILE_SLAVER != close_file_info.mode_ ? "master" : "slave", TIMER_DURATION());
                traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, true, write_file_size);
              }
              else
              {
                // post success.
                ret = TFS_SUCCESS;
              }
            }
            else
            {
              //slave will save seqno to prevent from the conflict when this block change to master block
              const BlockInfo* copyblk = message->get_block();
              if (NULL != copyblk)
              {
                blk.seq_no_ = copyblk->seq_no_;
              }
              ret = message->reply(new StatusMessage(STATUS_MESSAGE_OK));
              TIMER_END();
              TBSYS_LOG(INFO, "close file %s. filenumber: %" PRI64_PREFIX "u, blockid: %u, fileid: %" PRI64_PREFIX "u, peerip: %s, role: %s, cost time: %" PRI64_PREFIX "d",
                  TFS_SUCCESS == ret ? "success" : "fail", close_file_info.file_number_, close_file_info.block_id_,
                  close_file_info.file_id_, tbsys::CNetUtil::addrToString(peer_id).c_str(),
                  CLOSE_FILE_SLAVER != close_file_info.mode_ ? "master" : "slave", TIMER_DURATION());
              traffic_control_.rw_stat(RW_STAT_TYPE_WRITE, ret, true, write_file_size);
            }
          }
        }

      }

      return ret;
    }

    int DataService::read_data_extra(ReadDataMessageV2* message, int32_t version)
    {
      int ret = TFS_ERROR;
      TIMER_START();
      RespReadDataMessageV2* resp_rd_v2_msg = NULL;
      if (READ_VERSION_2 == version)
      {
        resp_rd_v2_msg = new RespReadDataMessageV2();
      }
      else if (READ_VERSION_3 == version)
      {
        resp_rd_v2_msg = new RespReadDataMessageV3();
      }
      else
      {
        TBSYS_LOG(ERROR, "unknown read version type %d", version);
        return TFS_ERROR;
      }

      uint32_t block_id = message->get_block_id();
      uint64_t file_id = message->get_file_id();
      int32_t read_len = message->get_length();
      int32_t read_offset = message->get_offset();
      uint64_t peer_id = message->get_connection()->getPeerId();
      int8_t flag = message->get_flag();

      TBSYS_LOG(DEBUG, "blockid: %u, fileid: %" PRI64_PREFIX "u, read len: %d, read offset: %d, resp: %p", block_id,
                file_id, read_len, read_offset, resp_rd_v2_msg);

      FileInfo file_info;
      int32_t real_read_len = read_len;
      ret = data_management_.read_file_info(block_id, file_id, 0, file_info);
      if (TFS_SUCCESS == ret)
      {
        if (read_offset + read_len > file_info.size_)
        {
          real_read_len = file_info.size_ - read_offset;
        }

        ret = (real_read_len <= 0) ? EXIT_PARAMETER_ERROR: TFS_SUCCESS;
        if (TFS_SUCCESS == ret)
        {
          read_offset += FILEINFO_EXT_SIZE;
          char* packet_data = resp_rd_v2_msg->alloc_data(real_read_len);
          assert(NULL != packet_data);
          ret = data_management_.read_data(block_id, file_id, read_offset, flag, real_read_len, packet_data);
        }
      }

      if (TFS_SUCCESS != ret)
      {
        resp_rd_v2_msg->set_length(ret);
        message->reply(resp_rd_v2_msg);
      }
      else
      {
        if (read_offset == FILEINFO_EXT_SIZE)
        {
          resp_rd_v2_msg->set_file_info(&file_info);
        }
        resp_rd_v2_msg->set_length(real_read_len);
        message->reply(resp_rd_v2_msg);
      }

      TIMER_END();
      TBSYS_LOG(INFO, "read v%d %s. blockid: %u, fileid: %" PRI64_PREFIX "u, read len: %d, read offset: %d, peer ip: %s, cost time: %" PRI64_PREFIX "d",
                version, TFS_SUCCESS == ret ? "success" : "fail",
                block_id, file_id, real_read_len, read_offset,
                tbsys::CNetUtil::addrToString(peer_id).c_str(), TIMER_DURATION());
      traffic_control_.rw_stat(RW_STAT_TYPE_READ, ret, 0 == read_offset, real_read_len);
      return TFS_SUCCESS;
    }

    int DataService::read_data(ReadDataMessage* message)
    {
      int ret = TFS_ERROR;
      TIMER_START();
      RespReadDataMessage* resp_rd_msg = new RespReadDataMessage();
      uint32_t block_id = message->get_block_id();
      uint64_t file_id = message->get_file_id();
      int32_t read_len = message->get_length();
      int32_t read_offset = message->get_offset();
      uint64_t peer_id = message->get_connection()->getPeerId();
      int8_t flag = message->get_flag();

      FileInfo file_info;
      int32_t real_read_len = read_len;
      ret = data_management_.read_file_info(block_id, file_id, 0, file_info);
      if (TFS_SUCCESS == ret)
      {
        if (read_offset + read_len > file_info.size_)
        {
          real_read_len = file_info.size_ - read_offset;
        }

        ret = (real_read_len <= 0) ? EXIT_PARAMETER_ERROR: TFS_SUCCESS;
        if (TFS_SUCCESS == ret)
        {
          read_offset += FILEINFO_EXT_SIZE;
          char* packet_data = resp_rd_msg->alloc_data(real_read_len);
          assert(NULL != packet_data);
          ret = data_management_.read_data(block_id, file_id, read_offset, flag, real_read_len, packet_data);
        }
      }

      if (TFS_SUCCESS != ret)
      {
        resp_rd_msg->set_length(ret);
        message->reply(resp_rd_msg);
      }
      else
      {
        resp_rd_msg->set_length(real_read_len);
        message->reply(resp_rd_msg);
      }

      TIMER_END();
      TBSYS_LOG(INFO, "read %s. blockid: %u, fileid: %" PRI64_PREFIX "u, read len: %d, read offset: %d, peer ip: %s, cost time: %" PRI64_PREFIX "d",
          TFS_SUCCESS == ret ? "success" : "fail", block_id, file_id, real_read_len, read_offset,
          tbsys::CNetUtil::addrToString(peer_id).c_str(), TIMER_DURATION());
      traffic_control_.rw_stat(RW_STAT_TYPE_READ, ret, 0 == read_offset, real_read_len);
      return TFS_SUCCESS;
    }

    int DataService::read_raw_data(ReadRawDataMessage* message)
    {
      RespReadRawDataMessage* resp_rrd_msg = new RespReadRawDataMessage();
      uint32_t block_id = message->get_block_id();
      int32_t read_len = message->get_length();
      int32_t read_offset = message->get_offset();

      TBSYS_LOG(DEBUG, "blockid: %u read data batch, read size: %d, offset: %d", block_id, read_len, read_offset);

      char* tmp_data_buffer = new char[read_len];
      int32_t real_read_len = read_len;
      int ret = data_management_.read_raw_data(block_id, read_offset, real_read_len, tmp_data_buffer);
      if (TFS_SUCCESS != ret)
      {
        tbsys::gDeleteA(tmp_data_buffer);
        resp_rrd_msg->set_length(ret);
        message->reply(resp_rrd_msg);
        return TFS_SUCCESS;
      }

      char* packet_data = resp_rrd_msg->alloc_data(real_read_len);
      if (0 != real_read_len)
      {
        if (NULL == packet_data)
        {
          tbsys::gDelete(resp_rrd_msg);
          tbsys::gDeleteA(tmp_data_buffer);
          TBSYS_LOG(ERROR, "allocdata fail, blockid: %u, realreadlen: %d", block_id, real_read_len);
          return TFS_ERROR;
        }
        else
        {
          memcpy(packet_data, tmp_data_buffer, real_read_len);
        }
      }
      message->set_length(real_read_len);
      message->reply(resp_rrd_msg);
      tbsys::gDeleteA(tmp_data_buffer);

      traffic_control_.rw_stat(RW_STAT_TYPE_READ, ret, read_offset,real_read_len);
      return TFS_SUCCESS;
    }

    int DataService::read_file_info(FileInfoMessage* message)
    {
      TIMER_START();
      uint32_t block_id = message->get_block_id();
      uint64_t file_id = message->get_file_id();
      int32_t mode = message->get_mode();

      TBSYS_LOG(DEBUG, "read file info, blockid: %u, fileid: %" PRI64_PREFIX "u, mode: %d",
          block_id, file_id, mode);
      FileInfo finfo;
      int ret = data_management_.read_file_info(block_id, file_id, mode, finfo);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "readfileinfo fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d", block_id, file_id, ret);
      }

      RespFileInfoMessage* resp_fi_msg = new RespFileInfoMessage();
      resp_fi_msg->set_file_info(&finfo);
      message->reply(resp_fi_msg);
      TIMER_END();
      TBSYS_LOG(DEBUG, "read fileinfo %s. blockid: %u, fileid: %" PRI64_PREFIX "u, mode: %d, cost time: %" PRI64_PREFIX "d",
          TFS_SUCCESS == ret ? "success" : "fail", block_id, file_id, mode, TIMER_DURATION());
      return TFS_SUCCESS;
    }

    int DataService::unlink_file(UnlinkFileMessage* message)
    {
      TIMER_START();
      uint32_t block_id = message->get_block_id();
      uint64_t file_id = message->get_file_id();
      int32_t option_flag = message->get_option_flag();
      int32_t action = message->get_unlink_type();
      uint64_t peer_id = message->get_connection()->getPeerId();
      //is master
      bool is_master = false;
      if ((message->get_server() & 1) == 0)
      {
        is_master = true;
      }

      int64_t file_size = 0;
      int ret = data_management_.unlink_file(block_id, file_id, action, file_size);
      if (TFS_SUCCESS != ret)
      {
        if (EXIT_NO_LOGICBLOCK_ERROR == ret)
        {
          message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
              "block not exist, blockid: %u", block_id);
        }
        else
        {
          message->reply_error_packet(TBSYS_LOG_LEVEL(WARN), ret,
              "file unlink fail, blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d", block_id, file_id, ret);
        }
      }
      else
      {
        if (is_master)
        {
          message->set_server();
          //do not concern the return value of other ds
          post_message_to_server(message, message->get_ds_list());
        }

        char data[128];
        snprintf(data, 128, "%"PRI64_PREFIX"d", file_size);
        message->reply(new StatusMessage(STATUS_MESSAGE_OK, data));

        //sync log
        if (sync_mirror_.size() > 0)
        {
          if (is_master && 0 == (option_flag & TFS_FILE_NO_SYNC_LOG))
          {
            TBSYS_LOG(DEBUG, "master dataserver: delete synclog. blockid: %d, fileid: %" PRI64_PREFIX "u, action: %d\n",
                block_id, file_id, action);
            {
              for (uint32_t i = 0; i < sync_mirror_.size(); i++)
              {
                // ignore return value, just print error log for unlink
                int tmp_ret = sync_mirror_.at(i)->write_sync_log(OPLOG_REMOVE, block_id, file_id, action);
                if (TFS_SUCCESS != tmp_ret)
                {
                  TBSYS_LOG(ERROR, " write sync log fail (id:%d), blockid: %u, fileid: %" PRI64_PREFIX "u, ret: %d",
                      i, block_id, file_id, tmp_ret);
                }
              }
            }
          }
        }
      }

      if (is_master && message->get_lease_id())
      {
        ds_requester_.req_block_write_complete(block_id, message->get_lease_id(), ret, UNLINK_FLAG_YES);
      }

      TIMER_END();
      TBSYS_LOG(INFO, "unlink file %s. blockid: %d, fileid: %" PRI64_PREFIX "u, action: %d, isserver: %s, peer ip: %s, cost time: %" PRI64_PREFIX "d",
          TFS_SUCCESS == ret ? "success" : "fail", block_id, file_id, action, is_master ? "master" : "slave",
          tbsys::CNetUtil::addrToString(peer_id).c_str(), TIMER_DURATION());
      traffic_control_.rw_stat(RW_STAT_TYPE_READ, ret, true,0);
      return TFS_SUCCESS;
    }

    int DataService::new_block(NewBlockMessage* message)
    {
      const VUINT32* new_blocks = message->get_new_blocks();

      int ret = data_management_.batch_new_block(new_blocks);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "newblock error, ret: %d", ret);
      }

      message->reply(new StatusMessage(STATUS_MESSAGE_OK));
      return TFS_SUCCESS;
    }

    int DataService::remove_block(RemoveBlockMessage* message)
    {
      std::vector<uint32_t> remove_blocks;
      remove_blocks.push_back(message->get());
      uint64_t peer_id = message->get_connection()->getPeerId();

      TBSYS_LOG(DEBUG, "remove block. peer id: %s", tbsys::CNetUtil::addrToString(peer_id).c_str());

      int ret = data_management_.batch_remove_block(&remove_blocks);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "removeblock error, ret: %d", ret);
      }

      if (common::REMOVE_BLOCK_RESPONSE_FLAG_YES == message->get_response_flag())
      {
        RemoveBlockResponseMessage* msg = new RemoveBlockResponseMessage();
        msg->set_seqno(message->get_seqno());
        msg->set(message->get());
        message->reply(msg);
      }
      else
      {
        message->reply(new StatusMessage(STATUS_MESSAGE_OK));
      }
      return TFS_SUCCESS;
    }

    int DataService::list_blocks(ListBlockMessage* message)
    {
      int32_t list_type = message->get_block_type();
      VUINT block_ids;
      map <uint32_t, vector<uint32_t> > logic_2_physic_blocks;
      map<uint32_t, BlockInfo*> block_2_info;

      data_management_.query_block_status(list_type, block_ids, logic_2_physic_blocks, block_2_info);

      RespListBlockMessage* resp_lb_msg = new RespListBlockMessage();
      resp_lb_msg->set_blocks(list_type, &block_ids);
      if (list_type & LB_PAIRS)
      {
        resp_lb_msg->set_pairs(list_type, &logic_2_physic_blocks);
      }
      if (list_type & LB_INFOS)
      {
        resp_lb_msg->set_infos(list_type, &block_2_info);
      }

      message->reply(resp_lb_msg);
      return TFS_SUCCESS;
    }

    /**
     * the implement will be changed in the new version
     * so don't include check related code in this version
     */
    int DataService::check_blocks(common::BasePacket* packet)
    {
      UNUSED(packet);
      return EXIT_NOT_SUPPORT_ERROR;
    }

    int DataService::get_block_info(GetBlockInfoMessage *message)
    {
      uint32_t block_id = message->get_block_id();
      BlockInfo blk;
      int32_t visit_count = 0;

      int ret = data_management_.get_block_info(block_id, blk, visit_count);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "block is not exist, blockid: %u, ret: %d", block_id, ret);
      }

      UpdateBlockInfoMessage* resp_ubi_msg = new UpdateBlockInfoMessage();
      resp_ubi_msg->set_block(&blk);
      //serverid has discarded
      resp_ubi_msg->set_server_id(0);
      resp_ubi_msg->set_repair(visit_count);

      message->reply(resp_ubi_msg);
      return TFS_SUCCESS;
    }

    int DataService::get_server_status(GetServerStatusMessage *message)
    {
      int32_t type = message->get_status_type();
      int32_t ret_row = message->get_return_row();

      if (GSS_MAX_VISIT_COUNT == type)
      {
        //get max visit count block
        CarryBlockMessage* resp_cb_msg = new CarryBlockMessage();
        // TODO
        /*
        vector<LogicBlock*> block_vecs;
        data_management_.get_visit_sorted_blockids(block_vecs);
        for (int32_t i = 0; i < ret_row && i < static_cast<int32_t>(block_vecs.size()); ++i)
        {
          resp_cb_msg->add_expire_id(block_vecs[i]->get_block_info()->block_id_);
          resp_cb_msg->add_new_id(block_vecs[i]->get_visit_count());
        }
        */

        message->reply(resp_cb_msg);
        return TFS_SUCCESS;
      }
      else if (GSS_BLOCK_FILE_INFO == type)
      {
        uint32_t block_id = ret_row;
        //get block file list
        vector <FileInfo> fileinfos;
        int ret = data_management_.get_block_file_list(block_id, fileinfos);
        if (TFS_SUCCESS != ret)
        {
          return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
              "GSS_BLOCK_FILE_INFO fail, blockid: %u, ret: %d", block_id, ret);
        }

        BlockFileInfoMessage* resp_bfi_msg = new BlockFileInfoMessage();
        FILE_INFO_LIST* v = resp_bfi_msg->get_fileinfo_list();
        for (uint32_t i = 0; i < fileinfos.size(); ++i)
        {
          v->push_back((fileinfos[i]));
        }
        message->reply(resp_bfi_msg);
        return TFS_SUCCESS;
      }
      else if (GSS_BLOCK_RAW_META_INFO == type)
      {
        //get block inode info
        uint32_t block_id = ret_row;
        RawMetaVec meta_vec;
        int ret = data_management_.get_block_meta_info(block_id, meta_vec);
        if (TFS_SUCCESS != ret)
        {
          return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
              "GSS_BLOCK_RAW_META_INFO fail, blockid: %u, ret: %d", block_id, ret);
        }

        BlockRawMetaMessage* resp_brm_msg = new BlockRawMetaMessage();
        RawMetaVec* v = resp_brm_msg->get_raw_meta_list();
        v->assign(meta_vec.begin(), meta_vec.end());
        message->reply(resp_brm_msg);
        return TFS_SUCCESS;
      }

      return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), STATUS_MESSAGE_ERROR,
          "get server status type unsupport: %d", type);
    }

    int32_t DataService::client_command(ClientCmdMessage* message)
    {
      StatusMessage* resp = new StatusMessage(STATUS_MESSAGE_ERROR, "unknown client cmd.");
      int32_t type = message->get_cmd();
      uint64_t from_server_id = message->get_value2();
      do
      {
        // load a lost block
        if (CLIENT_CMD_SET_PARAM == type)
        {
          uint32_t block_id = message->get_value3();
          uint64_t server_id = message->get_value1();
          TBSYS_LOG(DEBUG, "set run param block_id: %u, server: %" PRI64_PREFIX "u, from: %s", block_id, server_id,
              tbsys::CNetUtil::addrToString(from_server_id).c_str());
        }
      }
      while (0);
      message->reply(resp);
      return TFS_SUCCESS;
    }

    int DataService::write_raw_data(WriteRawDataMessage* message)
    {
      uint32_t block_id = message->get_block_id();
      int32_t msg_len = message->get_length();
      int32_t data_offset = message->get_offset();
      int32_t new_flag = message->get_new_block();
      const char* data_buffer = message->get_data();
      uint64_t peer_id = message->get_connection()->getPeerId();

      TBSYS_LOG(DEBUG, "writeblockdatafile start, blockid: %u, len: %d, offset: %d, new flag: %d, peer id: %s",
          block_id, msg_len, data_offset, new_flag, tbsys::CNetUtil::addrToString(peer_id).c_str());

      int ret = 0;
      if (new_flag)
      {
        ret = data_management_.new_single_block(block_id, true);
        if (TFS_SUCCESS != ret)
        {
          return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
              "write data batch fail, blockid: %u, ret: %d", block_id, ret);
        }

        //add to m_clonedBlockMap TODO
        // repl_block_->add_cloned_block_map(block_id);
      }

      ret = data_management_.write_raw_data(block_id, data_offset, msg_len, data_buffer);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "write data batch fail, blockid: %u, ret: %d", block_id, ret);
      }

      message->reply(new StatusMessage(STATUS_MESSAGE_OK));
      return TFS_SUCCESS;
    }

    int DataService::batch_write_info(WriteInfoBatchMessage* message)
    {
      uint32_t block_id = message->get_block_id();
      const BlockInfo* blk = message->get_block_info();
      const RawMetaVec* raw_metas = message->get_raw_meta_list();
      uint64_t peer_id = message->get_connection()->getPeerId();

      TBSYS_LOG(DEBUG, "write block fileinfo start, blockid: %u, cluster flag: %d, meta size: %d, peer id: %s", block_id,
          message->get_cluster(), static_cast<int32_t>(raw_metas->size()), tbsys::CNetUtil::addrToString(peer_id).c_str());
      int ret = data_management_.batch_write_meta(block_id, blk, raw_metas);
      if (TFS_SUCCESS != ret)
      {
        return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
            "write block fileinfo fail, blockid: %u, ret: %d", block_id, ret);
      }

      //between cluster copy
      if (message->get_cluster())
      {
        ret = ds_requester_.req_update_block_info(block_id);
        if (TFS_SUCCESS != ret)
        {
          return message->reply_error_packet(TBSYS_LOG_LEVEL(ERROR), ret,
              "write block FileInfo fail: update block, blockid: %u, ret: %d", block_id, ret);
        }
      }

      //clear m_clonedBlockMap
      // TODO

      TBSYS_LOG(DEBUG, "write block fileinfo successful, blockid: %u", block_id);
      message->reply(new StatusMessage(STATUS_MESSAGE_OK));
      return TFS_SUCCESS;
    }

    int DataService::get_dataserver_information(common::BasePacket* packet)
    {
      int32_t ret = NULL != packet ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        /*GetDataServerInformationMessage* message = dynamic_cast<GetDataServerInformationMessage*>(packet);
        GetDataServerInformationResponseMessage* reply_msg = new GetDataServerInformationResponseMessage();
        int32_t flag = message->get_flag();
        char* tmp_data_buffer = NULL;
        int32_t bit_map_len = 0;
        int32_t& set_count = reply_msg->get_bit_map_element_count();
        data_management_.query_bit_map(flag, &tmp_data_buffer, bit_map_len, set_count);
        char* data = reply_msg->alloc_data(bit_map_len);
        if (NULL == data)
        {
          tbsys::gDeleteA(tmp_data_buffer);
          tbsys::gDelete(reply_msg);
          TBSYS_LOG(ERROR, "query bitmap. allocate memory fail. type: %d", flag);
          ret = TFS_ERROR;
        }
        else
        {
          TBSYS_LOG(DEBUG, "query bitmap. type: %d, bitmaplen: %u, setcount: %u", flag, bit_map_len, set_count);
          memcpy(data, tmp_data_buffer, bit_map_len);
          tbsys::gDeleteA(tmp_data_buffer);

          SuperBlock block;
          memset(&block, 0, sizeof(block));
          // TODO query super block
          // ret = BlockFileManager::get_instance()->query_super_block(block);
          if (TFS_SUCCESS == ret)
          {
            reply_msg->set_super_block(block);
            reply_msg->set_dataserver_stat_info(data_server_info_);
            ret = packet->reply(reply_msg);
          }
          else
          {
            tbsys::gDelete(reply_msg);
            TBSYS_LOG(ERROR, "query super block information fail. ret: %d", ret);
          }
        }*/
      }
      return ret;
    }


    void DataService::do_stat(const uint64_t peer_id,
        const int32_t visit_file_size, const int32_t real_len, const int32_t offset, const int32_t mode)
    {
      UNUSED(peer_id);
      UNUSED(visit_file_size);
      UNUSED(real_len);
      UNUSED(offset);
      UNUSED(mode);
      /*count_mutex_.lock();
      if (AccessStat::READ_BYTES == mode)
      {
        data_server_info_.total_tp_.read_byte_ += real_len;
        acs_.incr(peer_id, AccessStat::READ_BYTES, real_len);
        if (0 == offset)
        {
          data_server_info_.total_tp_.read_file_count_++;
          visit_stat_.stat_visit_count(visit_file_size);
          acs_.incr(peer_id, AccessStat::READ_COUNT, 1);
        }
      }
      else if (AccessStat::WRITE_BYTES == mode)
      {
        data_server_info_.total_tp_.write_byte_ += visit_file_size;
        data_server_info_.total_tp_.write_file_count_++;
        acs_.incr(peer_id, AccessStat::WRITE_BYTES, visit_file_size);
        acs_.incr(peer_id, AccessStat::WRITE_COUNT, 1);
      }
      else
      {
        data_server_info_.total_tp_.read_byte_ += real_len;
        if (0 == offset)
        {
          data_server_info_.total_tp_.read_file_count_++;
        }
      }
      count_mutex_.unlock();*/
      return;
    }

    void DataService::TimeoutThreadHelper::run()
    {
      service_.timeout_();
    }

    void DataService::RunTaskThreadHelper::run()
    {
      service_.run_task_();
    }

    int ds_async_callback(common::NewClient* client)
    {
      DataService* service = dynamic_cast<DataService*>(BaseMain::instance());
      int32_t ret = NULL != service ? TFS_SUCCESS : TFS_ERROR;
      if (TFS_SUCCESS == ret)
      {
        ret = service->callback(client);
      }
      return ret;
    }
  }/** end namespace dataserver **/
}/** end namespace tfs **/
