#include "process.h"
#include "common.h"
#include "alloc.h"
#include "vmm.h"
#include "descriptortables.h"
#include "elf.h"
#include "log.h"
#include "isr.h"
#include "timer.h"
#include "message.h"
#include "list.h"
#include "ttydev.h"
#include "sharedmemory.h"

#define MESSAGE_QUEUE_SIZE 64

#define SIGNAL_QUEUE_SIZE 64

#define AUX_VECTOR_SIZE_BYTES (AUX_CNT * sizeof (Elf32_auxv_t))

Process* g_kernel_process = NULL;

Thread* g_first_thread = NULL;
Thread* g_current_thread = NULL;
Thread* g_previous_scheduled_thread = NULL;

Thread* g_destroyed_thread = NULL;

uint32_t g_process_id_generator = 0;
uint32_t g_thread_id_generator = 0;

uint32_t g_system_context_switch_count = 0;
uint32_t g_usage_mark_point = 0;

extern Tss g_tss;

static void fill_auxilary_vector(uint32_t location, void* elfData);

uint32_t generate_process_id()
{
    return g_process_id_generator++;
}

uint32_t generate_thread_id()
{
    return g_thread_id_generator++;
}

uint32_t get_system_context_switch_count()
{
    return g_system_context_switch_count;
}

void tasking_initialize()
{
    Process* process = (Process*)kmalloc(sizeof(Process));
    memset((uint8_t*)process, 0, sizeof(Process));
    strcpy(process->name, "[idle]");
    process->pid = generate_process_id();
    process->pd = g_kernel_page_directory_physical;
    process->working_directory = fs_get_root_node();

    g_kernel_process = process;


    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8_t*)thread, 0, sizeof(Thread));

    thread->owner = g_kernel_process;

    thread->threadId = generate_thread_id();

    thread->user_mode = 0;
    thread_resume(thread);
    thread->birth_time = get_uptime_milliseconds();

    thread->message_queue = fifobuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    spinlock_init(&(thread->message_queue_lock));

    thread->signals = fifobuffer_create(SIGNAL_QUEUE_SIZE);

    thread->regs.cr3 = process->pd;

    uint32_t selector = 0x10;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x08;
    thread->regs.eip = NULL;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector;

    thread->regs.esp = 0; //no need because this is already main kernel thread. ESP will written to this in first schedule.

    thread->tls_base = 0;
    thread->tls_limit = 0xFFFFFFFF;
    thread->tls_access = 0xF2;
    thread->tls_flags = 0xC;

    thread->kstack.ss0 = 0x10;
    thread->kstack.esp0 = 0;//For kernel threads, this is not required


    g_first_thread = thread;
    g_current_thread = thread;
}

void thread_create_kthread(Function0 func)
{
    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8_t*)thread, 0, sizeof(Thread));

    thread->owner = g_kernel_process;

    thread->threadId = generate_thread_id();

    thread->user_mode = 0;

    thread_resume(thread);

    thread->birth_time = get_uptime_milliseconds();

    thread->message_queue = fifobuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    spinlock_init(&(thread->message_queue_lock));

    thread->signals = fifobuffer_create(SIGNAL_QUEUE_SIZE);

    thread->regs.cr3 = thread->owner->pd;


    uint32_t selector = 0x10;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x08;
    thread->regs.eip = (uint32_t)func;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector;

    thread->tls_base = 0;
    thread->tls_limit = 0xFFFFFFFF;
    thread->tls_access = 0xF2;
    thread->tls_flags = 0xC;

    uint8_t* stack = (uint8_t*)kmalloc(KERN_STACK_SIZE);

    thread->regs.esp = (uint32_t)(stack + KERN_STACK_SIZE - 4);

    thread->kstack.ss0 = 0x10;
    thread->kstack.esp0 = 0;//For kernel threads, this is not required
    thread->kstack.stack_start = (uint32_t)stack;

    Thread* p = g_current_thread;

    while (p->next != NULL)
    {
        p = p->next;
    }

    p->next = thread;
}

static int get_string_array_item_count(char *const array[])
{
    if (NULL == array)
    {
        return 0;
    }

    int i = 0;
    const char* a = array[0];
    while (NULL != a)
    {
        a = array[++i];
    }

    return i;
}

static char** clone_string_array(char *const array[])
{
    int item_count = get_string_array_item_count(array);

    char** new_array = kmalloc(sizeof(char*) * (item_count + 1));

    for (int i = 0; i < item_count; ++i)
    {
        const char* str = array[i];
        int len = strlen(str);

        char* newStr = kmalloc(len + 1);
        strcpy(newStr, str);

        new_array[i] = newStr;
    }

    new_array[item_count] = NULL;

    return new_array;
}

