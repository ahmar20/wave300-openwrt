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


## RF Library (rflib)
The rflib library seems to be distributed separated from the driver itself. The driver can either statically link the distributed `mtlk_rflib.a` library or just use copied rflib sources. The kernel modules from multiple routers use version 3.4 of the driver licensed under GPL, but the source codes for v3.4 rflib are yet to be found. Even worse, the distributed `mtlk_rflib.a` is precompiled for non WAVE300 devices. For these reasons we need to backport the rflib sources from version 3.5 (sort of an obsoleted dead branch of development).\
The WAVE300 is not officially supported in v3.5 (and after WAVE300 devices have 5.x branch), but the support is still not removed. The sane assumption is the rflib part didn't change too much. I've disassembled the archive files from v3.4 and compared few percents of the code with v3.5 and it confirms it.

The v3.5 rflib source code `lq-wave-300-03.05.00.00.53.a2676e338c1e.rflib.wls.src.tar.bz2` can be obtained from: [Google Drive](https://drive.google.com/file/d/1Bozk1Cc8fB-FMgkxegyaSIBxru08bwJv/view).

As suggested in the openwrt forum, the updates are being distributed in a separate repo (currently through Google drive).

## Compilation and Installation
Obtain `wave300_rflib` sources from the external repo and copy them into the driver (from one top source directory to other top source directory).\
Set your compiled openwrt root in the file `support/ugw.env.common` with variable `DEFAULT_TOOLCHAIN_PATH`. The driver requires libnl-3 library (not libnl-tiny).\
The ./Makefile in the root is a special file. It starts the build (not the ./configure).

Do NOT run ./configure. It will delete ./Makefile, mess the build system and you will lose Kconfig support.

Some combinations of missing files may be undefined. You can delete all build-oriented files by:
`make distclean`\
The default options should create the working driver. Other options may cause kernel crashes or gcc refuses to compile them (patches are welcome): `make menuconfig`
`make`

It seems -j flag for make doesn't make much (almost no parallelization possible, too much dependencies). For every file some perl script is started (I think it is generating "SLID" debug info).

Resulting files are in `builds/ugw5.4-vrx288/binaries/wls/driver`. Copy them into the standard place in `/lib/modules`. Copy the firmware files into `/lib/firmware`.

Insert the modules with the following commands:\
`insmod mtlkroot.ko`
`insmod mtlk.ko ap=1`

Use iwpriv for setting TX power (there is a list).
Set your own country: `iwpriv wlan0 sCountry XXX`\
The global settings, not sure if the driver supports this interface:

`iw reg set XXX`
`iwconfig wlan0 essid test`
`ifconfig wlan0 10.0.0.42 netmask 255.255.255.0 up`

If the driver still didn't crash, do iperf/iperf3 test.

Unloading the modules causes crash, stopping the interface should be OK.

## Conclusions

The driver has very complicated macros, build system and function calls used probably for some code robustness. Problem is this complexity is probably causing another bugs (read git log). The best way for the continuation of the opens ource driver is a complete independent GPL rewrite of the driver. Anyway, the patches are welcomed.

The problems:
* write interface documentation from the driver, use it for new GPL driver
* search for a newer firmwares, documentation, source codes, etc...
* build process: Fix makefiles, Kconfig support in configure, autodetect libraries (like libnl) for host and target, speed up and parallelization, out of the tree build
* Only some (default) configurations are valid, others fails on various problems, find them and fix them
* Add more configuration options (STA_REF_DBG, there is probably more)
* Remove WAVE400+ support, dead code (from v3.5 there is change in ABI, incompatible with latest v3.4 wave300, newer chipsets are still supported by Intel)
* Remove pre 4.14 kernel support (cannot be tested)
* Fix Station mode. It seems the driver won't even try to communicate with chip without ap=1 parameter
* Fix random crashes (= understand driver's macro hell)
* Add hostapd and wpa_supplicant support
* Add fixes from newer versions (rest 3.5 and 5.x)

###### Much thanks to all the people involved in development and testing.
