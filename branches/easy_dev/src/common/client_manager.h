/*
 * (C) 2007-2010 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id
 *
 * Authors:
 *   duanfei<duanfei@taobao.com>
 *      - initial release
 *
 */

#ifndef TFS_COMMON_CLIENT_MANAGER_H_
#define TFS_COMMON_CLIENT_MANAGER_H_

#define EASY_MULTIPLICITY
#include <tbnet.h>
#include <Mutex.h>
#include <ext/hash_map>
#include "base_packet.h"
#include "base_packet_factory.h"
#include "base_packet_streamer.h"
#include "new_client.h"
#include "internal.h"
#include "easy_helper.h"

namespace tfs
{
  namespace common
  {
    class NewClientManager : public tbnet::IPacketHandler
    {
        friend int NewClient::post_request(const uint64_t server, tbnet::Packet* packet, uint8_t& send_id, const bool clone_source);
        friend bool NewClient::async_wait();
        typedef __gnu_cxx::hash_map<uint32_t, NewClient*> NEWCLIENT_MAP;
        typedef NEWCLIENT_MAP::iterator NEWCLIENT_MAP_ITER;
        typedef int (*async_callback_func_entry)(NewClient* client, void* args);
      public:
        NewClientManager();
        virtual ~NewClientManager();
        static NewClientManager& get_instance()
        {
          static NewClientManager client_manager;
          return client_manager;
        }
        int initialize(BasePacketFactory* factory, BasePacketStreamer* streamer,
                      tbnet::Transport* transport = NULL, async_callback_func_entry entry = NULL,
                      void* args = NULL);
        void destroy();
        bool is_init() const;
        tbnet::IPacketHandler::HPRetCode handlePacket(tbnet::Packet* packet, void* args);
        NewClient* create_client();
        bool destroy_client(NewClient* client);

        tbnet::Packet* clone_packet(tbnet::Packet* message, const int32_t version = TFS_PACKET_VERSION_V2, const bool deserialize = false);

        tbnet::Packet* create_packet(const int32_t pcode);

        static NewClient* malloc_new_client_object(const uint32_t seq_id);
        static void free_new_client_object(NewClient* client);

      // easy support
      public:
        BasePacketStreamer* get_packet_streamer()
        {
          return streamer_;
        }

        static uint64_t get_packet_id_cb(easy_connection_t *c, void *packet)
        {
          NewClientManager& manager = NewClientManager::get_instance();
          return manager.get_packet_streamer()->get_packet_id_handler(c, packet);
        }

        static void* decode_cb(easy_message_t *m)
        {
          NewClientManager& manager = NewClientManager::get_instance();
          return manager.get_packet_streamer()->decode_handler(m);
        }

        static int encode_cb(easy_request_t *r, void *packet)
        {
          NewClientManager& manager = NewClientManager::get_instance();
          return manager.get_packet_streamer()->encode_handler(r, packet);
        }

        static int process_cb(easy_request_t *r)
        {
          NewClientManager& manager = NewClientManager::get_instance();
          return manager.process_handler(r);
        }

        int process_handler(easy_request_t *r);
        int send_packet(uint64_t server_id, tbnet::Packet *packet, void *args, int32_t timeout = 3000)
        {
          return EasyHelper::easy_async_send(&eio_, server_id, packet, args, &eio_handler_, timeout);
        }

      private:
        easy_io_t eio_;
        easy_io_handler_pt eio_handler_;
        int easy_io_initialize();

      private:
        bool handlePacket(const WaitId& id, tbnet::Packet* response);
        bool do_async_callback(NewClient* client);

      private:
        DISALLOW_COPY_AND_ASSIGN(NewClientManager);
        NEWCLIENT_MAP new_clients_;
        BasePacketFactory* factory_;
        BasePacketStreamer* streamer_;
        tbnet::ConnectionManager* connmgr_;
        tbnet::Transport* transport_;

        tbutil::Mutex mutex_;
        async_callback_func_entry async_callback_entry_;
        void* args_;
        static const uint32_t MAX_SEQ_ID = 0xFFFFFF - 1;
        static const int32_t  DEFAULT_CLIENT_CONNTION_QUEUE_LIMIT = 256;
        uint32_t seq_id_;

        bool initialize_;
        bool own_transport_;
    };
  }/* message */
}/* tfs */
#endif