static void destroy_string_array(char** array)
{
    char* a = array[0];

    int i = 0;

    while (NULL != a)
    {
        kfree(a);

        a = array[++i];
    }

    kfree(array);
}

//This function must be called within the correct page directory for target process
static void copy_argv_env_to_process(uint32_t location, void* elfData, char *const argv[], char *const envp[])
{
    char** start_location = (char**)location;
    char** destination = start_location;
    int destination_index = 0;


    int argv_count = get_string_array_item_count(argv);
    int envp_count = get_string_array_item_count(envp);


    uint32_t* dest = (uint32_t*)destination;
    *dest = (uint32_t)argv_count; //writing argc at stack pointer
    dest = dest + 1;

    //TODO: make 800 better
    char* string_table = (char*)(location + 800);

    destination = (char**)(dest);

    for (int i = 0; i < argv_count; ++i)
    {
        strcpy(string_table, argv[i]);

        destination[destination_index] = string_table;

        string_table += strlen(argv[i]) + 2;

        destination_index++;
    }

    destination[destination_index++] = NULL;

    for (int i = 0; i < envp_count; ++i)
    {
        strcpy(string_table, envp[i]);

        destination[destination_index] = string_table;

        string_table += strlen(envp[i]) + 2;

        destination_index++;
    }

    destination[destination_index++] = NULL;

    fill_auxilary_vector((uint32_t)&destination[destination_index], elfData);
}

static void fill_auxilary_vector(uint32_t location, void* elf_data)
{
    Elf32_auxv_t* auxv = (Elf32_auxv_t*)location;

    //printkf("auxv:%x\n", auxv);

    memset((uint8_t*)auxv, 0, AUX_VECTOR_SIZE_BYTES);

    Elf32_Ehdr *hdr = (Elf32_Ehdr *) elf_data;
    Elf32_Phdr *p_entry = (Elf32_Phdr *) (elf_data + hdr->e_phoff);

    auxv[0].a_type = AT_HWCAP2;
    auxv[0].a_un.a_val = 0;

    auxv[1].a_type = AT_IGNORE;
    auxv[1].a_un.a_val = 0;

    auxv[2].a_type = AT_EXECFD;
    auxv[2].a_un.a_val = 0;

    auxv[3].a_type = AT_PHDR;
    auxv[3].a_un.a_val = elf_compute_phdr_runtime(elf_data);

    auxv[4].a_type = AT_PHENT;
    auxv[4].a_un.a_val = sizeof(Elf32_Phdr);//(uint32_t)p_entry->p_memsz;

    auxv[5].a_type = AT_PHNUM;
    auxv[5].a_un.a_val = hdr->e_phnum;

    auxv[6].a_type = AT_PAGESZ;
    auxv[6].a_un.a_val = PAGESIZE_4K;

    auxv[7].a_type = AT_BASE;
    auxv[7].a_un.a_val = 0;

    auxv[8].a_type = AT_FLAGS;
    auxv[8].a_un.a_val = 0;

    auxv[9].a_type = AT_ENTRY;
    auxv[9].a_un.a_val = (uint32_t)hdr->e_entry;

    auxv[10].a_type = AT_NOTELF;
    auxv[10].a_un.a_val = 0;

    auxv[11].a_type = AT_UID;
    auxv[11].a_un.a_val = 0;

    auxv[12].a_type = AT_EUID;
    auxv[12].a_un.a_val = 0;

    auxv[13].a_type = AT_GID;
    auxv[13].a_un.a_val = 0;

    auxv[14].a_type = AT_EGID;
    auxv[14].a_un.a_val = 0;

    auxv[15].a_type = AT_CLKTCK;
    auxv[15].a_un.a_val = 100;

    auxv[16].a_type = AT_PLATFORM;
    auxv[16].a_un.a_val = 0;

    auxv[17].a_type = AT_SECURE;
    auxv[17].a_un.a_val = 0;

    auxv[18].a_type = AT_NULL;
    auxv[18].a_un.a_val = 0;
}

Process* process_create_from_elf_data(const char* name, uint8_t* elf_data, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    return process_create_ex(name, generate_process_id(), generate_thread_id(), NULL, elf_data, argv, envp, parent, tty);
}

Process* process_create_from_function(const char* name, Function0 func, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    return process_create_ex(name, generate_process_id(), generate_thread_id(), func, NULL, argv, envp, parent, tty);
}

