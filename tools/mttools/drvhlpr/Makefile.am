# $Id$

include $(top_srcdir)/Makefile.am.target_apps

logger_origin_name = drvhlpr
logger_binary_name = mtlk.o
logger_origin_id = 3

nodist_wls_DATA=app_toolchain_info

mttools_PROGRAMS = drvhlpr

drvhlpr_SOURCES = arphelper.c arphelper.h \
                  fshlpr.c fshlpr.h \
                  drvhlpr.c drvhlpr.h \
                  irbponger.c irbponger.h \
                  asel.c asel.h \
                  ledsctrl.c ledsctrl.h \
                  wpsctrl.c wpsctrl.h

nodist_drvhlpr_SOURCES = app_info.c \
                         $(rtlogger_shared_srcdir)/logmacro_database.c

drvhlpr_LDADD = $(builddir)/../shared/libmttools.a \
                $(tools_shared_builddir)/3rd_party/iniparser/libiniparser.a \
                @ld_group_start@ \
                $(wireless_shared_builddir)/libmtlkwls.a \
                $(tools_shared_builddir)/linux/libmtlkc.a \
                @ld_group_end@ \
                -lpthread -lrt -lm

AM_CPPFLAGS += -g \
               -I$(fw_shared_srcdir)/ \
               -I$(tools_srcdir)/mtidl \
               -I$(srcdir)/../shared/ \
               -I$(srcdir)/../shared/linux/ \
               -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir)/3rd_party/iniparser \
               -I$(tools_shared_srcdir) \
               -I$(wireless_shared_srcdir)/linux \
               -I$(wireless_shared_srcdir)/linux \
               -I$(wireless_shared_srcdir) \
               -I$(driver_shared_srcdir)

# FIXME:
#   The driver helper should not know about EEPROM format.
#   To remove code after CRC generation on EEPROM data originator.
AM_CPPFLAGS += -DEEPROM_CRC_ON_DRVHLPR

#if unit test enabled
#AM_CPPFLAGS += -DRUN_EEPROM_CRC_UTEST
#drvhlpr_SOURCES += fshlpr_utest.c fshlpr_utest.h
#else
EXTRA_DIST = fshlpr_utest.c fshlpr_utest.h
#endif

