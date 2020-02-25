# Lantiq WAVE300 Driver
WAVE300 Driver Development for OpenWrt\
The original source of the info and code is: https://repo.or.cz/wave300.git

Initially driver was being ported by [Vittorio Alfieri](https://forum.openwrt.org/u/vittorio88) and [Mandrake Lee](https://forum.openwrt.org/u/mandrake-lee) (mainly) but then the development was appraently stopped due to lack of testers/much work. This code was ported by [Peter Cvek](https://forum.openwrt.org/u/pc2005) and tested by him and [me](https://forum.openwrt.org/u/ahmar16), initially.

## Introduction
The WAVE300 WiFi driver seems to be obsoleted and no longer developed. This causes problems for anyone trying to use it in the current OpenWrt environment as driver API gets old and incompatible with newer kernel versions. This repo is trying to fix the original driver and keep it updated.\
The WAVE300 is a 802.11 abgn PCI(e) (depends on PHY version and chip sub-type). The controller originates from Metalink (that's why mtlk.ko), developed by Lantiq and nowadays owned by Intel.

More information is available in following links:\
[Hardware Info @OpenWrt](https://openwrt.org/docs/techref/hardware/soc/soc.lantiq#wave300)\
[Hardware Info @WikiDevi](https://wikidevi.com/wiki/Lantiq#WAVE300)\
[WAVE300 Forum @OpenWrt](https://forum.openwrt.org/t/support-for-wave-300-wi-fi-chip/24690)\

## Firmware and its parts
The driver tries to load up to 6 different firmware files (depending on your device).

if you think you have found a new version of any of the following firmware files somewhere, then please report it to the above OpenWrt Forum.

1. cal_wlan0.bin:\
This file is external EEPROM data specified by the vendor. It contains the default MAC, country, channels, subtype of wave300 chip and revision information used for other firmware files. It is not required if there is a physical EEPROM on the PCB, otherwise you should use vendor's file. Do not change unless you really know what are you doing. Bitfield definitions for the progmodel are in the rflib's structure mtlk_cis_cardid_t (offset 0x0000004a in EEPROM/file).

2. ap_upper_wave300.bin:\
MAC layer firmware (upper). This firmware is required for the Access point operation and it must be (most likely) ABI compatible with the lower interface firmware. The current driver seems to try to call nonexisting functions in one of 3.4 firmware and will crash. So you need most likely the last possible version existing on the internet (now you get the challenge with firmware hunting). The version is defined as a string in the binary and as written in the string the architecture will be probably MIPS.

3. sta_upper_wave300.bin:\
MAC layer firmware (upper). This firmware is required for the Station operation. It seems the current driver version doesn't support the Station mode, there is even "STA unsupported" comment in the new driver source code. That doesn't necessary mean the STA is not supported, maybe there are just some bugs in the driver. This probably correlates with rarity of the firmware, only few 3.4 version has been found. It is mutually exclusive with ap_upper firmware (= chip can be only in station XOR accesspoint mode), so it it now really necessary if you need only AP mode (which seems to lack ability to scan, though).

4. contr_lm.bin:\
Lower interface (PCI/PCIe communication with the driver). Found in the same places as upper firmware. Again MIPS (are there 2 CPUs in the chip?). Without this binary, the wifi won't communicate.  
The last two are loaded when the interface is enabled. The name decoding is based on the type/revision values stored in the EEPROM. They are versioned (although most of them are same). The version uint16 is located at 0x00001f8c in the "HW firmware" file.

5. ProgModel_BG_nCB_wave300.bin - PHY firmware (?)
6. ProgModel_BG_nCB_3D_RevB_wave300.bin - HW firmware (?)\
&ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; | &ensp; &ensp; | &ensp; &ensp; | &ensp; &ensp; |\
&ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; | &ensp; &ensp; | &ensp; &ensp; | &ensp; &ensp; + - - HW revision\
&ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; | &ensp; &ensp; | &ensp; &ensp; + - - - - - &nbsp;HW type, 0x3D seems to be the newest (but one cal_wlan0 was 0x43)\
&ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; | &ensp; &ensp; + - - - - - - - - - 20/40 MHz bandwidth selection (?)\
&ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp; &ensp;+ - - - - - - - - - - - - - 11bgn or 11a (2.4GHz or 5GHz)\

Copy all the files into `/lib/firmware`.

#### Warning:
This code was initially updated in the minimal way not to introduce any WiFi malfunctions but there still may be some bugs in the original code. Furthermore `rflib` was backported from the newer (3.5) version and there are also different versions of the firmware which may be incompatible with each other at some stage. The driver does not support rfkill interface (not sure if at the moment or permanently).\
Use this driver only if you absolutely know what you are doing. A misuse can lead to jamming the 2.4 and 5 GHz bands.\
You are using the driver at your own risk! It is your responsibility to doublecheck the correct country setting before every use. You should also, probably, limit TX power not to cause any interference.