static void allocate_stack(Process* process)
{
    const uint32_t stack_page_count = 250; //1MB stack
    char* v_address_stack_page = (char *) (USER_STACK - PAGESIZE_4K * stack_page_count);
    uint32_t stack_frames[stack_page_count];
    for (uint32_t i = 0; i < stack_page_count; ++i)
    {
        stack_frames[i] = vmm_acquire_page_frame_4k();
    }
    void* stack_v_mem = vmm_map_memory(process, (uint32_t)v_address_stack_page, stack_frames, stack_page_count, TRUE);
    if (NULL == stack_v_mem)
    {
        for (uint32_t i = 0; i < stack_page_count; ++i)
        {
            vmm_release_page_frame_4k(stack_frames[i]);
        }
    }
}

static void allocate_args_env_aux(Process* process, uint8_t* elf_data, char *const argv[], char *const envp[])
{
    const uint32_t page_count = 1;
    char* v_address_args_env_aux = (char *) (USER_STACK);
    uint32_t p_address_args_env_aux[page_count];
    for (uint32_t i = 0; i < page_count; ++i)
    {
        p_address_args_env_aux[i] = vmm_acquire_page_frame_4k();
    }
    void* mapped = vmm_map_memory(process, (uint32_t)v_address_args_env_aux, p_address_args_env_aux, page_count, TRUE);
    if (NULL == mapped)
    {
        for (uint32_t i = 0; i < page_count; ++i)
        {
            vmm_release_page_frame_4k(p_address_args_env_aux[i]);
        }
    }
    else
    {
        copy_argv_env_to_process(USER_STACK, elf_data, argv, envp);
    }
}

Process* process_create_ex(const char* name, uint32_t process_id, uint32_t thread_id, Function0 func, uint8_t* elf_data, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    if (elf_data)
    {
        if (!elf_is_valid((const char*)elf_data))
        {
            log_printf("ELF file is not valid!\n");
            return NULL;
        }

        if (!elf_is_static((const char*)elf_data))
        {
            log_printf("ELF file is not a static binary!\n");
            return NULL;
        }

        if (!elf_is_elf32_x86((const char*)elf_data))
        {
            log_printf("ELF file is not 32bit x86 binary!\n");
            return NULL;
        }
    }
    uint32_t image_data_begin_in_memory = elf_get_begin_in_memory((char*)elf_data);
    uint32_t image_data_end_in_memory = elf_get_end_in_memory((char*)elf_data);
    uint32_t image_size_in_memory = image_data_end_in_memory - image_data_begin_in_memory;

    if (image_data_begin_in_memory >= image_data_end_in_memory)
    {
        printkf("Could not start the process %s. Image's memory location is wrong! %x-%x\n", name, image_data_begin_in_memory, image_data_end_in_memory);
        return NULL;
    }

    if (image_data_end_in_memory > KERNEL_VIRTUAL_BASE)
    {
        printkf("Could not start the process %s. Image's memory location is wrong! %x-%x\n", name, image_data_begin_in_memory, image_data_end_in_memory);
        return NULL;
    }

    if (0 == process_id)
    {
        process_id = generate_process_id();
    }

    if (0 == thread_id)
    {
        thread_id = generate_thread_id();
    }

    Process* process = (Process*)kmalloc(sizeof(Process));
    memset((uint8_t*)process, 0, sizeof(Process));
    strncpy(process->name, name, SOSO_PROCESS_NAME_MAX);
    process->name[SOSO_PROCESS_NAME_MAX - 1] = 0;
    process->pid = process_id;
    process->working_directory = fs_get_root_node();

    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8_t*)thread, 0, sizeof(Thread));

    thread->owner = process;

    thread->threadId = thread_id;

    thread->user_mode = 1;

    thread_resume(thread);

    thread->birth_time = get_uptime_milliseconds();

    thread->message_queue = fifobuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    spinlock_init(&(thread->message_queue_lock));

    thread->signals = fifobuffer_create(SIGNAL_QUEUE_SIZE);

    if (parent)
    {
        process->parent = parent;

        process->working_directory = parent->working_directory;

        process->tty = parent->tty;

        process->pgid = parent->pgid;
    }


    if (process->pgid == 0)
    {
        process->pgid = process->pid;
    }

    if (tty)
    {
        process->tty = tty;
    }

    //clone to kernel space since we are changing page directory soon
    char** new_argv = clone_string_array(argv);
    char** new_envp = clone_string_array(envp);

    uint32_t pd = vmm_acquire_page_directory();

    if (0 == pd)
    {
        printkf("Failed to create page directory for new process!\n");

        destroy_string_array(new_argv);
        destroy_string_array(new_envp);

        fifobuffer_destroy(thread->signals);
        fifobuffer_destroy(thread->message_queue);

        kfree(thread);
        kfree(process);

        return NULL;
    }
    
    process->pd = pd;
    thread->regs.cr3 = process->pd;

    //Change memory view (page directory)
    CHANGE_PD(process->pd);

    vmm_initialize_process_pages(process);

    
    //printkf("image size_in_memory:%d\n", image_size_in_memory);

    initialize_program_break(process, USER_BRK_START, 0);

    allocate_stack(process);

    allocate_args_env_aux(process, elf_data, new_argv, new_envp);

    destroy_string_array(new_argv);
    destroy_string_array(new_envp);

    uint32_t selector = 0x23;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x1B;
    thread->regs.eip = (uint32_t)func;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector;

    thread->tls_base = 0;
    thread->tls_limit = 0xFFFFFFFF;
    thread->tls_access = 0xF2;
    thread->tls_flags = 0xC;

    uint32_t stack_pointer = USER_STACK;
    thread->regs.esp = stack_pointer;

    thread->kstack.ss0 = 0x10;
    uint8_t* stack = (uint8_t*)kmalloc(KERN_STACK_SIZE);
    thread->kstack.esp0 = (uint32_t)(stack + KERN_STACK_SIZE);
    thread->kstack.stack_start = (uint32_t)stack;

    Thread* p = g_current_thread;

    while (p->next != NULL)
    {
        p = p->next;
    }

    p->next = thread;

    if (elf_data)
    {
        uint32_t start_location = elf_map_load(process, (char*)elf_data);

        //printkf("process start location:%x\n", start_location);

        if (start_location > 0)
        {
            thread->regs.eip = start_location;
        }
    }

    //Restore memory view (page directory)
    CHANGE_PD(g_current_thread->regs.cr3);

    fs_open_for_process(thread, process->tty, 0);//0: standard input
    fs_open_for_process(thread, process->tty, 0);//1: standard output
    fs_open_for_process(thread, process->tty, 0);//2: standard error

    return process;
}

