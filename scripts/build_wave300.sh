#########################################################  rflib and wave300  download and compilation
## Linux 4.15.0-130-generic #134-Ubuntu SMP Tue Jan 5 20:46:26 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux

cd ~

for dir in $(ls -dt ~/openwrt/staging_dir/toolchain-mips_24kc_gcc-* | tail -n+2)
do
    mv $dir ${dir/toolchain/renamed_by_build_wave300_toolchain} # rename gcc extra directory conflicts, keep only the newest
done

if ! [ -d wave300_rflib ] 
then
    GIT_SSL_NO_VERIFY=1 git clone https://repo.or.cz/wave300_rflib.git || break;
    # ^ ignores cert error, not recommended
else    
    echo -e '\e[1;31m[build_wave300]\e[0m wave300_rflib directory found, skipping download'
fi

if ! [ -d wave300 ] 
then
    git clone https://github.com/garlett/wave300.git || break;
    echo "dnl Definition of the branch version for configure.ac
m4_define([MTLK_BRANCH_VERSION], [@BRANCH_VERSION@])" > wave300/branch_version.m4.in
else    
    echo -e '\e[1;31m[build_wave300]\e[0m wave300 directory found, skipping download and some config'
    make distclean
fi
cd wave300

sed -i "s/DEFAULT_TOOLCHAIN_PATH=.*/DEFAULT_TOOLCHAIN_PATH=\"\/home\/$(whoami)\/openwrt\"/" support/ugw.env.common
if ! [ -d wireless/driver/rflib ]
then
    ln -s ~/wave300_rflib/wireless/driver/rflib/ wireless/driver/rflib
fi

mkdir -p "tools/mtidl/.deps/" "tools/rtlogger/shared/.deps/" "tools/shared/.deps/"
touch "tools/mtidl/.deps/mtidl_ini_parser.Po" "tools/rtlogger/shared/.deps/logmacro_database.Po" "tools/shared/.deps/Debug.Po" "tools/shared/.deps/CmdLine.Po" "tools/shared/.deps/aux_utils.Po" "tools/shared/.deps/ParamInfo.Po" "tools/shared/.deps/argv_parser.Po" "tools/shared/.deps/mtlk_socket.Po" "tools/shared/.deps/mtlkirbhash.Po" "tools/shared/.deps/mtlkcontainer.Po" "tools/shared/.deps/mtlk_pathutils.Po"

export STAGING_DIR=~/openwrt/staging_dir/

#suleiman
echo 'CONFIG_LINDRV_HW_PCIE=y' >> .config
echo 'CONFIG_USE_INTERRUPT_POLLING=y' >> .config

make menuconfig

start_time="$(date -u +%s)"
make 
if [ $? != 0 ]
then
    echo -e '\e[1;31m[build_wave300]\e[0m Compilation failure, restarting with debug verbose ......'
    x=$( (speaker-test -t sine -f 300 -l 1) & pid=$!; sleep 3s; kill -9 $pid )&
    read xxx
    make -w --trace
    echo -e '\e[1;31m[build_wave300]\e[0m probably you can call the first failed recipe that is not on the openwrt directories ...'
else
    echo -e '\e[1;31m[build_wave300]\e[0m after placing firmware files at /lib/firmware'
    echo -e '\e[1;31m[build_wave300]\e[0m copy and insmod in your router the following files:'
    ls -phlrs ~/wave300/builds/ugw5.4-vrx288/binaries/wls/driver/*.ko
fi

for dir in $(ls -dt ~/openwrt/staging_dir/renamed_by_build_wave300_*)
do
    mv $dir ${dir/renamed_by_build_wave300_/} # restore gcc extra directory name
done


echo -e "\e[1;31m[build_openwrt]\e[0m Elapsed time: \e[1;33m $(($(date -u +%s)-$start_time)) \e[0m seconds."
echo -e '\e[1;31m[build_wave300]\e[0m Hit Crtl+c to stop alarm ...'
while [ 1 ]
do
    x=$( (speaker-test -t sine -f 1250 -l 1) & pid=$!; sleep 0.1s; kill -9 $pid )
    sleep 7s
done

