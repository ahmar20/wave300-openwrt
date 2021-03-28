cd ~/wave300/scripts
mkdir scp
cd scp

if [ "$host" = "" ] # ignore if it was exported
then
    echo "Enter router address: ex:   user@addr"
    read host
    # sed -i "i/host=$host/" $0
fi
if [ "$pass" = "" ]
then
    echo "Enter router password:"
    read pass
    # sed -i "i/pass=$pass/" $0
fi

echo 'Before we start, do you want to reboot or clear files from the router and scp link folder?';
select option in 'No' 'Modules' 'Firmwares' 'Reboot'
do
    if [ "$option" = "" ] || [ $option = "No" ]
    then
        break;
    fi

    if [ "$option" = "Reboot" ]
    then
        sshpass -p $pass ssh $host "reboot"
        echo "wait lan reconnect before procede ..."
    fi

    
    shopt -s extglob
    for file in $( [ $option == Modules ] && ls !(*.bin) 2>/dev/null || ls *.bin 2>/dev/null )
    do
        echo "removing: $file"
        rm ../scp/$(basename $file)
        sshpass -p $pass ssh $host "rm /lib/$([ $option == Modules ] && echo "modules" || echo "firmware")/$(basename $file)" # remove previus 
    done
    echo "more?"
done


if ! [ -f config.conf ];
then
    echo " Copying drivers ..."
    ln ../config.conf
    ln ~/wave300/builds/ugw5.4-vrx288/binaries/wls/driver/mtlk.ko
    ln ~/wave300/builds/ugw5.4-vrx288/binaries/wls/driver/mtlkroot.ko
    ln ~/openwrt/key-build.pub
    ln ~/hostapd-devel-mtlk/hostapd/hostapd mtlk-ap
    ln ~/hostapd-devel-mtlk/wpa_supplicant/wpa_supplicant
    sshpass -p $pass scp * $host:/lib/modules/
    
    sshpass -p $pass ssh $host 'ln /lib/modules/key-build.pub /etc/opkg/keys/$(usign -F -p /lib/modules/key-build.pub)'
    sshpass -p $pass ssh $host "sed -i 's/downloads.openwrt.org\/releases\/19.07.7/$( ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' )/' /etc/opkg/distfeeds.conf"
    #sshpass -p $pass ssh $host "sed -i 's/$( ifconfig | sed -En 's/127.0.0.1//;s/.*inet (addr:)?(([0-9]*\.){3}[0-9]*).*/\2/p' )/downloads.openwrt.org\/releases\/19.07.7/' /etc/opkg/distfeeds.conf"
    sshpass -p $pass ssh $host 'opkg update'
    sshpass -p $pass ssh $host 'opkg install kmod-usb-storage block-mount kmod-fs-vfat kmod-nls-cp437 kmod-nls-iso8859-1 '
    sshpass -p $pass ssh $host 'opkg install pciutils wpad wireless-tools iw iwinfo libopenssl' # not installing from local binaries
    sshpass -p $pass ssh $host 'opkg install libnl libnl-tiny' #    kmod-lib80211 kmod-mac80211 kmod-cfg80211'
fi


echo "-------------------- router module files:"
sshpass -p $pass ssh $host 'ls /lib/modules/ -tp | grep -v /'
echo "-------------------- router firmware files:"
sshpass -p $pass ssh $host 'ls /lib/firmware/ -tp | grep -v /'
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
            sshpass -p $pass ssh $host 'iwinfo' # get the interface name here and update on config.conf
            #sshpass -p $pass ssh $host    sed -i "s/wlan0/wlan$i" config.conf
            sshpass -p $pass ssh $host 'ifconfig wlan0 192.168.0.1 netmask 255.255.255.0'
            sshpass -p $pass ssh $host '/lib/modules/mtlk-ap /lib/modules/config.conf ; dmesg | tail -n 69' # load hostapd
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
                ln $file ../scp/$(basename $file) # ?
                sshpass -p $pass scp $file $host:/lib/firmware/ # copy new candidate
                if [[ "${firmware:0:9}" != "ProgModel" ]]
                then
                    echo "-------------------- loading mtlk.ko ..."
                    sshpass -p $pass ssh $host 'rmmod /lib/modules/mtlk.ko' # ?
                    sshpass -p $pass ssh $host 'insmod /lib/modules/mtlk.ko ap=1; dmesg | tail -n 69' # try load wave300
                else
                    echo "-------------------- starting hostapd ..."
                    sshpass -p $pass ssh $host '/lib/modules/mtlk-ap /lib/modules/config.conf ; dmesg | tail -n 69' # try start hostapd
                fi
            fi
            echo -e "$text$last"
        done
    fi
done