Process * process_fork(Thread *parent_thread)
{
    Process* parent = parent_thread->owner;

    uint32_t pd = vmm_clone_page_directory_with_memory2();

    if (0 == pd)
    {
        return NULL;
    }

    uint32_t process_id = generate_process_id();
    uint32_t thread_id = generate_thread_id();

    Process* process = (Process*)kmalloc(sizeof(Process));
    memset((uint8_t*)process, 0, sizeof(Process));
    strcpy(process->name, parent->name);
    process->pid = process_id;
    process->pd = pd;

    Thread* child_thread = (Thread*)kmalloc(sizeof(Thread));
    memcpy((uint8_t*)child_thread, (uint8_t*)parent_thread, sizeof(Thread));
    child_thread->next = NULL;
    child_thread->owner = process;
    child_thread->threadId = thread_id;
    child_thread->user_mode = 1;
    thread_resume(child_thread);
    child_thread->birth_time = get_uptime_milliseconds();

    child_thread->message_queue = fifobuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    spinlock_init(&(child_thread->message_queue_lock));

    child_thread->signals = fifobuffer_create(SIGNAL_QUEUE_SIZE);

    child_thread->regs.cr3 = process->pd;

    process->parent = parent;
    process->working_directory = parent->working_directory;
    process->pgid = parent->pgid;
    process->tty = parent->tty;

    memcpy((uint8_t*)process->mmapped_virtual_memory, (uint8_t*)parent->mmapped_virtual_memory, sizeof(parent->mmapped_virtual_memory));

    process->brk_begin = parent->brk_begin;
    process->brk_end = parent->brk_end;
    process->brk_next_unallocated_page_begin = parent->brk_next_unallocated_page_begin;

    const uint32_t trap_frame_size = sizeof(Registers);
    uint8_t* stack = (uint8_t*)kmalloc(KERN_STACK_SIZE);
    child_thread->kstack.stack_start = (uint32_t)stack;
    Registers* child_tf = (Registers*)(stack + KERN_STACK_SIZE - trap_frame_size);
    Registers* parent_tf = (Registers*)(parent_thread->kstack.esp0 - trap_frame_size);

    memcpy((uint8_t*)child_tf, (uint8_t*)parent_tf, trap_frame_size);
    child_tf->eax = 0;  // child sees fork() return 0
    child_thread->kstack.esp0 = (uint32_t)child_tf + trap_frame_size;
    child_thread->kstack.ss0 = parent_thread->kstack.ss0;
    
    child_tf->esp = (uint32_t)child_tf;

    child_tf->eip = parent_tf->eip;
    child_tf->cs = parent_tf->cs;
    child_tf->eflags = parent_tf->eflags;
    child_tf->userEsp = parent_tf->userEsp;
    child_tf->ss = parent_tf->ss;


    // ALSO sync the Registers struct fields in Thread!
    child_thread->regs.eax    = child_tf->eax;
    child_thread->regs.ecx    = child_tf->ecx;
    child_thread->regs.edx    = child_tf->edx;
    child_thread->regs.ebx    = child_tf->ebx;
    child_thread->regs.esp    = child_tf->userEsp;  // note: from usermode
    child_thread->regs.ebp    = child_tf->ebp;
    child_thread->regs.esi    = child_tf->esi;
    child_thread->regs.edi    = child_tf->edi;
    child_thread->regs.eip    = child_tf->eip;
    child_thread->regs.eflags = child_tf->eflags;

    child_thread->regs.cs = child_tf->cs;
    child_thread->regs.ss = child_tf->ss;
    child_thread->regs.ds = child_tf->ds;
    child_thread->regs.es = child_tf->es;
    child_thread->regs.fs = child_tf->fs;
    child_thread->regs.gs = child_tf->gs;


    Thread* p = g_current_thread;

    while (p->next != NULL)
    {
        p = p->next;
    }

    p->next = child_thread;

    //TODO: duplicate file descriptors
    fs_open_for_process(child_thread, process->tty, 0);//0: standard input
    fs_open_for_process(child_thread, process->tty, 0);//1: standard output
    fs_open_for_process(child_thread, process->tty, 0);//2: standard error

    return process;
}

