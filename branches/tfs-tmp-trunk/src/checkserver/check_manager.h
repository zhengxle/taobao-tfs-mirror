/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Authors:
 *   linqing <linqing.zyd@taobao.com>
 *      - initial release
 *
 */

#ifndef TFS_CHECKSERVER_CHECKMANAGER_H
#define TFS_CHECKSERVER_CHECKMANAGER_H

#include <iostream>
#include <string>
#include <vector>

#include "common/tfs_vector.h"
#include "common/internal.h"
#include "message/message_factory.h"
#include "tbsys.h"
#include "TbThread.h"
#include "base_server_helper.h"

namespace tfs
{
  namespace checkserver
  {
    enum ServerStatus
    {
      SERVER_STATUS_OK,
      SERVER_STATUS_FAIL
    };

    enum BlockStatus
    {
      BLOCK_STATUS_PENDING,
      BLOCK_STATUS_DONE
    };

    class BlockObject
    {
      public:
        BlockObject():
        block_id_(common::INVALID_BLOCK_ID), server_size_(0), current_(0),
        status_(BLOCK_STATUS_PENDING)
        {
        }

        BlockObject(const uint64_t block_id):
          block_id_(block_id), server_size_(0), current_(0),
          status_(BLOCK_STATUS_PENDING)
        {
        }

        ~BlockObject()
        {
        }

        void reset()
        {
          server_size_ = 0;
          current_ = 0;
        }

        uint64_t get_block_id() const
        {
          return block_id_;
        }

        void set_block_id(const uint64_t block_id)
        {
          block_id_ = block_id;
        }

        BlockStatus get_status()
        {
          return status_;
        }

        void set_status(const BlockStatus status)
        {
          status_ = status;
        }

        void add_server(const uint64_t server_id)
        {
          servers_[server_size_++] = server_id;
        }

        uint64_t next_server()
        {
          current_ %= server_size_;
          return servers_[current_++];
        }

      private:
        uint64_t block_id_;
        uint64_t servers_[common::MAX_REPLICATION_NUM];
        int32_t server_size_;
        int32_t current_;
        BlockStatus status_;
    };

    struct BlockIdCompare
    {
      bool operator () (const BlockObject* lhs, const BlockObject* rhs) const
      {
        assert(NULL != lhs);
        assert(NULL != rhs);
        return lhs->get_block_id() < rhs->get_block_id();
      }
    };

    class ServerObject
    {
      public:
        ServerObject():
          server_id_(common::INVALID_SERVER_ID), status_(SERVER_STATUS_OK)
        {
        }

        ServerObject(const uint64_t server_id):
          server_id_(server_id), status_(SERVER_STATUS_OK)
        {
        }

        ~ServerObject()
        {
        }

        void reset()
        {
          blocks_.clear();
        }

        uint64_t get_server_id() const
        {
          return server_id_;
        }

        void set_server_id(const uint64_t server_id)
        {
          server_id_ = server_id;
        }

        void add_block(const uint64_t block_id)
        {
          blocks_.push_back(block_id);
        }

        const common::VUINT64& get_blocks() const
        {
          return blocks_;
        }

        ServerStatus get_status()
        {
          return status_;
        }

        void set_status(const ServerStatus status)
        {
          status_ = status;
        }

      private:
        uint64_t server_id_;
        common::VUINT64 blocks_;
        ServerStatus status_;
    };

    struct ServerIdCompare
    {
      bool operator ()(const ServerObject* lhs, const ServerObject* rhs) const
      {
        assert(NULL != lhs);
        assert(NULL != rhs);
        return lhs->get_server_id() < rhs->get_server_id();
      }
    };


    typedef common::TfsSortedVector<BlockObject*, BlockIdCompare> BLOCK_MAP;
    typedef common::TfsSortedVector<BlockObject*, BlockIdCompare>::iterator BLOCK_MAP_ITER;
    typedef common::TfsSortedVector<BlockObject*, BlockIdCompare>::iterator BLOCK_MAP_CONST_ITER;
    typedef common::TfsSortedVector<ServerObject*, ServerIdCompare> SERVER_MAP;
    typedef common::TfsSortedVector<ServerObject*, ServerIdCompare>::iterator SERVER_MAP_ITER;
    typedef common::TfsSortedVector<ServerObject*, ServerIdCompare>::iterator SERVER_MAP_CONST_ITER;

