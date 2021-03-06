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
 *      - initial release
 *
 */
#ifndef TFS_BG_TASK_H_
#define TFS_BG_TASK_H_

#include <string>
#include <Timer.h>
#include "common/statistics.h"
#include "gc_worker.h"

namespace tfs
{
  namespace client
  {
    class BgTask
    {
      public:
        static int initialize(int64_t gc_schedule_interval = 1200);
        static int wait_for_shut_down();
        static int destroy();
        static common::StatManager<std::string, std::string, common::StatEntry>& get_stat_mgr()
        {
          return stat_mgr_;
        }
        static GcManager& get_gc_mgr()
        {
          return gc_mgr_;
        }

      private:
        static tbutil::TimerPtr timer_;
        static common::StatManager<std::string, std::string, common::StatEntry> stat_mgr_;
        static GcManager gc_mgr_;
    };
  }
}

#endif