//This function should be called in interrupts disabled state
void thread_destroy(Thread* thread)
{
    //TODO: signal the process somehow
    Thread* previous_thread = thread_get_previous(thread);
    if (NULL != previous_thread)
    {
        previous_thread->next = thread->next;

        kfree((void*)thread->kstack.stack_start);

        spinlock_lock(&(thread->message_queue_lock));
        fifobuffer_destroy(thread->message_queue);

        fifobuffer_destroy(thread->signals);

        log_printf("destroying thread %d\n", thread->threadId);

        kfree(thread);

        if (thread == g_current_thread)
        {
            g_current_thread = NULL;
        }
    }
    else
    {
        printkf("Could not find previous thread for thread %d\n", thread->threadId);
        PANIC("This should not be happened!\n");
    }
}

//This function should be called in interrupts disabled state
void process_destroy(Process* process, BOOL wake_parent)
{
    sharedmemory_unmap_for_process_all(process);
    
    Thread* thread = g_first_thread;
    Thread* previous = NULL;
    while (thread)
    {
        if (process == thread->owner)
        {
            if (NULL != previous)
            {
                previous->next = thread->next;

                kfree((void*)thread->kstack.stack_start);

                spinlock_lock(&(thread->message_queue_lock));
                fifobuffer_destroy(thread->message_queue);

                fifobuffer_destroy(thread->signals);

                log_printf("destroying thread id:%d (owner process %d)\n", thread->threadId, process->pid);

                kfree(thread);

                if (thread == g_current_thread)
                {
                    g_current_thread = NULL;
                }

                thread = previous->next;
                continue;
            }
        }

        previous = thread;
        thread = thread->next;
    }

    //Cleanup opened files
    for (int i = 0; i < SOSO_MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] != NULL)
        {
            fs_close(process->fd[i]);
        }
    }

    if (wake_parent && process->parent)
    {
        thread = g_first_thread;
        while (thread)
        {
            if (process->parent == thread->owner)
            {
                if (thread->state == TS_WAITCHILD)
                {
                    thread_resume(thread);
                }
            }

            thread = thread->next;
        }
    }

    log_printf("destroying process %d\n", process->pid);

    uint32_t physical_pd = process->pd;

    kfree(process);

    vmm_destroy_page_directory_with_memory(physical_pd);
}

void process_change_state(Process* process, ThreadState state)
{
    Thread* thread = g_first_thread;

    while (thread)
    {
        if (process == thread->owner)
        {
            thread_change_state(thread, state, NULL);
        }

        thread = thread->next;
    }
}

void thread_change_state(Thread* thread, ThreadState state, void* private_data)
{
    thread->state = state;
    thread->state_privateData = private_data;
}

void thread_resume(Thread* thread)
{
    thread->state = TS_RUN;
    thread->state_privateData = NULL;
}

