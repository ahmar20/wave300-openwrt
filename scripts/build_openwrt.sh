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
    
    # config nginx
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
    echo 'openwrt directory found, skiping downloads (apt install, git colone, 1000-xrx200-pcie-msi-fix.patch)'
fi

cd openwrt

echo '
        Disclaimer: this is provided without warranties, 
        double check everything, use at your own risk !
        
        -> After you choose the branch:
         feeds and patches will be installed and
         menuconfig will open with target=lantiq and subtarget=XRX200.
        -> Optionally select your router(target profile) and other packages you like,
         this may(needs to investigate) change opkg magic hash, wich prevents
         installation of some package from official repositories.
        -> Save and exit.
        
        Then (make download and compile) will be executed, this takes a while,
        a sound test may play when it finishes.
'
git fetch --tags
branches=$(git tag -l)
git branch
select branch in $branches
do
    if [ "$branch" = "" ]
    then
        exit
    fi
    
    echo cleaning ...
    # make clean    # deletes contents of the directories /bin and /build_dir. 
    make dirclean # deletes contents of the directories /bin and /build_dir and additionally /staging_dir and /toolchain (=the cross-compile tools), /tmp (e.g data about packages) and /logs. 'Dirclean' is your basic “Full clean” operation.
    # make distclean # nukes everything you have compiled or configured and also deletes all downloaded feeds contents and package sources. 
    
    git reset --hard #
        
    echo configuring branch $branch ....
    git checkout $branch
    git branch

    ./scripts/feeds update -a
    ./scripts/feeds install -a
    ./scripts/feeds install libnl
    
    echo "Copying OpenWrt 'make menuconfig' '.config' file for the wave300 routers ..."
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
    echo "Applying patches for the wave300 routers ..."
    for f in ./target/linux/lantiq/patches-*/; 
    do
        ln ~/1000-xrx200-pcie-msi-fix.patch $f
        git add ${f}1000-xrx200-pcie-msi-fix.patch
    done

    echo "Adding configs for the wave300 routers ..."
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

    for f in ./target/linux/lantiq/files-*/arch/mips/boot/dts/*;
    do
        if ! grep -q 'pcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        then
            sed -i '/&pci0 {/a\\tpcie-reset = <&gpio 21 GPIO_ACTIVE_HIGH>;' $f
        fi
    done
    #</suleiman>



    make menuconfig

    make download
    ionice -c 3 nice -n19 make -j4
    speaker-test -t sine -f 250 -l 1 > /dev/null
    echo " ***** to flash your router, use the following sysupgrade file:"
    ls -phl ./bin/targets/lantiq/xrx200/*.bin

    break
done
