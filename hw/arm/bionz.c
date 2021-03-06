/* QEMU model of Sony BIONZ image processors */

#include "qemu/osdep.h"
#include "cpu.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "hw/boards.h"
#include "hw/char/pl011.h"
#include "hw/cpu/a9mpcore.h"
#include "hw/cpu/arm11mpcore.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "sysemu/block-backend.h"
#include "sysemu/sysemu.h"

//////////////////////////// CXD4115 ////////////////////////////
#define CXD4115_NAND_BASE 0x00000000
#define CXD4115_DDR_BASE 0x10000000
#define CXD4115_DDR_SIZE 0x10000000
#define CXD4115_DMA_BASE 0x78008000
#define CXD4115_DMA_NUM_CHANNEL 8
#define CXD4115_USB_BASE 0x78020000
#define CXD4115_ONA_BASE 0x78098000
#define CXD4115_HWTIMER_BASE(i) (0x7a000000 + (i) * 0x20)
#define CXD4115_NUM_HWTIMER 3
#define CXD4115_UART_BASE(i) (0x7a050000 + (i) * 0x1000)
#define CXD4115_NUM_UART 3
#define CXD4115_GPIO_BASE 0x7a400000
#define CXD4115_BOOTCON_BASE 0x7f000000
#define CXD4115_SRAM_BASE 0xfff00000
#define CXD4115_SRAM_SIZE 0x00008000
#define CXD4115_MPCORE_BASE 0xfffd0000
#define CXD4115_BOOTROM_BASE 0xffff0000
#define CXD4115_BOOTROM_SIZE 0x00002000

#define CXD4115_NUM_IRQ 256
#define CXD4115_IRQ_OFFSET 32
#define CXD4115_IRQ_NAND 54
#define CXD4115_IRQ_UART(i) (152 + (i))
#define CXD4115_IRQ_HWTIMER(i) (155 + (i))
#define CXD4115_IRQ_DMA(i) (168 + (i))
#define CXD4115_IRQ_USB0 233
#define CXD4115_IRQ_USB1 234

#define CXD4115_BOOT_DEVICE_OFFSET 0x0000236c
#define CXD4115_TEXT_OFFSET 0x00208000
#define CXD4115_INITRD_OFFSET 0x0062e000

//////////////////////////// CXD4132 ////////////////////////////
#define CXD4132_NAND_BASE 0x00000000
#define CXD4132_DDR_BASE 0x80000000
#define CXD4132_DDR_SIZE 0x20000000
#define CXD4132_SRAM_BASE 0xa0000000
#define CXD4132_SRAM_SIZE 0x00400000
#define CXD4132_USB_BASE 0xf0040000
#define CXD4132_BOOTCON_BASE 0xf0100000
#define CXD4132_DMA_BASE 0xf2001000
#define CXD4132_DMA_NUM_CHANNEL 4
#define CXD4132_MENO_BASE 0xf2002000
#define CXD4132_HWTIMER_BASE(i) (0xf2008000 + (i) * 0x20)
#define CXD4132_NUM_HWTIMER 5
#define CXD4132_UART_BASE(i) (0xf2038000 + (i) * 0x1000)
#define CXD4132_NUM_UART 3
#define CXD4132_MISCCTRL_BASE(i) (0xf3060000 + (i) * 0x10)
#define CXD4132_MPCORE_BASE 0xf8000000
#define CXD4132_BOOTROM_BASE 0xffff0000
#define CXD4132_BOOTROM_SIZE 0x00006000

#define CXD4132_NUM_IRQ 256
#define CXD4132_IRQ_OFFSET 32
#define CXD4132_IRQ_UART(i) (160 + (i))
#define CXD4132_IRQ_HWTIMER(i) (163 + (i))
#define CXD4132_IRQ_DMA(i) (176 + (i))
#define CXD4132_IRQ_MENO 180
#define CXD4132_IRQ_NAND 183
#define CXD4132_IRQ_USB 222

