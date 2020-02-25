# $Id$

include $(top_srcdir)/Makefile.am.target_apps

dut_PROGRAMS = dutserver

nodist_dutserver_SOURCES = $(rtlogger_shared_srcdir)/logmacro_database.c

dutserver_SOURCES = dut_host_if.c dut_msg_clbk.c \
                    driver_api.c driver_api.h \
                    dut_utest_nv.c dut_utest_nv.h \
										dutserver.c dutserver.h sockets.c

dutserver_LDADD = @ld_group_start@ \
                $(wireless_shared_builddir)/libmtlkwls.a \
                $(tools_shared_builddir)/linux/libmtlkc.a \
                @ld_group_end@ \
                $(tools_shared_builddir)/3rd_party/iniparser/libiniparser.a \
                -lpthread

AM_CPPFLAGS += -I$(fw_shared_srcdir)/ \
               -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir) \
               -I$(wireless_shared_srcdir) \
               -I$(driver_shared_srcdir)
