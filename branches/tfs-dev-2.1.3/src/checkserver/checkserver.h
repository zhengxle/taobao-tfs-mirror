/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: checkserver.h 746 2012-04-13 13:09:59Z linqing.zyd@taobao.com $
 *
 * Authors:
 *   duolong <duolong@taobao.com>
 *      - initial release
 *
 */

#ifndef TFS_CHECKSERVER_CHECKSERVER_H
#define TFS_CHECKSERVER_CHECKSERVER_H

#include "tbsys.h"
#include "TbThread.h"
#include "common/internal.h"
#include "common/base_service.h"
#include "server_helper.h"

#include <iostream>
#include <string>
#include <vector>

namespace tfs
{
  namespace checkserver
  {
    class BlockDiff
    {
      public:

   };

    /**
     * @brief check dataserver thread
     */
    class CheckThread: public tbutil::Thread
    {
      public:

        CheckThread(CheckBlockInfoMap* result_map, tbutil::Mutex* result_map_lock)
        {
          result_map_ = result_map;
          result_map_lock_ = result_map_lock;
        }

        ~CheckThread()
        {

        }

        /**
         * @brief thread loop
         */
        void run();

        /**
         * @brief check all ds i hold
         *
         * @return 0 on success
         */
        int check_ds_list();

        /**
         * @brief add ds to check list
         *
         * @param ds_id: dataserver id
         */
        void add_ds(uint64_t ds_id)
        {
          ds_list_.push_back(ds_id);
        }

        /**
        * @brief set current check time
        *
        * @param check_time
        */
        void set_check_time(const uint32_t check_time)
        {
          check_time_ = check_time;
        }

        /**
        * @brief set last check time
        *
        * @param last_check_time
        */
        void set_last_check_time(const uint32_t last_check_time)
        {
          last_check_time_ = last_check_time;
        }

        /**
         * @brief add check result to map
         *
         * @param result_vec: one ds's result
         */
        void add_result_map(const CheckBlockInfoVec& result_vec);

      private:
        uint32_t check_time_;
        uint32_t last_check_time_;
        VUINT64 ds_list_;                // ds list to check
        CheckBlockInfoMap* result_map_;  // map to store check result
        tbutil::Mutex* result_map_lock_; // lock to modify the map
    };

    typedef tbutil::Handle<CheckThread> CheckThreadPtr;

    /**
     * @brief check server impl
     */
    class CheckServer
    {
      public:
        CheckServer(): master_ns_id_(0), slave_ns_id_(0), thread_count_(0),
          check_interval_(0), last_check_time_(0), meta_fd_(-1),
          master_fp_(NULL), slave_fp_(NULL), stop_(false)
        {

        }

        virtual ~CheckServer()
        {
          if (meta_fd_ >= 0)
          {
            close(meta_fd_);
          }
        }


        /**
         * @brief init check server
         *
         * @param config_file: config file
         *
         * @return
         */
        int init(const char* config_file, const int index);

        /**
         * @brief main execute line
         *
         * @para
         */
        void run_check();

        /**
         * @brief check logic cluster
         *
         * @param cluster_info
         *
         * @return
         */
        int check_logic_cluster();

        /**
         * @brief check one physical result
         *
         * @param ns_id: ns address
         * @param cluster_result: result map
         *
         * @return 0 on success
         */
        int check_physical_cluster(const uint64_t ns_id, const uint32_t check_time,
              CheckBlockInfoMap& cluster_result);


        /**
        * @brief recheck different blocks, compact may happen in master
        *
        * @param recheck_block: block list
        *
        * @return
        */
        int recheck_block(const VUINT& recheck_block);

      private:

        /**
        * @brief load last check time
        *
        *
        * @return
        */
        int init_meta();

        /**
        * @brief save last check time
        *
        * @return
        */
        int update_meta();

        /**
         * @brief init log file
         *
         * @param index
         *
         * @return
         */
        int init_log(const int index);

        /**
         * @brief init pid file
         *
         * @param index
         *
         * @return
         */
        int init_pid(const int index);

        /**
         * @brief open output file
         *
         * @param check_time
         *
         * @return
         */
        int open_file(const uint32_t check_time);

        /**
         * @brief close output file
         *
         * @return
         */
        int close_file();


        /**
         * @brief check blocks in cluster
         *
         * @param ns_id: cluster ns id
         * @param block_result: result map
         */
        void diff_blocks_in_cluster(const uint64_t ns_id, const CheckBlockInfoMap& block_result);

        /**
         * @brief diff logic block in cluster
         *
         * @param master_ns_id: block in which cluter, 0 notes between cluster
         * @param block_result: result to be checked
         */
        void diff_logic_block(const uint64_t master_ns_id, const CheckBlockInfoVec& block_result,
            const bool in_cluter = true, const uint64_t slave_ns_id = 0);

        /**
         * @brief diff block between cluster
         *
         * @param master_ns_id: master ip&port
         * @param master_result: master result
         * @param slave_ns_id: slave ip&port
         * @param slave_result: slave result
         */
        void diff_blocks_between_cluster(
            const uint64_t master_ns_id, CheckBlockInfoMap& master_result,
            const uint64_t slave_ns_id, CheckBlockInfoMap& slave_result,
            common::VUINT& recheck_list);

      private:
        uint64_t master_ns_id_;
        uint64_t slave_ns_id_;
        int thread_count_;
        int check_interval_;
        uint32_t last_check_time_;
        int32_t meta_fd_;
        FILE* master_fp_;
        FILE* slave_fp_;
        bool stop_;

        std::string log_dir_;
        std::string meta_dir_;

      private:
        DISALLOW_COPY_AND_ASSIGN(CheckServer);
    };

  }
}

#endif