#define CXD4132_BOOT_DEVICE_OFFSET 0x00005050
#define CXD4132_CMDLINE_OFFSET 0x00013000
#define CXD4132_TEXT_OFFSET 0x00018000
#define CXD4132_INITRD_OFFSET 0x00408000

#define CXD4132_CMDLINE \
    "lpj=622592 " \
    "console=ttyAM0,115200n8 " \
    "amba2.console=1 " \
    "ip=off " \
    "initrd=0x80408000,0x00700000 " \
    "root=/dev/ram0 " \
    "boottime=0x20000@0x833C0000 " \
    "klog.size=0x20000 " \
    "klog.addr=0x833E0080 " \
    "mem=64M@0x80000000@0 " \
    "memrsv=32K@0x80000000 " \
    "memrsv=0x1270000@0x82D90000 "

//////////////////////////// CXD90014 ////////////////////////////
#define CXD90014_BOSS_SRAM_BASE 0x00000000
#define CXD90014_BOSS_IO_BASE 0x00011000
#define CXD90014_NAND_REG_BASE 0x00020000
#define CXD90014_NAND_DATA_BASE 0x10000000
#define CXD90014_DDR_BASE 0x80000000
#define CXD90014_DDR_SIZE 0x40000000
#define CXD90014_SRAM_BASE 0xc0000000
#define CXD90014_SRAM_SIZE 0x01000000
#define CXD90014_BOOTCON_BASE 0xc0005030
#define CXD90014_DDMC_BASE 0xf0104000
#define CXD90014_USB_HDMAC_BASE 0xf0204000
#define CXD90014_USB_BASE 0xf0210000
#define CXD90014_UART_BASE(i) (0xf2000000 + (i) * 0x1000)
#define CXD90014_NUM_UART 3
#define CXD90014_HWTIMER_BASE(i) (0xf2403000 + (i) * 0x100)
#define CXD90014_NUM_HWTIMER 4
#define CXD90014_BOSS_CLKRST_BASE 0xf29000d0
#define CXD90014_MISCCTRL_BASE(i) (0xf2915000 + (i) * 0x10)
#define CXD90014_USB_OTG_BASE 0xf2920000
#define CXD90014_MPCORE_BASE 0xf8000000
#define CXD90014_BOOTROM_BASE 0xffff0000
#define CXD90014_BOOTROM_SIZE 0x00006000

#define CXD90014_NUM_IRQ 256
#define CXD90014_IRQ_OFFSET 32
#define CXD90014_IRQ_UART(i) (150 + (i))
#define CXD90014_IRQ_HWTIMER(i) (153 + (i))
#define CXD90014_IRQ_BOSS 170
#define CXD90014_IRQ_USB 227

#define CXD90014_TEXT_OFFSET 0x00038000
#define CXD90014_INITRD_OFFSET 0x00628000

#define NAND_SECTOR_SIZE 0x200

#define TYPE_CXD "cxd"
#define CXD(obj) OBJECT_CHECK(CxdState, (obj), TYPE_CXD)

typedef struct CxdState {
    MachineState parent_obj;
    ARMCPU cpu;
    BlockBackend *drive;
    hwaddr loader_base;
} CxdState;

static void cxd_reset(void *opaque)
{
    CxdState *s = CXD(opaque);
    cpu_reset(CPU(&s->cpu));
    cpu_set_pc(CPU(&s->cpu), s->loader_base);
}

