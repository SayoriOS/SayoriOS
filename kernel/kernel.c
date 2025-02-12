/**
 * @file kernel.c
 * @author Арен Елчинян (a2.dev@yandex.com)
 * @brief Входная точка ядра, инициализация драйверов
 * @version 0.1.13
 * @date 2022-08-13
 * @copyright Copyright Арен Елчинян (c) 2022
 */


#include <kernel.h>
#include <drivers/ata.h>

#define kCMD_BOOTSCREEN_MINIMAL "--bootscreen=minimal"
#define kCMD_BOOTSCREEN_LIGHT "--bootscreen=light"
#define kCMD_BOOTSCREEN_DARK "--bootscreen=dark"
#define kCMD_EXEC_TSHELL "--tshell"
#define kCMD_NO_DRIVER_RTL8139 "--nortl8139"
#define kCMD_NO_DRIVER_BGA "--nobga"

int32_t errno = 0;
uint32_t os_mode = 1; // 0 - мало ОЗУ, 1 - обычный режим, 2 - режим повышенной производительности, 3 - сервер
bool autotshell = false;
bool rtl8139_load = true;
bool bga_load = true;

void kernelCMDHandler(char* cmd){
    qemu_log("[kCMD] '%s'",cmd);
    uint32_t kCMDc = str_cdsp(cmd," ");
    char* out[128] = {0};
    str_split(cmd,out," ");

    for(int i = 0; kCMDc >= i; i++){
        qemu_log("[kCMD] %s",out[i]);
        if (strcmpn(out[i],kCMD_BOOTSCREEN_MINIMAL)){
            bootScreenChangeMode(1);
            qemu_log("[kCMD] The minimum operating mode for BootScreen is selected.");
            continue;
        } else if (strcmpn(out[i],kCMD_EXEC_TSHELL)){
            autotshell = true;
            qemu_log("[kCMD] After loading the kernel, TShell will automatically start.");
            continue;
        } else if (strcmpn(out[i],kCMD_NO_DRIVER_BGA)){
            bga_load = false;
            qemu_log("[kCMD] The BGA driver will not be loaded on kernel startup.");
            continue;
        } else if (strcmpn(out[i],kCMD_NO_DRIVER_RTL8139)){
            rtl8139_load = false;
            qemu_log("[kCMD] The Realtek RTL8139 driver will not be loaded on kernel startup.");
            continue;
        }  else if (strcmpn(out[i],kCMD_BOOTSCREEN_LIGHT)){
            bootScreenChangeTheme(1);
            qemu_log("[kCMD] The Realtek RTL8139 driver will not be loaded on kernel startup.");
            continue;
        } else if (strcmpn(out[i],kCMD_BOOTSCREEN_DARK)){
            bootScreenChangeTheme(0);
            qemu_log("[kCMD] The Realtek RTL8139 driver will not be loaded on kernel startup.");
            continue;
        }
    }
}


