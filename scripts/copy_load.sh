cd ~/wave300/scripts
[ -d scp ] || mkdir scp
cd scp

if [ "$host" = "" ] # ignore if it was exported
then
    echo "Enter router address: ex: root@192.168.0.1"
    read host
    # sed -i "i/host=$host/" $0 fi # TODO: save to file
fi    
if [ "$pass" = "" ]
then
    echo "Enter router password:"
    read pass
    # sed -i "i/pass=$pass/" $0 # TODO: save to file
fi

echo -e "\n-------------------- router firmware files:"
sshpass -p $pass ssh $host 'ls -tphl /lib/firmware/ProgModel_* /lib/firmware/contr_lm.bin /lib/firmware/ap_upper_wave300.bin 2> /dev/null'
echo -e "\n-------------------- router module files: (auto rsync)"
sshpass -p $pass ssh $host 'ls -tphl /lib/modules/ | grep -v /'

echo -e '\n-------------------- What do you want to do?';
select option in 'Reboot the router' 'Copy and test modules and firmwares' 'Clear previus firmwares attempts' 'Start the driver stored on the router' 'Config it to run on boot'
do
    if [ "$option" = "Reboot the router" ]
    then
        sshpass -p $pass ssh $host "reboot"
        echo "... Wait for lan reconnection before procede ..."
    fi

    if [ "$option" = "Copy and test modules and firmwares" ]
    then
        break;
    fi


    if [ "$option" = "Clear previus firmwares attempts" ]
    then
        for firmware in cal_wlan0 ap_upper_wave300 sta_upper_wave300 contr_lm ProgModel_BG_CB_wave300 ProgModel_BG_CB_3 ProgModel_BG_nCB_wave300 ProgModel_BG_nCB_3
        do
            echo "Removing local link and router file: $firmware*.bin"
            [ -L $firmware*.bin ] && rm $firmware*.bin
            sshpass -p $pass ssh $host "[ -f /lib/firmware/$firmware*.bin ] && rm /lib/firmware/$firmware*.bin"
        done    
        break;
    fi  

    if [ "$option" = "Start the driver stored on the router" ] # use this option after you have all the files on the router
    then
        echo "-------------------- Starting mtlkroot.ko ..."
        sshpass -p $pass ssh $host 'insmod /lib/modules/mtlkroot.ko cdebug=3'
        echo "-------------------- Starting mtlk.ko ..."
        sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1'
        echo "-------------------- Router info ..."
        sshpass -p $pass ssh $host 'brctl addif "br-lan" "wlan0" ; iwlist wlan0 f ; iwinfo ; ifconfig wlan0 ; brctl show' # bridge and show info
        echo "-------------------- Starting hostpad in the background ..."
        sshpass -p $pass ssh -x $host '/lib/modules/mtlk-ap -d /lib/modules/config.conf &>/dev/nul &' # load hostapd
        echo "-------------------- In a few seconds the wlan0 will be avaiable ..."
        exit 1
    fi

    if [ "$option" = "Config it to run on boot" ]
    then
        echo Creating config ....
        sshpass -p $pass ssh $host ' echo "#!/bin/sh /etc/rc.common
START=99
STOP=10
start(){
    insmod /lib/modules/mtlkroot.ko
    insmod /lib/modules/mtlk.ko ap=1
    brctl addif "br-lan" "wlan0"
    /lib/modules/mtlk-ap -d /lib/modules/config.conf &>/dev/nul &
}
stop(){
    killall mtlk-ap
    brctl delif "br-lan" "wlan0"
    # rmmod mtlk.ko
    # rmmod mtlkroot.ko
}" > /etc/init.d/wave300'
        sshpass -p $pass ssh $host 'chmod +x /etc/init.d/wave300'
        sshpass -p $pass ssh $host '/etc/init.d/wave300 enable'
    fi

    echo "Do you want run another option?"
done


