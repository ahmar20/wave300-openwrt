################################################## hostapd and wpa_supplicant download and compilation
## Linux 4.15.0-130-generic #134-Ubuntu SMP Tue Jan 5 20:46:26 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux

cd ~

if ! [ -d hostapd-devel-mtlk ]
then
    git clone https://github.com/benjaminlevine/hostapd-devel-mtlk
else    
    echo -e '\e[1;31m[build_hostapd]\e[0m hostapd directory found, skipping download'
fi


export STAGING_DIR=$(find ~/openwrt/staging_dir/ -maxdepth 1 -type d -name 'target*' -print -quit) 

cd hostapd-devel-mtlk
for dir in hostapd wpa_supplicant; do
    cd $dir
    echo -e "\e[1;31m[build_hostapd]\e[0m ..... starting building procedure for:\e[1;33m $dir \e[0m ...."
    if [ -f Makefile.bkp ]
    then
        cp Makefile.bkp Makefile
    else
        cp Makefile Makefile.bkp
    fi

    sed -i "s/benjamin\/router/$(whoami)/" Makefile
    sed -i "s/linux-4.4.14/$(basename $(find ~/openwrt/build_dir/ -type d -name 'linux-[45]*' -print -quit))/" Makefile
    sed -i "s/target-mips_34kc+dsp_musl-1.1.16/$(basename $STAGING_DIR)/" Makefile
    sed -i "s/toolchain-mips_34kc+dsp_gcc-5.3.0_musl-1.1.16/$(basename $( find ~/openwrt/staging_dir/ -maxdepth 1 -type d -name 'toolchain-mips*' -print -quit))/" Makefile
    
    make clean && make
    cd ..
done
cd ..
echo -e '\e[1;31m[build_hostapd]\e[0m done, now copy to your router config.conf and the following two files:'
ls ~/hostapd-devel-mtlk/hostapd/hostapd -phl
ls ~/hostapd-devel-mtlk/wpa_supplicant/wpa_supplicant -phl
echo -e '\e[1;31m[build_hostapd]\e[0m then rn hostapd to mtlk-ap and execute ./mtlk-ap config.conf'