static hwaddr cxd_init_loader2(BlockBackend *drive)
{
    char boot_block[NAND_SECTOR_SIZE];
    uint32_t loader_offset, loader_size, loader_base;
    void *loader_buffer;

    if (blk_pread(drive, 0, boot_block, sizeof(boot_block)) < 0) {
        hw_error("%s: Cannot read boot block\n", __func__);
    }

    if (*(uint32_t *) boot_block != *(uint32_t *) "EXBL") {
        hw_error("%s: Wrong boot block signature\n", __func__);
    }

    loader_offset = (*(uint32_t *) (boot_block + 0x40)) * NAND_SECTOR_SIZE;
    loader_size = (*(uint32_t *) (boot_block + 0x44)) * NAND_SECTOR_SIZE;
    loader_base = *(uint32_t *) (boot_block + 0x50);

    loader_buffer = g_malloc(loader_size);
    if (blk_pread(drive, loader_offset, loader_buffer, loader_size) < 0) {
        hw_error("%s: Cannot read loader2\n", __func__);
    }
    rom_add_blob_fixed("loader2", loader_buffer, loader_size, loader_base);
    g_free(loader_buffer);

    return loader_base;
}

static void cxd_init_cmdline(const char *default_cmdline, const char *cmdline, hwaddr base)
{
    const char *header = "kemco ";
    const char *spacer = " ";
    const char *footer = " *";
    const size_t len = strlen(header) + strlen(default_cmdline) + strlen(spacer) + strlen(cmdline) + strlen(footer);
    char *buf = g_malloc(len + 1);
    strcpy(buf, header);
    strcat(buf, default_cmdline);
    strcat(buf, spacer);
    strcat(buf, cmdline);
    strcat(buf, footer);
    rom_add_blob_fixed("cmdline", buf, len, base);
    g_free(buf);
}

static void cxd_add_const_reg(const char *name, hwaddr base, uint32_t value)
{
    MemoryRegion *mem = g_new(MemoryRegion, 1);
    uint32_t *buffer = g_new(uint32_t, 1);
    *buffer = value;
    memory_region_init_ram_ptr(mem, NULL, name, sizeof(uint32_t), buffer);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), base, mem);
}

static void cxd4115_init(MachineState *machine)
{
    CxdState *s = CXD(machine);
    DriveInfo *dinfo;
    MemoryRegion *mem;
    DeviceState *dev;
    qemu_irq irq[CXD4115_NUM_IRQ - CXD4115_IRQ_OFFSET];
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    s->drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    object_initialize(&s->cpu, sizeof(s->cpu), machine->cpu_type);
    qdev_init_nofail(DEVICE(&s->cpu));

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD4115_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4115_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD4115_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4115_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD4115_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD4115_BOOTROM_BASE, mem);

    dev = qdev_create(NULL, TYPE_ARM11MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD4115_NUM_IRQ);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD4115_NUM_IRQ - CXD4115_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    dev = qdev_create(NULL, "onenand");
    qdev_prop_set_uint16(dev, "manufacturer_id", NAND_MFR_SAMSUNG);
    qdev_prop_set_uint16(dev, "device_id", 0x30);
    qdev_prop_set_int32(dev, "shift", 1);
    qdev_prop_set_drive(dev, "drive", s->drive, &error_fatal);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_NAND_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_NAND - CXD4115_IRQ_OFFSET]);

    dev = qdev_create(NULL, "bionz_dma");
    qdev_prop_set_uint32(dev, "version", 1);
    qdev_prop_set_uint32(dev, "num-channel", CXD4115_DMA_NUM_CHANNEL);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_DMA_BASE);
    for (i = 0; i < CXD4115_DMA_NUM_CHANNEL; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4115_IRQ_DMA(i) - CXD4115_IRQ_OFFSET]);
    }

    dev = qdev_create(NULL, "inventra_usb");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_USB_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_USB0 - CXD4115_IRQ_OFFSET]);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 1, irq[CXD4115_IRQ_USB1 - CXD4115_IRQ_OFFSET]);

    for (i = 0; i < CXD4115_NUM_HWTIMER; i++) {
        dev = qdev_create(NULL, "bionz_hwtimer");
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4115_IRQ_HWTIMER(i) - CXD4115_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4115_NUM_UART; i++) {
        pl011_create(CXD4115_UART_BASE(i), irq[CXD4115_IRQ_UART(i) - CXD4115_IRQ_OFFSET], serial_hd(i));
    }

    dev = qdev_create(NULL, "bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4115_BOOTCON_BASE);

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD4115_DDR_BASE + CXD4115_TEXT_OFFSET, CXD4115_DDR_SIZE - CXD4115_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD4115_DDR_BASE + CXD4115_INITRD_OFFSET, CXD4115_DDR_SIZE - CXD4115_INITRD_OFFSET);
        s->loader_base = CXD4115_DDR_BASE + CXD4115_TEXT_OFFSET;

        uint32_t boot_device = 1;
        rom_add_blob_fixed("boot_device", &boot_device, sizeof(boot_device), CXD4115_DDR_BASE + CXD4115_BOOT_DEVICE_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD4115_BOOTROM_BASE, CXD4115_BOOTROM_SIZE);
        s->loader_base = CXD4115_BOOTROM_BASE;
    } else if (s->drive) {
        s->loader_base = cxd_init_loader2(s->drive);
    }

    cxd_add_const_reg("gpio0_data", CXD4115_GPIO_BASE + 4, 0x48000);
    cxd_add_const_reg("ona_reset", CXD4115_ONA_BASE, 1);

    qemu_register_reset(cxd_reset, s);
}