//must be called in interrupts disabled
BOOL thread_signal(Thread* thread, uint8_t signal)
{
    //TODO: check for ignore mask

    BOOL result = FALSE;

    if (signal < SIGNAL_COUNT)
    {
        if (signal == SIGCONT)
        {
            if (thread->state == TS_SUSPEND)
            {
                thread->state = TS_RUN;
            }
        }

        if (fifobuffer_get_free(thread->signals) > 0)
        {
            fifobuffer_enqueue(thread->signals, &signal, 1);
            thread->pending_signal_count = fifobuffer_get_size(thread->signals);

            if (thread->state == TS_WAITIO)
            {
                thread->state = TS_RUN;
                //it should wake and it should return -EINTR
            }

            result = TRUE;
        }
    }

    return result;
}

BOOL process_signal(uint32_t pid, uint8_t signal)
{
    Thread* t = thread_get_first();

    while (t != NULL)
    {
        if (t->owner->pid == pid)
        {
            if (thread_signal(t, signal))
            {
                //only one thread should receive a signal per process!
                return TRUE;
            }
        }
        t = t->next;
    }

    return FALSE;
}

void process_signal_group(uint32_t pgrp, uint8_t signal)
{
    Thread* t = thread_get_first();

    while (t != NULL)
    {
        if (t->owner->pgid == pgrp)
        {
            thread_signal(t, signal);
        }
        t = t->next;
    }
}

void thread_state_to_string(ThreadState state, uint8_t* buffer, uint32_t buffer_size)
{
    if (buffer_size < 1)
    {
        return;
    }

    buffer[0] = '\0';

    switch (state)
    {
    case TS_RUN:
        strncpy_null((char*)buffer, "run", buffer_size);
        break;
    case TS_SLEEP:
        strncpy_null((char*)buffer, "sleep", buffer_size);
        break;
    case TS_SUSPEND:
        strncpy_null((char*)buffer, "suspend", buffer_size);
        break;
    case TS_WAITCHILD:
        strncpy_null((char*)buffer, "waitchild", buffer_size);
        break;
    case TS_WAITIO:
        strncpy_null((char*)buffer, "waitio", buffer_size);
        break;
    case TS_SELECT:
        strncpy_null((char*)buffer, "select", buffer_size);
        break;
    case TS_CRITICAL:
        strncpy_null((char*)buffer, "critical", buffer_size);
        break;
    case TS_DEAD:
        strncpy_null((char*)buffer, "dead", buffer_size);
        break;
    case TS_UNINTERRUPTIBLE:
        strncpy_null((char*)buffer, "uninterruptible", buffer_size);
        break;
    default:
        break;
    }
}

void wait_for_schedule()
{
    //Screen_PrintF("Waiting for a schedule()\n");

    enable_interrupts();
    while (TRUE)
    {
        halt();
    }
    disable_interrupts();
    PANIC("wait_for_schedule(): Should not be reached here!!!\n");
}

int32_t process_get_empty_fd(Process* process)
{
    int32_t result = -1;

    begin_critical_section();

    for (int i = 0; i < SOSO_MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] == NULL)
        {
            result = i;
            break;
        }
    }

    end_critical_section();

    return result;
}

int32_t process_add_file(Process* process, File* file)
{
    int32_t result = -1;

    begin_critical_section();

    //Screen_PrintF("process_add_file: pid:%d\n", process->pid);

    for (int i = 0; i < SOSO_MAX_OPENED_FILES; ++i)
    {
        //Screen_PrintF("process_add_file: i:%d fd[%d]:%x\n", i, i, process->fd[i]);
        if (process->fd[i] == NULL)
        {
            result = i;
            file->fd = i;
            process->fd[i] = file;
            break;
        }
    }

    end_critical_section();

    return result;
}

int32_t process_remove_file(Process* process, File* file)
{
    int32_t result = -1;

    begin_critical_section();

    for (int i = 0; i < SOSO_MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] == file)
        {
            result = i;
            process->fd[i] = NULL;
            break;
        }
    }

    end_critical_section();

    return result;
}

File* process_find_file(Process* process, FileSystemNode* node)
{
    File* result = NULL;

    for (int i = 0; i < SOSO_MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] && process->fd[i]->node == node)
        {
            result = process->fd[i];
            break;
        }
    }

    return result;
}

Thread* thread_get_by_id(uint32_t threadId)
{
    Thread* p = g_first_thread;

    while (p != NULL)
    {
        if (p->threadId == threadId)
        {
            return p;
        }
        p = p->next;
    }

    return NULL;
}

Thread* thread_get_previous(Thread* thread)
{
    Thread* t = g_first_thread;

    while (t->next != NULL)
    {
        if (t->next == thread)
        {
            return t;
        }
        t = t->next;
    }

    return NULL;
}