    class CheckManager
    {
      public:
        CheckManager(BaseServerHelper* server_helper);
        ~CheckManager();
        void reset();
        void reset_blocks();
        void reset_servers();
        int64_t get_seqno() const;
        const BLOCK_MAP* get_blocks() const;
        const SERVER_MAP* get_servers() const;
        int handle(tbnet::Packet* packet);
        void add_server_to_block(const uint64_t server_id, const uint64_t block_id);
        void add_block_to_server(const uint64_t block_id, const uint64_t server_id);
        int fetch_blocks(const uint64_t ds_id, const common::TimeRange& time_range, common::VUINT64& blocks);
        int dispatch_blocks(const uint64_t ds_id, const int64_t seqno, const common::VUINT64& blocks);
        void run_check();
        void stop_check();

      private:
        int fetch_servers();
        int fetch_blocks(const common::TimeRange& time_range);
        int assign_blocks();
        int dispatch_task();
        int update_task(message::ReportCheckBlockMessage* message);

      private:
        class FetchBlockThread: public tbutil::Thread
        {
          public:
            FetchBlockThread(CheckManager& check_manager, const common::TimeRange& time_range,
                const int32_t thread_count, const int32_t thread_seq):
                check_manager_(check_manager), time_range_(time_range),
                thread_count_(thread_count), thread_seq_(thread_seq)

            {
            }

            ~FetchBlockThread()
            {
            }

            /*
             * Iterator every server
             * send FetchModifiedBlockMessage to DS
             * wait for request, if fail, retry fetch
             */
            void run();

          private:
            DISALLOW_COPY_AND_ASSIGN(FetchBlockThread);
            CheckManager& check_manager_;
            common::TimeRange time_range_;
            int32_t thread_count_;
            int32_t thread_seq_;
        };
        typedef tbutil::Handle<FetchBlockThread> FetchBlockThreadPtr;

        class AssignBlockThread: public tbutil::Thread
        {
          public:
            AssignBlockThread(CheckManager& check_manager,
                const int32_t thread_count, const int32_t thread_seq):
                check_manager_(check_manager), thread_count_(thread_count), thread_seq_(thread_seq)
            {
            }

            ~AssignBlockThread()
            {
            }

            /**
             * dispatch every sever's block to server
             * if fail, retry, fail
             */
            void run();

          private:
            DISALLOW_COPY_AND_ASSIGN(AssignBlockThread);
            CheckManager& check_manager_;
            int32_t thread_count_;
            int32_t thread_seq_;
        };
        typedef tbutil::Handle<AssignBlockThread> AssignBlockThreadPtr;

        class DispatchBlockThread: public tbutil::Thread
        {
          public:
            DispatchBlockThread(CheckManager& check_manager,
                const int32_t thread_count, const int32_t thread_seq):
                check_manager_(check_manager), thread_count_(thread_count), thread_seq_(thread_seq)
            {
            }

            ~DispatchBlockThread()
            {
            }

            /**
             * dispatch every sever's block to server
             * if fail, retry, fail
             */
            void run();

          private:
            DISALLOW_COPY_AND_ASSIGN(DispatchBlockThread);
            CheckManager& check_manager_;
            int32_t thread_count_;
            int32_t thread_seq_;
        };
        typedef tbutil::Handle<DispatchBlockThread> DispatchBlockThreadPtr;

      private:
        DISALLOW_COPY_AND_ASSIGN(CheckManager);
        BLOCK_MAP all_blocks_;  // TODO: change it to an BLOCK_MAP array
        SERVER_MAP all_servers_;
        BaseServerHelper* server_helper_;
        int64_t seqno_;  // every check has an unique seqno, use timestamp(us)
        bool stop_;      // check thread stop flag
    };
  }
}

#endif