static void cxd4132_init(MachineState *machine)
{
    CxdState *s = CXD(machine);
    DriveInfo *dinfo;
    MemoryRegion *mem;
    DeviceState *dev;
    qemu_irq irq[CXD4132_NUM_IRQ - CXD4132_IRQ_OFFSET];
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    s->drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    object_initialize(&s->cpu, sizeof(s->cpu), machine->cpu_type);
    qdev_init_nofail(DEVICE(&s->cpu));

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD4132_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4132_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD4132_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD4132_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD4132_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD4132_BOOTROM_BASE, mem);

    dev = qdev_create(NULL, TYPE_ARM11MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD4132_NUM_IRQ);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD4132_NUM_IRQ - CXD4132_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    dev = qdev_create(NULL, "onenand");
    qdev_prop_set_uint16(dev, "manufacturer_id", NAND_MFR_SAMSUNG);
    qdev_prop_set_uint16(dev, "device_id", 0x40);
    qdev_prop_set_int32(dev, "shift", 1);
    qdev_prop_set_drive(dev, "drive", s->drive, &error_fatal);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_NAND_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_NAND - CXD4132_IRQ_OFFSET]);

    dev = qdev_create(NULL, "synopsys_usb");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_USB_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_USB - CXD4132_IRQ_OFFSET]);

    dev = qdev_create(NULL, "bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_BOOTCON_BASE);

    dev = qdev_create(NULL, "bionz_dma");
    qdev_prop_set_uint32(dev, "version", 2);
    qdev_prop_set_uint32(dev, "num-channel", CXD4132_DMA_NUM_CHANNEL);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_DMA_BASE);
    for (i = 0; i < CXD4132_DMA_NUM_CHANNEL; i++) {
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), i, irq[CXD4132_IRQ_DMA(i) - CXD4132_IRQ_OFFSET]);
    }

    dev = qdev_create(NULL, "bionz_meno");
    qdev_prop_set_ptr(dev, "drive_ptr", s->drive);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_MENO_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_MENO - CXD4132_IRQ_OFFSET]);

    for (i = 0; i < CXD4132_NUM_HWTIMER; i++) {
        dev = qdev_create(NULL, "bionz_hwtimer");
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD4132_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD4132_IRQ_HWTIMER(i) - CXD4132_IRQ_OFFSET]);
    }

    for (i = 0; i < CXD4132_NUM_UART; i++) {
        pl011_create(CXD4132_UART_BASE(i), irq[CXD4132_IRQ_UART(i) - CXD4132_IRQ_OFFSET], serial_hd(i));
    }

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD4132_DDR_BASE + CXD4132_TEXT_OFFSET, CXD4132_DDR_SIZE - CXD4132_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD4132_DDR_BASE + CXD4132_INITRD_OFFSET, CXD4132_DDR_SIZE - CXD4132_INITRD_OFFSET);
        cxd_init_cmdline(CXD4132_CMDLINE, machine->kernel_cmdline, CXD4132_DDR_BASE + CXD4132_CMDLINE_OFFSET);
        s->loader_base = CXD4132_DDR_BASE + CXD4132_TEXT_OFFSET;

        uint32_t boot_device = 1;
        rom_add_blob_fixed("boot_device", &boot_device, sizeof(boot_device), CXD4132_DDR_BASE + CXD4132_BOOT_DEVICE_OFFSET);
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD4132_BOOTROM_BASE, CXD4132_BOOTROM_SIZE);
        s->loader_base = CXD4132_BOOTROM_BASE;
    } else if (s->drive) {
        s->loader_base = cxd_init_loader2(s->drive);
    }

    cxd_add_const_reg("miscctrl_readdone", CXD4132_MISCCTRL_BASE(1), 1);
    cxd_add_const_reg("miscctrl_typeid", CXD4132_MISCCTRL_BASE(2), 0x301);

    qemu_register_reset(cxd_reset, s);
}