if ! [ -f config.conf ];
then
    ln -s ../config.conf
    ln -s ~/wave300/builds/ugw5.4-vrx288/binaries/wls/driver/mtlkroot.ko
    ln -s ~/wave300/builds/ugw5.4-vrx288/binaries/wls/driver/mtlk.ko
    ln -s ~/hostapd-devel-mtlk/hostapd/hostapd mtlk-ap
    #ln -s ~/hostapd-devel-mtlk/wpa_supplicant/wpa_supplicant

    echo "Configuring local opkg repo ..."
    sshpass -p $pass ssh $host "cat /etc/opkg/distfeeds.conf | sed -e 's/downloads.openwrt.org\/releases\/[0-9.]*/$HOSTNAME.lan/' > /etc/opkg/customfeeds.conf" # | sed -e 's/openwrt/local/'
    sshpass -p $pass ssh $host 'mv /etc/opkg/keys/key-build.pub /etc/opkg/keys/$(usign -F -p /etc/opkg/keys/key-build.pub)'

    echo "Updating and installing some tools ..."
    sshpass -p $pass ssh $host 'opkg update; opkg install rsync libopenssl'
    #sshpass -p $pass ssh $host 'opkg install kmod-usb-storage block-mount kmod-fs-vfat kmod-nls-cp437 kmod-nls-iso8859-1 '
    #sshpass -p $pass ssh $host 'opkg install pciutils wpad wireless-tools iw iwinfo ' # not installing from local binaries
    #sshpass -p $pass ssh $host 'opkg install libnl libnl-tiny' #    kmod-lib80211 kmod-mac80211 kmod-cfg80211'
fi


echo "Coping/Updating modules to the router ..."
sshpass -p $pass rsync -caL config.conf mtlkroot.ko mtlk.ko mtlk-ap $host:/lib/modules/

echo "-------------------- starting mtlkroot.ko ..."
sshpass -p $pass ssh $host 'insmod /lib/modules/mtlkroot.ko cdebug=3'
# here should check if mtlkroot.ko loaded correctly
echo "-------------------- insmod mtlk.ko ..."
sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1'

# - cal_wlan0 calibration data -> probably on EEPROM
# ap_upper_wave300 -> access point operation
# - sta_upper_wave300 -> station operation (not working?)
# contr_lm.bin: Lower interface (PCI/PCIe communication with the driver)
# ProgModel_BG_nCB_wave300.bin - PHY firmware (?)
# ProgModel_BG_nCB_3D_RevB_wave300.bin - HW firmware (?)
IFS=$'\n'
cd ../firmware
for firmware in cal_wlan0 ap_upper_wave300 sta_upper_wave300 contr_lm ProgModel_BG_CB_wave300 ProgModel_BG_CB_3 ProgModel_BG_nCB_wave300 ProgModel_BG_nCB_3
do
    files=$(find -name "$firmware*.bin")
    if [[ "$files" != "" ]] && ! [ -L ../scp/$firmware*.bin ]
    then
        [ "$firmware" = 'ProgModel_BG_CB_wave300' ] && echo "-------------------- Configuring bridge" && sshpass -p $pass ssh $host 'brctl addif "br-lan" "wlan0" ; iwlist wlan0 f ; iwinfo ; ifconfig wlan0 ; brctl show'

        while true 
        do
            sshpass -p $pass ssh $host 'dmesg -c' | grep --color=auto -nw -C 5 $(basename $firmware)
            echo -e "\n-------------------- What do you want to try?"
            last="Last file worked, so lets procede to the next"
            select file in $files
            do
                [ "$file" = $last ] && break;

                [ -L ../scp/$firmware*.bin ] && rm ../scp/$firmware*.bin # remove previus firmware
                sshpass -p $pass ssh $host "[ -f /lib/firmware/$firmware*.bin ] && rm /lib/firmware/$firmware*.bin"

                [ "$file" = "" ] && exit;

                echo -e "Creating link and coping file to the router ..."
                ln -s $file ../scp/$(basename $file)
                sshpass -p $pass scp $file $host:/lib/firmware/
                if [[ "${firmware:0:9}" != "ProgModel" ]]
                then
                    echo "-------------------- insmod mtlk.ko ..."
                    sshpass -p $pass ssh $host 'rmmod /lib/modules/mtlk.ko'
                    sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1'
                else
                    echo "-------------------- starting hostapd ..."
                    sshpass -p $pass ssh $host '/lib/modules/mtlk-ap -d /lib/modules/config.conf' # try start hostapd
                fi
                break;
            done
        [ "${files:0:9}" = "${last:0:9}" ] || files=$last$'\n'$files
        [ "$file" = "$last" ] && break;
        done
    fi
done

