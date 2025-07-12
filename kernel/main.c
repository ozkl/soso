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
#include "log.h"
#include "ramdisk.h"
#include "fatfilesystem.h"
#include "vbe.h"
#include "fifobuffer.h"
#include "gfx.h"
#include "mouse.h"
#include "sleep.h"
#include "console.h"
#include "kernelterminal.h"
#include "socket.h"

extern uint32_t _start;
extern uint32_t _kernel_end;
uint32_t g_physical_kernel_start_address = (uint32_t)&_start;
uint32_t g_physical_kernel_end_address = (uint32_t)&_kernel_end;
uint32_t g_modules_end_physical = 0;

uint32_t g_kern_heap_begin = 0;

static void* locate_initrd(struct Multiboot *mbi, uint32_t* size)
{
    if (mbi->mods_count > 0)
    {
        uint32_t start_location = *((uint32_t*)mbi->mods_addr);
        uint32_t end_location = *(uint32_t*)(mbi->mods_addr + 4);

        *size = end_location - start_location;

        return (void*)start_location;
    }

    return NULL;
}

int execute_file(const char *path, char *const argv[], char *const envp[], FileSystemNode* tty)
{
    int result = -1;

    Process* process = thread_get_current()->owner;
    if (process)
    {
        FileSystemNode* node = fs_get_node_absolute_or_relative(path, process);
        if (node)
        {
            File* f = fs_open(node, 0);
            if (f)
            {
                void* image = kmalloc(node->length);

                int32_t bytes_read = fs_read(f, node->length, image);

                if (bytes_read > 0)
                {
                    char* name = "userProcess";
                    if (NULL != argv && NULL != argv[0])
                    {
                        name = argv[0];
                    }
                    Process* new_process = process_create_from_elf_data(name, image, argv, envp, process, tty);

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

    descriptor_tables_initialize();

    serial_initialize();

    uint32_t initrd_size = 0;
    uint8_t* initrd_location = locate_initrd(mboot_ptr, &initrd_size);
    uint8_t* initrd_end_location = initrd_location + initrd_size;
    if (initrd_location)
    {
        g_modules_end_physical = (uint32_t)initrd_end_location;
    }

    uint32_t memory_kb = mboot_ptr->mem_upper;
    vmm_initialize(memory_kb);

    struct Multiboot* mboot_clone = kmalloc(sizeof(struct Multiboot));
    memcpy((uint8_t*)mboot_clone, (uint8_t*)mboot_ptr, sizeof(struct Multiboot));
    mboot_clone->cmdline = (uint32_t)kmalloc(strlen((char*)mboot_ptr->cmdline) + 1);
    strcpy((char*)mboot_clone->cmdline, (char*)mboot_ptr->cmdline);
    mboot_ptr = mboot_clone;
    unmap_first_4m();
    

    fs_initialize();
    devfs_initialize();

    BOOL graphics_mode = (MULTIBOOT_FRAMEBUFFER_TYPE_RGB == mboot_ptr->framebuffer_type);

    if (graphics_mode)
    {
        gfx_initialize((uint32_t*)(uint32_t)mboot_ptr->framebuffer_addr, mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp / 8, mboot_ptr->framebuffer_pitch);
    }

    tasking_initialize();
    
    console_initialize(graphics_mode);

    print_ascii_art();

    printkf("Kernel built on %s %s\n", __DATE__, __TIME__);
    printkf("Memory initialized for %d MB\n", memory_kb / 1024);
    printkf("Kernel resides in %x - [code] - %x\n", g_physical_kernel_start_address, g_physical_kernel_end_address);
    printkf("Modules End(Physical): %x (Index 4M:%d)\n", g_modules_end_physical, PAGE_INDEX_4M(g_modules_end_physical));
    printkf("Kernel Heap: %x - %x\n", g_kern_heap_begin, KERN_HEAP_END);
    printkf("Video: %dx%dx%d - %x mapped to %x\n", mboot_ptr->framebuffer_width, mboot_ptr->framebuffer_height, mboot_ptr->framebuffer_bpp, GFX_MEMORY, (uint32_t)mboot_ptr->framebuffer_addr);

    systemfs_initialize();
    pipe_initialize();
    sharedmemory_initialize();

    syscalls_initialize();

    timer_initialize();

    keyboard_initialize();
    initialize_mouse();

    if (0 != mboot_ptr->cmdline)
    {
        printkf("Kernel cmdline:%s\n", (char*)mboot_ptr->cmdline);
    }

    serial_initialize_file_device();

    //if (0 != mboot_ptr->cmdline && strstr((char*)mboot_ptr->cmdline, "logserial"))
    {
        log_initialize(NULL);
    }
    //else
    {
        //log_initialize("/dev/ptty9");
    }

    log_printf("Kernel built on %s %s\n", __DATE__, __TIME__);

    random_initialize();
    null_initialize();

    fatfs_initialize();

    net_initialize();

    printkf("System started!\n");

    char* argv[] = {"shell", NULL};
    char* envp[] = {"HOME=/", "PATH=/initrd", NULL};

    if (initrd_location == NULL)
    {
        PANIC("Initrd not found!\n");
    }
    else
    {
        printkf("Initrd found at %x - %x (%d bytes)\n", initrd_location, initrd_end_location, initrd_size);
        //now initrd_location is accesible as initrd_location + KERNEL_VIRTUAL_BASE
        ramdisk_create("ramdisk1", initrd_size, initrd_location + KERNEL_VIRTUAL_BASE);
        //memcpy((uint8_t*)*(uint32_t*)fs_get_node("/dev/ramdisk1")->private_node_data, initrd_location, initrd_size);
        BOOL mountSuccess = fs_mount("/dev/ramdisk1", "/initrd", "fat", 0, 0);

        if (mountSuccess)
        {
            printkf("Starting shell on TTYs\n");

            //execute_file("/initrd/test_fork", argv, envp, fs_get_node("/dev/ptty1"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty1"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty2"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty3"));
            execute_file("/initrd/shell", argv, envp, fs_get_node("/dev/ptty4"));

            /*
            FileSystemNode* tty_node_x = fs_get_node("/dev/ptty7");
            Terminal* terminal_x = console_get_terminal_by_slave(tty_node_x);
            if (terminal_x)
            {
                terminal_x->disabled = TRUE;

                console_set_active_terminal(terminal_x);
            }
            execute_file("/initrd/nano-X", argv, envp, tty_node_x);
            execute_file("/initrd/tasks", argv, envp, fs_get_node("/dev/null"));
            */
        }
        else
        {
            printkf("Mounting initrd failed!\n");
        }
    }

    pipe_create("pipe0", 8);

    scheduler_enable();

    enable_interrupts();

    while(TRUE)
    {
        //Idle thread

        halt();
    }

    return 0;
}