/*!
	\brief Входная точка ядра SynapseOS
	\warning Отсутствует проверка multiboot!
*/
void kernel(uint32_t magic_number, struct multiboot_info *mboot_info) {
    if (magic_number != MULTIBOOT_BOOTLOADER_MAGIC) {
        qemu_log("Invalid magic number: %x, valid = %x", magic_number, MULTIBOOT_BOOTLOADER_MAGIC);
    }
    tty_init(mboot_info);   // Настройка графики

    char* kCMD = mboot_info->cmdline;
    kernelCMDHandler(kCMD);

    uint32_t VBOX_FOUND = pci_read(pci_get_device(0x80EE, 0xCAFE, -1), PCI_BAR0);
    if (VBOX_FOUND == 0){
        qemu_log("[VBox] Error...");
        return;
    }

    // Загружаем bootScreen
    bootScreenInit(10);
    bootScreenLazy(true);

    bootScreenPaint("Setting `Global Descriptor Table`...");
    gdt_init(); // Установка и настройка прерываний

    bootScreenPaint("Setting `Interrupt Descriptor Table`...");
    idt_init(); //

    bootScreenPaint("Configuring the Physical Storage Manager...");
    pmm_init(mboot_info);


    uint32_t initrd_beg = *(uint32_t*) (mboot_info->mods_addr);     // Адрес начала ramdisk
    uint32_t initrd_end = *(uint32_t*) (mboot_info->mods_addr + 4); // Адрес конца ramdisk
    qemu_log("initrd_beg = %x initrd_end = %x",
        initrd_beg, initrd_end              // Вывод информации о адресах ramdisk в отладчик
    );

    bootScreenPaint("Configuring the Virtual Memory Manager...");
    vmm_init();                             // Инициализация менеджера виртуальной памяти

    kheap_init();                           // Инициализация кучи ядра

    init_vbe(mboot_info);                   // Активация графики 1024x768

    bootScreenPaint("Setting up a virtual file system...");
    vfs_init();                             // Инициализация виртуальной файловой системы


    bootScreenPaint("Initializing a virtual disk...");
    initrd_init(initrd_beg, initrd_end);    // Инициализация ramdisk
    bootScreenLazy(true);

    bootScreenPaint("SysApiApps Configurate...");
    syscall_init();                         // Инициализация системного api для программ

    bootScreenPaint("Installing the Keyboard Driver...");
    keyboard_install();                     // Установка драйвера клавиатуры

    bootScreenPaint("Setting the Programmable Interval Timer (PIT)...");
    timer_install();                        // Установка PIT

    bootScreenPaint("Identifying PCI Devices...");
    pci_init();                             // Установка драйвера PCI

    if (bga_load){
        bootScreenPaint("Initializing the BGA video adapter...");
        bgaInit();
        bgaTest();
    }
    //bootScreenPaint("Virtual Box...");
    //vbox_guest_init();

    if (rtl8139_load){
        // Загружать ли драйвеп RTL8139
        bootScreenPaint("Installing the RTL-8139 network card driver...");
        unit_test(RTL8139_init());              // Тестируем драйвер RTL8139
    }
    vfs_mount_list();                       // Выводим список корня VFS

    bootScreenPaint("Determining the device's processor...");
    detect_cpu(1);


    bootScreenClose(0x000000,0xFFFFFF);

    if (autotshell){
        // Автоматический запуск TShell
        run_elf_file("/initrd/apps/tshell", 0, 0);
    }

    tty_printf("SayoriOS kernel version: %d.%d.%d, Built: %s\n\n",
        VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,    // Версия ядра
        __TIMESTAMP__                                   // Время окончания компиляции ядра
    );

    char * file_motd = "/initrd/etc/motd";
    FILE* motd = fopen(file_motd,"r");
    if (ferror(motd) == 0){
        char * buffer2 = fread(motd);
        fclose(motd);
        tty_printf("%s\n",buffer2);
    }

    struct synapse_time TIME = get_time();
    tty_printf("Current datetime is: %d/%d/%d %d:%d:%d\n", TIME.day, TIME.month,
    							TIME.year, TIME.hours, TIME.minutes, TIME.seconds);
    tty_puts("Experimental font demo: \xFF\x01 \xFF\x02 \xFF\x03 \xFF\x04 \xFF\x05 \xFF\x06 \xFF\x07 \xFF\x08 \xFF\x09 \xFF\x0A \xFF\x0B \xFF\x0C\n");
    tty_puts("Colors: \xFF\x0D\xFF\x0E\xFF\x0F\n");
    tty_puts_color("Circles: [\xFF\x10][\xFF\x11]\n", 0, 0xFF0000);

    /* Перед тем как раскомментировать, хорошо подумайте, это создает громкий шум вместо звука
    sb16_init();

    if(vfs_exists("/initrd/res/sound.wav")) {
        tty_printf("EXISTS!\n");

        int fsize = vfs_get_size("/initrd/res/sound.wav");
        char* fdat = kheap_malloc(fsize);
        memset(fdat, 0, fsize);
        tty_printf("FILE SIZE: %d\n", fsize);

        vfs_read("/initrd/res/sound.wav", 0, fsize, fdat);
        tty_printf("PREDATA: %d\n", fdat[1]);

        sb16_play_audio(fdat, 44100, 1, 0, 0, fsize);

        kheap_free(fdat);
    }
    */
    //setFontPath("/initrd/var/fonts/MicrosoftLuciaConsole18.duke","/initrd/var/fonts/MicrosoftLuciaConsole18.fdat");
    setFontPath("/initrd/var/fonts/MicrosoftLuciaConsole9.duke","/initrd/var/fonts/MicrosoftLuciaConsole9.fdat"); // Для 9го размера
    fontInit();

    //setConfigurationFont(18,22); // Для 18
    setConfigurationFont(9,10); // Для 9
    drawRect(0,0,800,200);
    drawFont("АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ",10,30,0);
    setColorFont(0xCAFE12);
    drawFont("абвгдеёжзийклмнопрстуфхцчшщъыьэюя",10,60,0);
    setColorFont(0x333333);
    drawFont("!«№;%:?*()_+-=@#$^&[]{}|\\/",10,90,0);
    setColorFont(0xDDDDDD);
    drawFont("QWERTYUIOPASDFGHJKLZXCVBNM",10,120,0);
    setColorFont(0xAAAAAA);
    drawFont("qwertyuiopasdfghjklzxcvbnm",10,150,0);
    setColorFont(0xFFFFFF);
    drawFont("1234567890.,",10,180,0);


    tty_printf("[ATOI] %d",atoi("1234567890"));
    shell();                                // Активация терминала
}

