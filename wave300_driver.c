// WAVE300 Driver Development

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>


#define LOG(string...) printk(KERN_INFO string)

#define		WAVE300_Vendor_Id	0x1a30
#define		WAVE300_Device_Id	0x0700		// WAVE300 8224
#define		WAVE300_Device_Id_2	0x0710		// WAVE300 8231

#ifndef		COMMANDS
	static int device_probe(struct pci_dev *dev, const struct pci_device_id *id);
	static void device_remove(struct pci_dev *dev);
	static void show_device_info(struct pci_dev *dev);
	
	#ifdef 	OldStylePCIGetDevice
		static int find_current_wave300_device(void);
	#endif

#endif

#ifndef		VARIABLES
	static struct pci_dev *pci_dev_wave300;
	static struct pci_device_id pci_device_id_table_wave300[] =
	{
		{
			vendor: WAVE300_Vendor_Id,
			device: WAVE300_Device_Id,
			subvendor: PCI_ANY_ID,
			subdevice: PCI_ANY_ID,
			class: 0,
			class_mask: 0,
			driver_data: 0,
		},
		{
			vendor: WAVE300_Vendor_Id,
			device: WAVE300_Device_Id_2,
			subvendor: PCI_ANY_ID,
			subdevice: PCI_ANY_ID,
			class: 0,
			class_mask: 0,
			driver_data: 0,
		},
		{ 0, }
	};
	static struct pci_driver pci_driver_wave300 =
	{
		name: "wave300_driver",
		id_table: pci_device_id_table_wave300,
		probe: device_probe,
		remove: device_remove,
		//resume: device_resume,
		//suspend: device_suspend,
	};

	static unsigned long mmio_base;
	static unsigned long mmio_length;
	static unsigned long mmio_flags;
	static unsigned char wave300_irq;
#endif

static int 	wave300_driver_init(void)
{
	int err;
	LOG("WAVE300 Module initializing...\n");
	err = pci_register_driver(&pci_driver_wave300);
	if (err < 0) LOG("Device registration failed with error: %x.\n", err);
	else
	{
		LOG("Device registration succeeded.\n");
	}
	
	if (err < 0)
	{
		LOG("Initializing failed.\n");
	}
	else
	{
		LOG("Initializing succeeded.\n");
	}

	return err;
}

static void wave300_driver_exit(void)
{
	LOG("WAVE300 Module exiting...\n");
	pci_unregister_driver(&pci_driver_wave300);
	LOG("Device unregistered.\n");
}

static int device_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	int err;
	pci_dev_wave300 = dev;

	LOG("Device probed.\n");
	err = pci_enable_device(dev);
	if (err < 0)
	{
		LOG("Failed while enabling. Error: %d\n", err);
	}
	else
	{
		LOG("Device enable succeeded\n");
	}

	show_device_info(dev);

	//err = pci_request_regions()

	return err;
}

static void device_remove(struct pci_dev *dev)
{
	pci_release_regions(dev);
	pci_disable_device(dev);
}

static void show_device_info(struct pci_dev *dev)
{
	LOG("Device has been identified as: \nVendor(%x), Device(%x), Class(%x), \nPower-State(%x),\
	PM-Capability-Offset(%x), \nD1-Support(%x), D2-Support(%x), \nClear-Retrain-Link(%x),\
	D3-D0-Transition-Time(%x), \nConfiguration-Space-Size(%x), IRQ(%x)\n",
	dev->vendor, dev->device, dev->class, dev->current_state, dev->pm_cap, dev->d1_support,
	dev->d2_support, dev->clear_retrain_link, dev->d3_delay, dev->cfg_size, dev->irq);
}

#ifdef		OldStylePCIGetDevice
	/*
	* Old style pci_get_device implementation
	*/
	static int find_current_wave300_device(void)
	{
		int err;
		int device;

		pci_dev = NULL;
		// old method to get a specific device
		pci_dev = pci_get_device(WAVE300_Vendor_Id, WAVE300_Device_Id, pci_dev);
		if (pci_dev == NULL)
		{
			pci_dev = pci_get_device(WAVE300_Vendor_Id, WAVE300_Device_Id_2, pci_dev);
			if (pci_dev != NULL) device = 2;
		}
		if (pci_dev != NULL) device = 1;

		if (pci_dev != NULL)
		{
			LOG("WAVE300 device found. Device has been identified as: Vendor: %x , Device: %x , CLass: %x \n",
			pci_dev->vendor, pci_dev->device, pci_dev->class);
			pci_dev_put(pci_dev);
		}
		if (pci_dev == NULL) err = -1;
		if (err < 0) return err;

		return device;
	}
#endif


#ifndef		ModuleSpecificCommands

module_init(wave300_driver_init);
module_exit(wave300_driver_exit);

/*
 * Probably related to hot-pluggable PCI devices.
 * In case a device gets available the kernel can load
 * the specific driver modules through this
 */
MODULE_DEVICE_TABLE(pci, pci_device_id_table_wave300);

MODULE_DESCRIPTION("Sample WAVE300 Driver Module");
MODULE_AUTHOR("Ahmar <pakahmar@hotmail.com>");
MODULE_LICENSE("GPL");
#endif
