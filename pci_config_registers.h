/*
* This file contains pci_configuration_registers data for WAVE300 device
* For any PCI device there is a 256-byte address configuration space
* First 64 bytes are standardized and contain required and optional data
*/

#ifndef PCI_CONFIG_REGISTERS
#define PCI_CONFIG_REGISTERS

#define VendorID         0x0
#define DeviceID         0x2
#define CommandReg       0x4
#define StatusReg        0x6
#define RevisionID       0x8
#define ClassCode        0x9
#define CacheLine        0xc
#define LatencyTimer     0xd
#define HeaderType       0xe
#define BIST             0xf





#endif /* PCI_CONFIG_REGISTERS */