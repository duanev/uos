
# cm4

### booting with start4.elf

    I've been using this config.txt:

        include distroconfig.txt
        arm_64bit=1
        #boot_uart=1
        #enable_uart=1
        #uart_2ndstage=1
        kernel hello.bin
        #kernel u-boot.bin
        #kernel kernel8.img

### booting with u-boot

    Building a useable u-boot for a cm4 is still painful, but this helped:

        $ git clone git://git.denx.de/u-boot.git
        $ make rpi_arm64_defconfig

    then patch to stop the autoboot (but still init eth0 and download a boot script):

    diff --git a/include/configs/rpi.h b/include/configs/rpi.h
    index 7a5f0851b5..9a3e4f50e1 100644
    --- a/include/configs/rpi.h
    +++ b/include/configs/rpi.h
    @@ -157,20 +157,35 @@
            #define BOOT_TARGET_DHCP(func)
     #endif

    +#define BOOT_TARGET_DEVICES(func) \
    +       BOOT_TARGET_MMC(func) \
    +       BOOT_TARGET_DHCP(func)
    +
    +/*
     #define BOOT_TARGET_DEVICES(func) \
            BOOT_TARGET_MMC(func) \
            BOOT_TARGET_USB(func) \
            BOOT_TARGET_PXE(func) \
            BOOT_TARGET_DHCP(func)
    +*/
    +
    +//#include <config_distro_bootcmd.h>
     
    -#include <config_distro_bootcmd.h>
    +#define BOOTENV \
    +        "distro_bootcmd=dhcp; source 0x1000000\0"
     
    +#define CONFIG_EXTRA_ENV_SETTINGS \
    +       ENV_DEVICE_SETTINGS \
    +       ENV_MEM_LAYOUT_SETTINGS \
    +       BOOTENV
    +
    +/*
     #define CONFIG_EXTRA_ENV_SETTINGS \
            "dhcpuboot=usb start; dhcp u-boot.uimg; bootm\0" \
            ENV_DEVICE_SETTINGS \
            ENV_DFU_SETTINGS \
            ENV_MEM_LAYOUT_SETTINGS \
            BOOTENV
    -
    +*/


Compile with:

        $ CROSS_COMPILE=aarch64-linux-gnu- make -j12 all

Create an example script:

        setenv serverip 192.168.1.10
        setenv loadaddr 0x80000
        setenv bootcmd
        echo ready

which can be generated and placed with:

        $ mkimage -c none -A arm -T script -d myboot.script /tftp/boot.scr.uimg

to finally allow:

        U-Boot> tftpboot hello.bin
        U-Boot> go $loadaddr

