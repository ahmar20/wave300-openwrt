######################################################################################### OpenWrt source compilation for wave300
## Linux 4.15.0-130-generic #134-Ubuntu SMP Tue Jan 5 20:46:26 UTC 2021 x86_64 x86_64 x86_64 GNU/Linux

# stdbuf -oL make .... | while read -r line
# do
#     echo "$line" 
#     line=$(echo $line | egrep -o "recipe for target '.*' failed")
#     line=${line:19:-8}
#     if [ -s $line ] && ! [ -f /tmp/frecipe ]; then mkfifo /tmp/frecipe && echo "error code" > /tmp/frecipe & fi
# done
# if [ -f /tmp/frecipe ]  then make `cat /tmp/frecipe` V=Sc else ... fi

# stdbuf -oL make | { 
#     while read -r line
#     do
#         echo "$line"
#         line=$(echo $line | egrep -o "recipe for target '.*' failed")
#         frecipe=$frecipe${line:19:-8}
#     done
#     if [ $frecipe != "" ]
#     then
#         make $frecipe V=Sc
#     else
#         ls 
#         echo ....
#     fi
# }

# exit

export GIT_SSL_NO_VERIFY=1 # ignores git cert error, not recommended

start_time="$(date -u +%s)"
cd ~

if [ -d openwrt ] 
then
    echo -e '\e[1;31m[build_openwrt]\e[0m OpenWrt directory found, skipping downloads (git clone and apt install ....)'
else    
    echo -e '\e[1;31m[build_openwrt]\e[0m Installing required packages ... '

    sudo apt install build-essential ccache ecj fastjar file g++ gawk \
    gettext git java-propose-classpath libelf-dev libncurses5-dev \
    libncursesw5-dev libssl-dev python python2.7-dev python3 unzip wget \
    python3-distutils python3-setuptools rsync subversion swig time \
    xsltproc zlib1g-dev flex bison nginx

    # TODO: config nginx as opkg server, because of magic hash mismatch, check make menuconfig "build packages repositories" option
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
    
    git config --global credential.helper store
    
    echo -e '\e[1;31m[build_openwrt]\e[0m Clonning OpenWrt ... '
    git clone https://git.openwrt.org/openwrt/openwrt.git

    #wget https://raw.githubusercontent.com/garlett/wave300/master/scripts/1000-xrx200-pcie-msi-fix.patch #<suleiman>
fi

if [ -d wave300 ] 
then
    echo -e '\e[1;31m[build_openwrt]\e[0m Wave300 directory found, skipping download'
else    
    echo -e '\e[1;31m[build_openwrt]\e[0m Clonning wave300 (for pci patch files) ... '
    git clone https://github.com/garlett/wave300.git
fi


cd openwrt
echo -e '\e[1;31m[build_openwrt]\e[0m Loading OpenWrt versions ...'
git fetch --tags
branches=$(git tag -l)
git branch

echo -e '\e[1;31m[build_openwrt]\e[0m  Disclaimer:

        This is provided without warranties, double
        check everything, use at your own risk !
        
        -> After you choose the branch:
         feeds and patches will be installed and menuconfig
         will open with target=lantiq and subtarget=XRX200.
         
        -> Optionally select other packages you like and 
        your router model (target devices).
         
        -> Exit and Save.
