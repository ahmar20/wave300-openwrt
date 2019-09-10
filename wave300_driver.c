// WAVE300 Driver Development

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/dma-mapping.h>
#include <pci_vendor_registers.h>
#include <drv_shared.h>


#define LOG(string...) printk(KERN_INFO string)

#define		WAVE300_Vendor_Id	0x1a30
#define		WAVE300_Device_Id	0x0700		// WAVE300 8224
#define		WAVE300_Device_Id_2	0x0710		// WAVE300 8231
#define		DMA_64BIT_MASK		DMA_BIT_MASK(64)
#define		DMA_32BIT_MASK		DMA_BIT_MASK(32)
#define		PCI_Memory_Block_0	0
#define		PCI_Memory_Block_1	1


#ifndef		COMMANDS
	static int device_probe(struct pci_dev *dev, const struct pci_device_id *id);
	static void device_remove(struct pci_dev *dev);
	static void show_device_info(struct pci_dev *dev);
	static int reserve_memory_wave300(struct pci_dev *dev);
	static irqreturn_t wave300_interrupt_handler(int irq, void *dev_id);
	struct interrupt_info_to_pass 
	{ 
		int *some_int; 
	} dev_info;
	
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
	const char *wave300_resource_name = "wave300_memory_resources";
	static int irq;
	const char *interrupt_name = "wave300";
	unsigned int *BAR_0_mem;
	unsigned int *BAR_1_mem;

#endif

static int wave300_driver_init(void)
{
	int err;
	LOG("WAVE300 Module initializing...\n");
	err = pci_register_driver(&pci_driver_wave300);
	if (err < 0) LOG("Device registration failed with error: %x.\n", err);
	else
	{
		show_device_info(pci_dev_wave300);
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
	int consistent_dma;
	err = pci_enable_device(dev);
	if (err < 0)
	{
		LOG("Failed to enable device. Error: %d\n", err);
		return err;
	}
	pci_set_master(dev);
	
	pci_try_set_mwi(dev);
	err = reserve_memory_wave300(dev);
	if (err < 0) return err;

	err = pci_set_dma_mask(dev, DMA_64BIT_MASK);
	if (err < 0)
	{
		LOG("64bit DMA mask failed.\n");
		err = pci_set_dma_mask(dev, DMA_32BIT_MASK);
		if (err < 0)
		{
			LOG("32bit DMA mask failed.\n");
			return err;
		}
		else
		{
			consistent_dma = pci_set_consistent_dma_mask(dev, DMA_32BIT_MASK);
			if (consistent_dma < 0)
			{
				LOG("Consistent DMA Mask(32) not set.");
			}
			LOG("32bit mask set for WAVE300.\n");
		}
	}
	else
	{
		consistent_dma = pci_set_consistent_dma_mask(dev, DMA_64BIT_MASK);
		if (consistent_dma < 0)
		{
			LOG("Consistent DMA Mask(64) not set.");
		}
		LOG("64bit mask set for WAVE300.\n");
	}

	// clear any pending interrupts here before registering interrupt handler
	irq = dev->irq;
	enable_irq(irq);
	err = request_irq(irq, &wave300_interrupt_handler, IRQF_SHARED, interrupt_name, &dev_info);
	if (err < 0)
	{ 
		LOG("IRQ registration failed with error code: %d\n", err);
		return err;
	}
	else
	{
		LOG("IRQ registered for WAVE300.\n");
	}
	pci_dev_wave300 = dev;
	return err;
}

static void device_remove(struct pci_dev *dev)
{
	iounmap(BAR_0_mem);
	iounmap(BAR_1_mem);

	disable_irq(irq);
	free_irq(irq, &dev_info);
	pci_clear_mwi(dev);
	pci_clear_master(dev);
	
	pci_disable_device(dev);
	pci_release_regions(dev);
}

static void show_device_info(struct pci_dev *dev)
{
	LOG("Device has been identified as: \nVendor(%x), Device(%x), Class(%x), \nPower-State(%x),\
	PM-Capability-Offset(%x), \nD1-Support(%x), D2-Support(%x), \nClear-Retrain-Link(%x),\
	D3-D0-Transition-Time(%x), \nConfiguration-Space-Size(0x%x), IRQ(0x%x).\n",
	dev->vendor, dev->device, dev->class, dev->current_state, dev->pm_cap, dev->d1_support,
	dev->d2_support, dev->clear_retrain_link, dev->d3_delay, dev->cfg_size, dev->irq);
}

static irqreturn_t wave300_interrupt_handler(int irq, void *dev_Id)
{
	LOG("IRQ %x may be handled here!\n", irq);

	return IRQ_HANDLED;
}

static int reserve_memory_wave300(struct pci_dev *dev)
{
	int err;
	err = pci_request_regions(dev, wave300_resource_name);
	if (err < 0)
	{
		LOG("Failed to acquire memory regions for WAVE300.\n");
		return err;
	}
	else
	{
		LOG("Aquired memory regions for WAVE300.\n");
	}

	if (!(dev->no_msi))
	{
		LOG("MSI can be enabled on WAVE300.\n");
		pci_enable_msi(dev);
	}
	// determine how to distinguish between PCI and PCIe
	if (true) // check if the device is PCI or PCIe - true for PCI
	{
		BAR_0_mem = ioremap(pci_resource_start(dev, PCI_Memory_Block_0), pci_resource_len(dev, PCI_Memory_Block_0));
		LOG("PCI Memory block %i: PA: 0x%p, VA: 0x%p, Len=0x%x\n", PCI_Memory_Block_0,
            (void*) pci_resource_start(dev, PCI_Memory_Block_0), BAR_0_mem, 
            (unsigned int) pci_resource_len(dev, PCI_Memory_Block_0));
	}
	BAR_1_mem = ioremap(pci_resource_start(dev, PCI_Memory_Block_1), pci_resource_len(dev, PCI_Memory_Block_1));
	LOG("PCI Memory block %i: PA: 0x%p, VA: 0x%p, Len=0x%x\n", PCI_Memory_Block_1,
		(void*) pci_resource_start(dev, PCI_Memory_Block_1), BAR_1_mem, 
		(unsigned int) pci_resource_len(dev, PCI_Memory_Block_1));

	return err;
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
