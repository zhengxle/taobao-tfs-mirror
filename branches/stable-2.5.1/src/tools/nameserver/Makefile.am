AM_CPPFLAGS=-I$(top_srcdir)/src\
      $(TAIR_CACHE_CPPFLAGS)\
	    -I$(TBLIB_ROOT)/include/tbsys\
      -I$(TBLIB_ROOT)/include/tbnet\
	    $(READLINE_INCLUDE)\
			$(TAIR_STORE_CPPFLAGS)

AM_LDFLAGS=-lrt -lpthread -ldl $(READLINE_LIB) -luuid -lz $(TAIR_CACHE_LDFLAGS)

bin_PROGRAMS = admintool tfstool ssm repair_file_tool stat_tool gen_compact_blk_order ns_meta_tool # import_family_info export_family_info

if WITH_TAIR_CACHE
bin_PROGRAMS += remote_block_cache_tool
endif

LDADD = $(top_builddir)/src/tools/util/libtfstoolsutil.a\
	$(top_builddir)/src/clientv2/.libs/libtfsclientv2.a\
	$(top_builddir)/src/nameserver/libnameserver.a\
	$(top_builddir)/src/requester/libtfsrequester.a\
	$(top_builddir)/src/message/libtfsmessage.a\
	$(top_builddir)/src/common/libtfscommon.a\
	$(UNIQUE_STORE_LDFLAGS)\
  $(TAIR_CACHE_LDFLAGS) \
	$(TAIR_STORE_LDFLAGS) \
	$(TBLIB_ROOT)/lib/libtbnet.a\
	$(TBLIB_ROOT)/lib/libtbsys.a\
	-lz

tfstool_SOURCES =  tfstool.cpp

ssm_SOURCES = main.cpp show.cpp show_factory.cpp metacmp.cpp cmp_factory.cpp common.cpp\
							show.h  show_factory.h metacmp.h cmp_factory.h common.h


admintool_SOURCES = admintool.cpp
if WITH_TAIR_CACHE
remote_block_cache_tool_SOURCES=remote_block_cache_tool.cpp
endif

repair_file_tool_SOURCES=repair_file_tool.cpp
stat_tool_SOURCES=stat_main.cpp stat_tool.cpp stat_tool.h common.cpp common.h show_factory.h show_factory.cpp
gen_compact_blk_order_SOURCES = gen_compact_blk_order.cpp common.cpp common.h
#import_family_info_SOURCES = import_family_info.cpp
#export_family_info_SOURCES = export_family_info.cpp
ns_meta_tool_SOURCES=ns_meta_tool.cpp