Thread* thread_get_first()
{
    return g_first_thread;
}

Thread* thread_get_current()
{
    return g_current_thread;
}

BOOL thread_is_valid(Thread* thread)
{
    Thread* p = g_first_thread;

    while (p != NULL)
    {
        if (p == thread)
        {
            return TRUE;
        }
        p = p->next;
    }

    return FALSE;
}

BOOL process_is_valid(Process* process)
{
    Thread* p = g_first_thread;

    while (p != NULL)
    {
        if (p->owner == process)
        {
            return TRUE;
        }
        p = p->next;
    }

    return FALSE;
}

Process * process_get(int pid)
{
    Thread *t = g_first_thread;

    while (t != NULL)
    {
        if (t->owner->pid == pid)
        {
            return t->owner;
        }
        t = t->next;
    }

    return NULL;
}

static void thread_switch_to(Thread* thread, int mode);

static void thread_update_usage_metrics()
{
    uint32_t seconds = get_uptime_seconds();

    if (seconds > g_usage_mark_point)
    {
        g_usage_mark_point = seconds;

        const uint32_t milliseconds_passed = 1000;

        Thread* t = g_first_thread;
        while (NULL != t)
        {
            uint32_t consumed_from_mark = t->consumed_cpu_time_ms - t->consumed_cpu_time_ms_at_prev_mark;

            t->usage_cpu = (100 * consumed_from_mark) / milliseconds_passed;

            t->consumed_cpu_time_ms_at_prev_mark = t->consumed_cpu_time_ms;
            
            t = t->next;
        }
    }    
}

static void thread_update_state(Thread* t)
{
    if (t->state == TS_SLEEP)
    {
        uint32_t uptime = get_uptime_milliseconds();
        uint32_t target = (uint32_t)t->state_privateData;

        if (uptime >= target || fifobuffer_get_size(t->signals) > 0)
        {
            thread_resume(t);
        }
    }
    else if (t->state == TS_SELECT)
    {
        select_update(t);

        if (t->select.select_state == SS_FINISHED)
        {
            thread_resume(t);
        }
    }
}

static Thread* look_threads(Thread* current)
{
    Thread* t = current->next;
    while (NULL != t)
    {
        thread_update_state(t);

        if (t->state == TS_RUN)
        {
            //We definetly found a good one!
            return t;
        }
        t = t->next;
    }


    //Reached the last thread. Let's try from first thread up to the current including.
    //Of course this makes sense if current is not the first one already.

    if (current == g_first_thread)
    {
        //We already searched for all threads

        //Desperately return idle thread
        return g_first_thread;
    }
    else
    {
        t = g_first_thread->next;
        while (NULL != t)
        {
            thread_update_state(t);

            if (t->state == TS_RUN)
            {
                //We definetly found a good one!
                return t;
            }

            if (t == current)
            {
                //We reached the last point to search
                break;
            }
            t = t->next;
        }
    }

    //Desperately return idle thread
    return g_first_thread;
}

static void end_context(TimerInt_Registers* registers, Thread* thread)
{
    thread->context_end_time = get_uptime_milliseconds();
    thread->consumed_cpu_time_ms += thread->context_end_time - thread->context_start_time;

    thread->regs.eflags = registers->eflags;
    thread->regs.cs = registers->cs;
    thread->regs.eip = registers->eip;
    thread->regs.eax = registers->eax;
    thread->regs.ecx = registers->ecx;
    thread->regs.edx = registers->edx;
    thread->regs.ebx = registers->ebx;
    thread->regs.ebp = registers->ebp;
    thread->regs.esi = registers->esi;
    thread->regs.edi = registers->edi;
    thread->regs.ds = registers->ds;
    thread->regs.es = registers->es;
    thread->regs.fs = registers->fs;
    thread->regs.gs = registers->gs;

    if (thread->regs.cs != 0x08)
    {
        //log_printf("schedule() - 2.1\n");
        thread->regs.esp = registers->esp_if_privilege_change;
        thread->regs.ss = registers->ss_if_privilege_change;
    }
    else
    {
        //log_printf("schedule() - 2.2\n");
        thread->regs.esp = registers->esp + 12;
        thread->regs.ss = g_tss.ss0;
    }

    //Save the TSS from the old process
    thread->kstack.ss0 = g_tss.ss0;
    thread->kstack.esp0 = g_tss.esp0;
}

