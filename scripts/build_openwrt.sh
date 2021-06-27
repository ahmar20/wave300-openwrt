######################################################################################### OpenWrt source compilation for wave300
## Linux 4.15.0-130-generic #134-Ubuntu SMP Tue Jan 5 20:46:26 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux

cd ~

if ! [ -d openwrt ] 
then
    sudo apt install build-essential ccache ecj fastjar file g++ gawk \
    gettext git java-propose-classpath libelf-dev libncurses5-dev \
    libncursesw5-dev libssl-dev python python2.7-dev python3 unzip wget \
    python3-distutils python3-setuptools rsync subversion swig time \
    xsltproc zlib1g-dev flex bison nginx

    git config --global credential.helper store
    
    git clone https://git.openwrt.org/openwrt/openwrt.git
    
    wget https://raw.githubusercontent.com/garlett/wave300/master/scripts/1000-xrx200-pcie-msi-fix.patch #<suleiman>
    
    # config nginx as opkg server, because of possible magic hash mismatch
    # sed -i 'i/  / 
    echo "
    server {
        listen $( ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' ):80;
        location / {
            root /home/$(whoami)/openwrt/bin/;
            autoindex on;
        }
    }" # /etc/nginx/nginx.conf
    sudo nginx -s reload
    
else    
    echo -e '\e[1;31m[build_openwrt]\e[0m openwrt directory found, skiping downloads (apt install, git colone, 1000-xrx200-pcie-msi-fix.patch)'
fi

cd openwrt
echo -e '\e[1;31m[build_openwrt]\e[0m Downloading OpenWrt versions ...'
git fetch --tags
branches=$(git tag -l)
git branch

echo -e '\e[1;31m[build_openwrt]\e[0m 
        Disclaimer: this is provided without warranties, 
        double check everything, use at your own risk !
        
        -> After you choose the branch:
         feeds and patches will be installed and
         menuconfig will open with target=lantiq and subtarget=XRX200.
        -> Optionally select other packages you like and your
         router(target: profile? device?; this will change opkg magic hash,
         preventing installation of kernel packages from official repositories)
        -> Save and exit.'
select branch in $branches
do
    if [ "$branch" = "" ]
    then
        exit
    fi
    
    echo -e '\e[1;31m[build_openwrt]\e[0m cleaning ...'
    # make clean    # deletes contents of the directories /bin and /build_dir. 
    make dirclean # deletes contents of the directories /bin and /build_dir and additionally /staging_dir and /toolchain (=the cross-compile tools), /tmp (e.g data about packages) and /logs. 'Dirclean' is your basic “Full clean” operation.
    # make distclean # nukes everything you have compiled or configured and also deletes all downloaded feeds contents and package sources. 
    
    echo -e '\e[1;31m[build_openwrt]\e[0m git reset ...'
    git reset --hard #
        
    echo -e "\e[1;31m[build_openwrt]\e[0m selecting branch $branch ...."
    git checkout $branch
    git branch

    echo -e '\e[1;31m[build_openwrt]\e[0m updating feeds ....'
    ./scripts/feeds update -a
    ./scripts/feeds install -a
    ./scripts/feeds install libnl
    
    echo -e '\e[1;31m[build_openwrt]\e[0m Downloading OpenWrt default make_menuconfig file for the wave300 routers ...'
    if [[ $branch > "v19.07.0" ]] || [[ $branch == "v19.07.0" ]]
    then
        wget https://downloads.openwrt.org/releases/${branch:1}/targets/lantiq/xrx200/config.buildinfo -O .config
    else
        if [[ $branch < "v18.06.4" ]] || [[ $branch == "v18.06.4" ]] 
        then
            wget https://downloads.openwrt.org/releases/${branch:1}/targets/lantiq/xrx200/config.seed -O .config
        else
            wget https://downloads.openwrt.org/releases/18.06.4/targets/lantiq/xrx200/config.seed -O .config
        fi
    fi



    #<suleiman>
    echo -e "\e[1;31m[build_openwrt]\e[0m Applying patches for the wave300 routers ..."
    for f in ./target/linux/lantiq/patches-*/; 
    do
        ln ~/1000-xrx200-pcie-msi-fix.patch $f
        # TODO: fix makefile section incompatibilites with v21(kernel v5)
        # git add ${f}1000-xrx200-pcie-msi-fix.patch # push to the official repo
    done

    echo -e '\e[1;31m[build_openwrt]\e[0m Appending configs for the wave300 routers ...'
    for f in ./target/linux/lantiq/xrx200/config-*; 
    do
        if ! grep -q 'CONFIG_PCI_MSI=y' $f
        then
            echo 'CONFIG_PCI_MSI=y' >> $f
        fi
        if ! grep -q 'CONFIG_PCIE_LANTIQ_MSI=y' $f
        then
            echo 'CONFIG_PCIE_LANTIQ_MSI=y' >> $f
        fi
    done

    for f in $(find ./target/linux/lantiq/file* -name '*.dts*'); # ./target/linux/lantiq/files*/arch/mips/boot/dts/*;
    do
        if ! grep -q 'pcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        then
            sed -i '/&pci0 {/a\\tpcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        fi
    done
    #</suleiman>
    
    # TODO: remove gcc extra directory conflicts
    
    # TODO: check if user wants to compile for use with  ?kgdb ?over ethernet
    # add package ?name? that forward serial console to tcp ?
    # build gbd with phyton? 
    # disable kernel address space layout randomization -> nokaslr at CONFIG_CMDLINE ?
    # ? append to file: ./target/linux/lantiq/config*
    # CONFIG_FRAME_POINTER=y
    # CONFIG_KGDB=y
    # CONFIG_KGDB_SERIAL_CONSOLE=y
    # CONFIG_DEBUG_INFO=y
    # CONFIG_DEBUG_INFO_DWARF4=y
    
    # TODO: link the 4 firmwares bin files to be added in the system image
    
    # TODO: find where to put and test if the hostapd patch from v2.7 works with current versions

    echo -e "\e[1;31m[build_openwrt]\e[0m Starting make menuconfig ..."
    make menuconfig

    echo -e '\e[1;31m[build_openwrt]\e[0m Starting download, this will take some minutes ...'
    make download

    echo -e '\e[1;31m[build_openwrt]\e[0m Starting compilation(low priority on hd and 4 threads), this may take hours, a sound test may play when it finishes'
    ionice -c 3 nice -n19 make -j4
    speaker-test -t sine -f 250 -l 1 > /dev/null
    
    # TODO: display elapsed time
    echo -e '\e[1;31m[build_openwrt]\e[0m to flash your router, use the following sysupgrade file:'
    ls -phl ./bin/targets/lantiq/xrx200/*.bin

    break
done