static void cxd90014_init(MachineState *machine)
{
    CxdState *s = CXD(machine);
    DriveInfo *dinfo;
    MemoryRegion *mem;
    DeviceState *dev;
    qemu_irq irq[CXD90014_NUM_IRQ - CXD90014_IRQ_OFFSET];
    qemu_irq boss_irq;
    int i;

    dinfo = drive_get(IF_MTD, 0, 0);
    s->drive = dinfo ? blk_by_legacy_dinfo(dinfo) : NULL;

    object_initialize(&s->cpu, sizeof(s->cpu), machine->cpu_type);
    object_property_set_bool(OBJECT(&s->cpu), false, "has_el3", &error_fatal);
    qdev_init_nofail(DEVICE(&s->cpu));

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "ddr", CXD90014_DDR_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90014_DDR_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "sram", CXD90014_SRAM_SIZE, &error_fatal);
    memory_region_add_subregion(get_system_memory(), CXD90014_SRAM_BASE, mem);

    mem = g_new(MemoryRegion, 1);
    memory_region_init_ram(mem, NULL, "bootrom", CXD90014_BOOTROM_SIZE, &error_fatal);
    memory_region_set_readonly(mem, true);
    memory_region_add_subregion(get_system_memory(), CXD90014_BOOTROM_BASE, mem);

    dev = qdev_create(NULL, TYPE_A9MPCORE_PRIV);
    qdev_prop_set_uint32(dev, "num-cpu", 1);
    qdev_prop_set_uint32(dev, "num-irq", CXD90014_NUM_IRQ);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_MPCORE_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, qdev_get_gpio_in(DEVICE(&s->cpu), ARM_CPU_IRQ));
    for (i = 0; i < CXD90014_NUM_IRQ - CXD90014_IRQ_OFFSET; i++) {
        irq[i] = qdev_get_gpio_in(dev, i);
    }

    dev = qdev_create(NULL, "bionz_boss");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_BOSS_SRAM_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_BOSS_IO_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 2, CXD90014_BOSS_CLKRST_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_BOSS - CXD90014_IRQ_OFFSET]);
    boss_irq = qdev_get_gpio_in(dev, 0);

    dev = qdev_create(NULL, "bionz_nand");
    qdev_prop_set_drive(dev, "drive", s->drive, &error_fatal);
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_NAND_REG_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_NAND_DATA_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, boss_irq);

    dev = qdev_create(NULL, "bionz_bootcon");
    qdev_prop_set_chr(dev, "chardev", serial_hd(0));
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_BOOTCON_BASE);

    dev = qdev_create(NULL, "fujitsu_usb");
    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_USB_BASE);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 1, CXD90014_USB_HDMAC_BASE);
    sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_USB - CXD90014_IRQ_OFFSET]);

    for (i = 0; i < CXD90014_NUM_UART; i++) {
        pl011_create(CXD90014_UART_BASE(i), irq[CXD90014_IRQ_UART(i) - CXD90014_IRQ_OFFSET], serial_hd(i));
    }

    for (i = 0; i < CXD90014_NUM_HWTIMER; i++) {
        dev = qdev_create(NULL, "bionz_hwtimer");
        qdev_init_nofail(dev);
        sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, CXD90014_HWTIMER_BASE(i));
        sysbus_connect_irq(SYS_BUS_DEVICE(dev), 0, irq[CXD90014_IRQ_HWTIMER(i) - CXD90014_IRQ_OFFSET]);
    }

    if (machine->kernel_filename) {
        load_image_targphys(machine->kernel_filename, CXD90014_DDR_BASE + CXD90014_TEXT_OFFSET, CXD90014_DDR_SIZE - CXD90014_TEXT_OFFSET);
        load_image_targphys(machine->initrd_filename, CXD90014_DDR_BASE + CXD90014_INITRD_OFFSET, CXD90014_DDR_SIZE - CXD90014_INITRD_OFFSET);
        s->loader_base = CXD90014_DDR_BASE + CXD90014_TEXT_OFFSET;
    } else if (bios_name) {
        load_image_targphys(bios_name, CXD90014_BOOTROM_BASE, CXD90014_BOOTROM_SIZE);
        s->loader_base = CXD90014_BOOTROM_BASE;
    }

    cxd_add_const_reg("miscctrl_typeid", CXD90014_MISCCTRL_BASE(0), 0x500);
    cxd_add_const_reg("miscctrl_mode", CXD90014_MISCCTRL_BASE(1), 0x0c010003);

    cxd_add_const_reg("ddmc_ctl_int_status", CXD90014_DDMC_BASE + 0x128, 0x10);

    cxd_add_const_reg("fusb_otg_usb_id_ext", CXD90014_USB_OTG_BASE + 0x10, 2);

    qemu_register_reset(cxd_reset, s);
}