static void start_context(Thread* thread)
{
    g_previous_scheduled_thread = g_current_thread;
    
    g_current_thread = thread;//Now g_current_thread is the thread we are about to schedule to

    thread->context_start_time = get_uptime_milliseconds();

    ++g_system_context_switch_count;

    ++thread->context_switch_count;

    if (thread->regs.cs != 0x08)
    {
        thread_switch_to(thread, USERMODE);
    }
    else
    {
        thread_switch_to(thread, KERNELMODE);
    }
}

void schedule(TimerInt_Registers* registers)
{
    Thread* current = g_current_thread;

    Thread* ready_thread = NULL;

    if (NULL != current)
    {
        if (current->next == NULL && current == g_first_thread)
        {
            //We are the only thread, no need to schedule
            return;
        }

        if (current->state == TS_UNINTERRUPTIBLE)
        {
            return;
        }

        end_context(registers, current);

        ready_thread = look_threads(current);
    }
    else
    {
        //current is NULL. This means the thread is destroyed.

        ready_thread = look_threads(g_first_thread);
    }

    if (ready_thread != g_first_thread)
    {
        if (ready_thread->owner->exiting)
        {
            process_destroy(ready_thread->owner, TRUE);
            ready_thread = look_threads(g_first_thread);
        }
        else if (fifobuffer_get_size(ready_thread->signals) > 0)
        {
            uint8_t signal = 0;
            fifobuffer_dequeue(ready_thread->signals, &signal, 1);
            ready_thread->pending_signal_count = fifobuffer_get_size(ready_thread->signals);

            printkf("Signal %d proccessing for pid:%d in scheduler!\n", (uint32_t)signal, ready_thread->owner->pid);

            //TODO: call signal handlers

            switch (signal)
            {
            case SIGHUP:
            case SIGTERM:
            case SIGKILL:
            case SIGSEGV:
            case SIGINT:
            case SIGILL:
                printkf("Killing pid:%d in scheduler!\n", ready_thread->owner->pid);
            
                process_destroy(ready_thread->owner, TRUE);

                ready_thread = look_threads(g_first_thread);
                break;
            case SIGSTOP:
            case SIGTSTP:
                ready_thread->state = TS_SUSPEND;

                ready_thread = look_threads(g_first_thread);
                break;
            
            default:
                break;
            }
            
        }
    }

    thread_update_usage_metrics();

    start_context(ready_thread);
}

void change_pd_and_sync(uint32_t page_directory)
{
    if (page_directory != g_kernel_page_directory_physical)
    {
        CHANGE_PD(page_directory);
        uint32_t* pd = (uint32_t*)0xFFFFF000;
        uint32_t start_index = PAGE_INDEX_4M(KERNEL_VIRTUAL_BASE);
        for (uint32_t i = start_index; i < 1023; ++i)
        {
            pd[i] = g_kernel_page_directory[i] & ~PG_OWNED;
        }
    }
}

//The mode indicates whether this process was in user mode or kernel mode
//When it was previously interrupted by the scheduler.
static void thread_switch_to(Thread* thread, int mode)
{
    uint32_t kesp, eflags;
    uint16_t kss, ss, cs;

    //Set TSS values
    g_tss.ss0 = thread->kstack.ss0;
    g_tss.esp0 = thread->kstack.esp0;

    ss = thread->regs.ss;
    cs = thread->regs.cs;
    eflags = (thread->regs.eflags | 0x200) & 0xFFFFBFFF;

    if (mode == USERMODE)
    {
        kss = thread->kstack.ss0;
        kesp = thread->kstack.esp0;
    }
    else
    {
        kss = thread->regs.ss;
        kesp = thread->regs.esp;
    }

    //sync from kernel page directory
    change_pd_and_sync(thread->owner->pd);

    set_gdt_entry(TLS_ENTRY_IDX, thread->tls_base, thread->tls_limit, thread->tls_access, thread->tls_flags);
    asm volatile("movw %0, %%gs" :: "r"((uint16_t)(TLS_SELECTOR | 3)));

    //switch_task is in task.asm

    asm("	mov %0, %%ss; \
        mov %1, %%esp; \
        cmpl %[KMODE], %[mode]; \
        je nextt; \
        push %2; \
        push %3; \
        nextt: \
        push %4; \
        push %5; \
        push %6; \
        push %7; \
        ljmp $0x08, $switch_task"
        :: \
        "m"(kss), \
        "m"(kesp), \
        "m"(ss), \
        "m"(thread->regs.esp), \
        "m"(eflags), \
        "m"(cs), \
        "m"(thread->regs.eip), \
        "m"(thread), \
        [KMODE] "i"(KERNELMODE), \
        [mode] "g"(mode)
        );
}
