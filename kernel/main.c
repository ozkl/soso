#include "descriptortables.h"
#include "timer.h"
#include "multiboot.h"
#include "fs.h"
#include "syscalls.h"
#include "serial.h"
#include "isr.h"
#include "vmm.h"
#include "alloc.h"
#include "process.h"
#include "keyboard.h"
#include "devfs.h"
#include "systemfs.h"
#include "pipe.h"
#include "sharedmemory.h"
#include "random.h"
#include "null.h"
#include "elf.h"
#include "debugprint.h"
#include "ramdisk.h"
#include "fatfilesystem.h"
#include "vbe.h"
#include "fifobuffer.h"
#include "gfx.h"
#include "mouse.h"
#include "sleep.h"
#include "console.h"

extern uint32 _start;
extern uint32 _end;
uint32 g_physical_kernel_start_address = (uint32)&_start;
uint32 g_physical_kernel_end_address = (uint32)&_end;

static void* locate_initrd(struct Multiboot *mbi, uint32* size)
{
    if (mbi->mods_count > 0)
    {
        uint32 start_location = *((uint32*)mbi->mods_addr);
        uint32 end_location = *(uint32*)(mbi->mods_addr + 4);

        *size = end_location - start_location;

        return (void*)start_location;
    }

    return NULL;
}

int execute_file(const char *path, char *const argv[], char *const envp[], FileSystemNode* tty)
{
    int result = -1;

    Process* process = get_current_thread()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);
        if (node)
        {
            File* f = fs_open(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                int32 bytes_read = fs_read(f, node->length, image);

                if (bytes_read > 0)
                {
                    char* name = "userProcess";
                    if (NULL != argv && NULL != argv[0])
                    {
                        name = argv[0];
                    }
                    Process* new_process = create_user_process_from_elf_data(name, image, argv, envp, process, tty);

                    if (new_process)
                    {
                        result = new_process->pid;
                    }
                }
                fs_close(f);

                kfree(image);
            }

        }
    }

    return result;
}

void print_ascii_art()
{
    printkf("     ________       ________      ________       ________     \n");
    printkf("    |\\   ____\\     |\\   __  \\    |\\   ____\\     |\\   __  \\    \n");
    printkf("    \\ \\  \\___|_    \\ \\  \\|\\  \\   \\ \\  \\___|_    \\ \\  \\|\\  \\   \n");
    printkf("     \\ \\_____  \\    \\ \\  \\\\\\  \\   \\ \\_____  \\    \\ \\  \\\\\\  \\  \n");
    printkf("      \\|____|\\  \\    \\ \\  \\\\\\  \\   \\|____|\\  \\    \\ \\  \\\\\\  \\ \n");
    printkf("        ____\\_\\  \\    \\ \\_______\\    ____\\_\\  \\    \\ \\_______\\\n");
    printkf("       |\\_________\\    \\|_______|   |\\_________\\    \\|_______|\n");
    printkf("       \\|_________|                 \\|_________|              \n");
    printkf("\n");
}

int kmain(struct Multiboot *mboot_ptr)
{
    int stack = 5;

    initialize_descriptor_tables();

    uint32 memory_kb = mboot_ptr->mem_upper;//96*1024;
    initialize_memory(memory_kb);

    fs_initialize();
    devfs_initialize();

    BOOL graphics_mode = (MULTIBOOT_FRAMEBUFFER_TYPE_RGB == mboot_ptr->framebuffer_type);

    if (graphics_mode)
    {
        gfx_initialize((uint32*)(uint32)mboot_ptr->framebuffer_addr, mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp / 8, mboot_ptr->framebuffer_pitch);
    }

    console_initialize(graphics_mode);

    print_ascii_art();

    printkf("Kernel built on %s %s\n", __DATE__, __TIME__);
    //printkf("Lower Memory: %d KB\n", mboot_ptr->mem_lower);
    //printkf("Upper Memory: %d KB\n", mboot_ptr->mem_upper);
    printkf("Memory initialized for %d MB\n", memory_kb / 1024);
    printkf("Kernel resides in %x - %x\n", g_physical_kernel_start_address, g_physical_kernel_end_address);
    //printkf("Initial stack: %x\n", &stack);
    printkf("Video: %x\n", (uint32)mboot_ptr->framebuffer_addr);
    printkf("Video: %dx%dx%d Pitch:%d\n", mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp, mboot_ptr->framebuffer_pitch);

    initialize_systemfs();
    pipe_initialize();
    initialize_sharedmemory();

    initialize_tasking();

    initialize_syscalls();

    timer_initialize();

    keyboard_initialize();
    initialize_mouse();

    if (0 != mboot_ptr->cmdline)
    {
        printkf("Kernel cmdline:%s\n", (char*)mboot_ptr->cmdline);
    }

    initialize_serial();

    //log_initialize("/dev/com1");
    log_initialize("/dev/ptty9");

    log_printf("Kernel built on %s %s\n", __DATE__, __TIME__);

    random_initialize();
    null_initialize();

    ramdisk_create("ramdisk1", 20*1024*1024);

    fatfs_initialize();

    printkf("System started!\n");

    char* argv[] = {"shell", NULL};
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    uint32 initrd_size = 0;
    uint8* initrd_location = locate_initrd(mboot_ptr, &initrd_size);
    uint8* initrd_end_location = initrd_location + initrd_size;
    if (initrd_location == NULL)
    {
        PANIC("Initrd not found!\n");
    }
    else
    {
        printkf("Initrd found at %x - %x (%d bytes)\n", initrd_location, initrd_end_location, initrd_size);
        if ((uint32)KERN_PD_AREA_BEGIN < (uint32)initrd_end_location)
        {
            printkf("Initrd must reside below %x !!!\n", KERN_PD_AREA_BEGIN);
            PANIC("Initrd image is too big!");
        }
        memcpy((uint8*)*(uint32*)fs_get_node("/dev/ramdisk1")->privateNodeData, initrd_location, initrd_size);
        BOOL mountSuccess = fs_mount("/dev/ramdisk1", "/initrd", "fat", 0, 0);

        if (mountSuccess)
        {
            printkf("Starting shell on TTYs\n");

            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty1"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty2"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty3"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty4"));
        }
        else
        {
            printkf("Mounting initrd failed!\n");
        }
    }

    pipe_create("pipe0", 8);

    scheduler_enable();

    enableInterrupts();

    while(TRUE)
    {
        //Idle thread

        halt();
    }

    return 0;
}