static const TypeInfo cxd_info = {
    .name          = TYPE_CXD,
    .parent        = TYPE_MACHINE,
    .abstract      = true,
    .instance_size = sizeof(CxdState),
};

static void cxd_register_type(void)
{
    type_register_static(&cxd_info);
}

type_init(cxd_register_type)

static void cxd4115_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);

    mc->desc = "Sony BIONZ CXD4115";
    mc->init = cxd4115_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm11mpcore");
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo cxd4115_info = {
    .name          = MACHINE_TYPE_NAME("cxd4115"),
    .parent        = TYPE_CXD,
    .class_init    = cxd4115_class_init,
};

static void cxd4115_register_type(void)
{
    type_register_static(&cxd4115_info);
}

type_init(cxd4115_register_type)

static void cxd4132_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);

    mc->desc = "Sony BIONZ CXD4132";
    mc->init = cxd4132_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm11mpcore");
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo cxd4132_info = {
    .name          = MACHINE_TYPE_NAME("cxd4132"),
    .parent        = TYPE_CXD,
    .class_init    = cxd4132_class_init,
};

static void cxd4132_register_type(void)
{
    type_register_static(&cxd4132_info);
}

type_init(cxd4132_register_type)

static void cxd90014_class_init(ObjectClass *klass, void *data)
{
    MachineClass *mc = MACHINE_CLASS(klass);

    mc->desc = "Sony BIONZ CXD90014";
    mc->init = cxd90014_init;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-a9");
    mc->max_cpus = 2;
    mc->default_cpus = 2;// main + boss
    mc->ignore_memory_transaction_failures = true;
}

static const TypeInfo cxd90014_info = {
    .name          = MACHINE_TYPE_NAME("cxd90014"),
    .parent        = TYPE_CXD,
    .class_init    = cxd90014_class_init,
};

static void cxd90014_register_type(void)
{
    type_register_static(&cxd90014_info);
}

type_init(cxd90014_register_type)
