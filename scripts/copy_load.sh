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

echo "-------------------- router module files:"
sshpass -p $pass ssh $host 'ls /lib/modules/ -tphl | grep -v /'
echo "-------------------- router firmware files:"
sshpass -p $pass ssh $host 'ls /lib/firmware/ -tphl | grep -v /'

echo '--------------------  What do you want to do?';
select option in 'Reboot the router' 'Copy and test modules and firmwares' 'Clear previus firmwares attempts' 'Start the driver stored on the router' 'Config it to run on boot'
do
    if [ "$option" = "Reboot the router" ]
    then
        sshpass -p $pass ssh $host "reboot"
        echo "wait lan reconnect before procede ..."
    fi

    if [ "$option" = "Copy and test modules and firmwares" ]
    then
        break;
    fi


    if [ "$option" = "Clear previus firmwares attempts" ]
    then
        for firmware in cal_wlan0 ap_upper_wave300 sta_upper_wave300 contr_lm ProgModel_BG_CB_wave300 ProgModel_BG_CB_3 ProgModel_BG_nCB_wave300 ProgModel_BG_nCB_3
        do
            echo "Removing $firmware local link and router file"
            rm $firmware
            sshpass -p $pass ssh $host "rm /lib/firmware/$firmware"
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
        echo Starting ....
        sshpass -p $pass ssh $host '/etc/init.d/wave300 start'
        exit 1
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

    #echo "Instaling local openwrt build keys ..."
    #ln -s ~/openwrt/key-build.pub
    #sshpass -p $pass ssh $host 'ln -s /lib/modules/key-build.pub /etc/opkg/keys/$(usign -F -p /lib/modules/key-build.pub)'
    #sshpass -p $pass ssh $host "sed -i 's/downloads.openwrt.org\/releases\/19.07.7/$( ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' )/' /etc/opkg/distfeeds.conf"
    #sshpass -p $pass ssh $host "sed -i 's/$( ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' )/downloads.openwrt.org\/releases\/19.07.7/' /etc/opkg/distfeeds.conf"
    
    #echo "Updating and installing some tools ..."
    sshpass -p $pass ssh $host 'opkg update; opkg install rsync'
    #sshpass -p $pass ssh $host 'opkg install kmod-usb-storage block-mount kmod-fs-vfat kmod-nls-cp437 kmod-nls-iso8859-1 '
    #sshpass -p $pass ssh $host 'opkg install pciutils wpad wireless-tools iw iwinfo libopenssl' # not installing from local binaries
    #sshpass -p $pass ssh $host 'opkg install libnl libnl-tiny' #    kmod-lib80211 kmod-mac80211 kmod-cfg80211'
fi


echo "Coping/Updating modules to the router ..."
sshpass -p $pass rsync -caL config.conf mtlkroot.ko mtlk.ko mtlk-ap $host:/lib/modules/

echo "-------------------- Starting mtlkroot.ko ..."
sshpass -p $pass ssh $host 'insmod /lib/modules/mtlkroot.ko cdebug=3'
echo "-------------------- Starting mtlk.ko ..."
sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1; dmesg | tail -n 69'


# - cal_wlan0 calibration data -> probably on EEPROM
# ap_upper_wave300 -> access point operation
# - sta_upper_wave300 -> station operation (not working?)
# contr_lm.bin: Lower interface (PCI/PCIe communication with the driver)
# ProgModel_BG_nCB_wave300.bin - PHY firmware (?)
# ProgModel_BG_nCB_3D_RevB_wave300.bin - HW firmware (?)
text="--------------------\nSelect the new firmware cadidate, or \n '1' to procede to the next firmware, or \n any other value, removes from router and scp the candidate: "
cd ../firmware
for firmware in cal_wlan0 ap_upper_wave300 sta_upper_wave300 contr_lm ProgModel_BG_CB_wave300 ProgModel_BG_CB_3 ProgModel_BG_nCB_wave300 ProgModel_BG_nCB_3
do
    files="Next $(find -name "$firmware*.bin")"
    if [[ "$files" != "Next " ]]
    then
        last=$(ls ../scp/$firmware*.bin -t1 2> /dev/null | head -n 1 ) #; echo $last
        if [ "$firmware" = 'ProgModel_BG_CB_wave300' ]
        then
            sshpass -p $pass ssh $host 'brctl addif "br-lan" "wlan0" ; iwlist wlan0 f ; iwinfo ; ifconfig wlan0 ; brctl show' # bridge and show info
            sshpass -p $pass ssh $host '/lib/modules/mtlk-ap -d /lib/modules/config.conf ; dmesg | tail -n 69' # load hostapd
        fi
        echo -e "$text$last"
        select file in $files
        do
            if [ "$file" = "Next" ]
            then
                break
            fi
            
            if [[ "$last" != "" ]] 
            then
                rm ../scp/$(basename $last)
                sshpass -p $pass ssh $host "rm /lib/firmware/$(basename $last)" # remove previus firmware
            fi
            last=$file
            
            if [[ "$file" != "" ]] 
            then
                ln -s $file ../scp/$(basename $file) # ?
                sshpass -p $pass scp $file $host:/lib/firmware/ # copy new candidate
                if [[ "${firmware:0:9}" != "ProgModel" ]]
                then
                    echo "-------------------- loading mtlk.ko ..."
                    sshpass -p $pass ssh $host 'rmmod /lib/modules/mtlk.ko' # ?
                    sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1; dmesg | tail -n 69' # try load wave300
                else
                    echo "-------------------- starting hostapd ..."
                    sshpass -p $pass ssh $host '/lib/modules/mtlk-ap -d /lib/modules/config.conf ; dmesg | tail -n 69' # try start hostapd
                fi
            fi
            echo -e "$text$last"
        done
    fi
done