'
select branch in $branches
do
    if [ "$branch" = "" ]
    then
        exit
    fi

    echo -e '\e[1;31m[build_openwrt]\e[0m What do you like to clean before start?'
    select option in 'Nothing' 'contents of the directories /bin and /build_dir' 'contents of the directories /bin, /build_dir, /staging_dir, /toolchain, /tmp and /logs (recommended)' 'everything compiled, configured, downloaded feeds and package sources'
    do
        if [ "$option" = "" ] || [ "$option" = "Nothing" ]
        then
            break;
        fi
        echo -e "\e[1;31m[build_openwrt]\e[0m Cleanning ($option) ..."
        if [ "$option" = "contents of the directories /bin and /build_dir" ]
        then
            make clean   
        fi
        if [ "$option" = "contents of the directories /bin, /build_dir, /staging_dir, /toolchain, /tmp and /logs (recommended)" ]
        then
            make dirclean
        fi
        if [ "$option" = "everything compiled, configured, downloaded feeds and package sources" ]
        then
            make distclean
        fi
        break;
    done

    
    echo -e '\e[1;31m[build_openwrt]\e[0m Git reset ...'
    git reset --hard #

    echo -e "\e[1;31m[build_openwrt]\e[0m Selecting branch $branch ...."
    git checkout $branch
    git branch
    
    
    echo -e '\e[1;31m[build_openwrt]\e[0m Updating feeds ....'
    ./scripts/feeds update -a || break
    echo -e '\e[1;31m[build_openwrt]\e[0m Installing feeds ....'
    ./scripts/feeds install -a || echo -e '\e[1;31m[build_openwrt]\e[0m Retrying feeds ....' && ( ./scripts/feeds update -a || ./scripts/feeds install -a || break ) # retry, if not work maybe try:  make defconfig;make oldconfig
    ./scripts/feeds install libnl || break
    
    echo -e '\e[1;31m[build_openwrt]\e[0m Downloading OpenWrt default make_menuconfig file for the wave300 routers ...'
    if [[ $branch > "v19.07.0" ]] || [[ $branch == "v19.07.0" ]]
    then
        wget https://downloads.openwrt.org/releases/${branch:1}/targets/lantiq/xrx200/config.buildinfo -O .config || break
    else
        if [[ $branch < "v18.06.4" ]] || [[ $branch == "v18.06.4" ]] 
        then
            wget https://downloads.openwrt.org/releases/${branch:1}/targets/lantiq/xrx200/config.seed -O .config || break
        else
            wget https://downloads.openwrt.org/releases/18.06.4/targets/lantiq/xrx200/config.seed -O .config || break
        fi
    fi



    #<suleiman>
    echo -e "\e[1;31m[build_openwrt]\e[0m Applying patches for the wave300 routers ..."
    for f in ./target/linux/lantiq/patches-4*/;
    do
        rm ${f}1000-xrx200-pcie-msi-fix.patch 2> /dev/null
        ln -s ~/wave300/scripts/1000-xrx200-pcie-msi-fix.patch $f
        # git add ${f}1000-xrx200-pcie-msi-fix.patch # push to the official repo
    done

    for f in ./target/linux/lantiq/patches-5*/;
    do
        rm ${f}1005-xrx200-pcie-msi-fix.patch 2> /dev/null
        ln -s ~/wave300/scripts/1005-xrx200-pcie-msi-fix.patch $f
        # git add ${f}1005-xrx200-pcie-msi-fix.patch # push to the official repo
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

    echo -e '\e[1;31m[build_openwrt]\e[0m Inserting configs for the wave300 routers ...'
    for f in $(find ./target/linux/lantiq/file* -name '*.dts*'); # ./target/linux/lantiq/files*/arch/mips/boot/dts/*;
    do
        if ! grep -q 'pcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        then
            sed -i '/&pci0 {/a\\tpcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        fi
    done
    #</suleiman>
    
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
    
    # TODO: link the 4 firmwares bin files (ahmar seems to work for everybody) to be added in the system image
    
    # TODO: find if the hostapd patch from v2.7 works with current versions and where to put it

    x=$( (speaker-test -t sine -f 600 -l 1) & pid=$!; sleep 0.6s; kill -9 $pid )
    echo -e "\e[1;31m[build_openwrt]\e[0m Starting make menuconfig ..."
    make menuconfig
    
    echo -e '\e[1;31m[build_openwrt]\e[0m Starting download, this will take some minutes ...'
    make download 
    
    cp ~/wave300/scripts/led package/base-files/files/etc/init.d/ # version that allow leds to turn off. TODO make patch and pull request
    
    threads=$(($(nproc --all) - 1))
    echo -e "\e[1;31m[build_openwrt]\e[0m starting compilation with low priority for: storage and \e[1;33m$threads\e[0m threads, this may take hours, sound alarm starts when it finishes. `date`"
    ionice -c 3 nice -n19 make -O -j$threads

    if [ $? == 0 ]
    then
        ls -phltr ./bin/targets/lantiq/xrx200/*.bin
        echo -e '\e[1;31m[build_openwrt]\e[0m Use the above sysupgrade file of your router to flash it'
    else
        echo -e '\e[1;31m[build_openwrt]\e[0m To find details about the error run: make (put here the first failed make recipe) V=sc'
        #make $frecipe V=sc
    fi

    echo -e "\e[1;31m[build_openwrt]\e[0m Elapsed time: \e[1;33m $(($(date -u +%s)-$start_time)) \e[0m seconds.`date`"
    echo -e '\e[1;31m[build_openwrt]\e[0m Hit Crtl+c to stop alarm ...'
    while [ 1 ]
    do
        x=$( (speaker-test -t sine -f 1250 -l 1) & pid=$!; sleep 0.15s; kill -9 $pid )
        sleep 7s
    done

    break
done
