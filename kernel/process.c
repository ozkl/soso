#include "process.h"
#include "common.h"
#include "alloc.h"
#include "vmm.h"
#include "descriptortables.h"
#include "elf.h"
#include "debugprint.h"
#include "isr.h"
#include "timer.h"
#include "message.h"
#include "list.h"
#include "ttydev.h"

#define MESSAGE_QUEUE_SIZE 64

#define SIGNAL_QUEUE_SIZE 64

#define AUX_VECTOR_SIZE_BYTES (AUX_CNT * sizeof (Elf32_auxv_t))

Process* gKernelProcess = NULL;

Thread* gFirstThread = NULL;
Thread* gCurrentThread = NULL;

Thread* gDestroyedThread = NULL;

uint32 gProcessIdGenerator = 0;
uint32 gThreadIdGenerator = 0;

uint32 gSystemContextSwitchCount = 0;
uint32 gUsageMarkPoint = 0;

extern Tss gTss;

static void fillAuxilaryVector(uint32 location, void* elfData);

uint32 generateProcessId()
{
    return gProcessIdGenerator++;
}

uint32 generateThreadId()
{
    return gThreadIdGenerator++;
}

uint32 getSystemContextSwitchCount()
{
    return gSystemContextSwitchCount;
}

void initializeTasking()
{
    Process* process = (Process*)kmalloc(sizeof(Process));
    memset((uint8*)process, 0, sizeof(Process));
    strcpy(process->name, "[kernel]");
    process->pid = generateProcessId();
    process->pd = (uint32*) KERN_PAGE_DIRECTORY;
    process->workingDirectory = getFileSystemRootNode();

    gKernelProcess = process;


    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8*)thread, 0, sizeof(Thread));

    thread->owner = gKernelProcess;

    thread->threadId = generateThreadId();

    thread->userMode = 0;
    resumeThread(thread);
    thread->birthTime = getUptimeMilliseconds();

    thread->messageQueue = FifoBuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    Spinlock_Init(&(thread->messageQueueLock));

    thread->signals = FifoBuffer_create(SIGNAL_QUEUE_SIZE);

    thread->regs.cr3 = (uint32) process->pd;

    uint32 selector = 0x10;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x08;
    thread->regs.eip = NULL;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector;

    thread->regs.esp = 0; //no need because this is already main kernel thread. ESP will written to this in first schedule.

    thread->kstack.ss0 = 0x10;
    thread->kstack.esp0 = 0;//For kernel threads, this is not required


    gFirstThread = thread;
    gCurrentThread = thread;
}

void createKernelThread(Function0 func)
{
    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8*)thread, 0, sizeof(Thread));

    thread->owner = gKernelProcess;

    thread->threadId = generateThreadId();

    thread->userMode = 0;

    resumeThread(thread);

    thread->birthTime = getUptimeMilliseconds();

    thread->messageQueue = FifoBuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    Spinlock_Init(&(thread->messageQueueLock));

    thread->signals = FifoBuffer_create(SIGNAL_QUEUE_SIZE);

    thread->regs.cr3 = (uint32) thread->owner->pd;


    uint32 selector = 0x10;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x08;
    thread->regs.eip = (uint32)func;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector;

    uint8* stack = (uint8*)kmalloc(KERN_STACK_SIZE);

    //thread->miniAllocations[0] = (uint32)stack;

    thread->regs.esp = (uint32)(stack + KERN_STACK_SIZE);

    thread->kstack.ss0 = 0x10;
    thread->kstack.esp0 = 0;//For kernel threads, this is not required
    thread->kstack.stackStart = (uint32)stack;

    Thread* p = gCurrentThread;

    while (p->next != NULL)
    {
        p = p->next;
    }

    p->next = thread;
}

static int getStringArrayItemCount(char *const array[])
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

static char** cloneStringArray(char *const array[])
{
    int itemCount = getStringArrayItemCount(array);

    char** newArray = kmalloc(sizeof(char*) * (itemCount + 1));

    for (int i = 0; i < itemCount; ++i)
    {
        const char* str = array[i];
        int len = strlen(str);

        char* newStr = kmalloc(len + 1);
        strcpy(newStr, str);

        newArray[i] = newStr;
    }

    newArray[itemCount] = NULL;

    return newArray;
}

static void destroyStringArray(char** array)
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
static void copyArgvEnvToProcess(uint32 location, void* elfData, char *const argv[], char *const envp[])
{
    char** destination = (char**)location;
    int destinationIndex = 0;

    //printkf("ARGVENV: destination:%x\n", destination);

    int argvCount = getStringArrayItemCount(argv);
    int envpCount = getStringArrayItemCount(envp);

    //printkf("ARGVENV: argvCount:%d envpCount:%d\n", argvCount, envpCount);

    char* stringTable = (char*)location + sizeof(char*) * (argvCount + envpCount + 3) + AUX_VECTOR_SIZE_BYTES;

    uint32 auxVectorLocation = location + sizeof(char*) * (argvCount + envpCount + 2);

    //printkf("ARGVENV: stringTable:%x\n", stringTable);

    for (int i = 0; i < argvCount; ++i)
    {
        strcpy(stringTable, argv[i]);

        destination[destinationIndex] = stringTable;

        stringTable += strlen(argv[i]) + 2;

        destinationIndex++;
    }

    destination[destinationIndex++] = NULL;

    for (int i = 0; i < envpCount; ++i)
    {
        strcpy(stringTable, envp[i]);

        destination[destinationIndex] = stringTable;

        stringTable += strlen(envp[i]) + 2;

        destinationIndex++;
    }

    destination[destinationIndex++] = NULL;

    fillAuxilaryVector(auxVectorLocation, elfData);
}

static void fillAuxilaryVector(uint32 location, void* elfData)
{
    Elf32_auxv_t* auxv = (Elf32_auxv_t*)location;

    //printkf("auxv:%x\n", auxv);

    memset((uint8*)auxv, 0, AUX_VECTOR_SIZE_BYTES);

    Elf32_Ehdr *hdr = (Elf32_Ehdr *) elfData;
    Elf32_Phdr *p_entry = (Elf32_Phdr *) (elfData + hdr->e_phoff);

    auxv[0].a_type = AT_HWCAP2;
    auxv[0].a_un.a_val = 0;

    auxv[1].a_type = AT_IGNORE;
    auxv[1].a_un.a_val = 0;

    auxv[2].a_type = AT_EXECFD;
    auxv[2].a_un.a_val = 0;

    auxv[3].a_type = AT_PHDR;
    auxv[3].a_un.a_val = 0;//(uint32)p_entry;

    auxv[4].a_type = AT_PHENT;
    auxv[4].a_un.a_val = 0;//(uint32)p_entry->p_memsz;

    auxv[5].a_type = AT_PHNUM;
    auxv[5].a_un.a_val = 0;//hdr->e_phnum;

    auxv[6].a_type = AT_PAGESZ;
    auxv[6].a_un.a_val = PAGESIZE_4K;

    auxv[7].a_type = AT_BASE;
    auxv[7].a_un.a_val = 0;

    auxv[8].a_type = AT_FLAGS;
    auxv[8].a_un.a_val = 0;

    auxv[9].a_type = AT_ENTRY;
    auxv[9].a_un.a_val = (uint32)hdr->e_entry;

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

    auxv[16].a_type = AT_SECURE;
    auxv[16].a_un.a_val = 0;

    auxv[17].a_type = AT_NULL;
    auxv[17].a_un.a_val = 0;
}

Process* createUserProcessFromElfData(const char* name, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    return createUserProcessEx(name, generateProcessId(), generateThreadId(), NULL, elfData, argv, envp, parent, tty);
}

Process* createUserProcessFromFunction(const char* name, Function0 func, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    return createUserProcessEx(name, generateProcessId(), generateThreadId(), func, NULL, argv, envp, parent, tty);
}

Process* createUserProcessEx(const char* name, uint32 processId, uint32 threadId, Function0 func, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty)
{
    uint32 imageDataEndInMemory = getElfEndInMemory((char*)elfData);

    if (imageDataEndInMemory <= USER_OFFSET)
    {
        printkf("Could not start the process. Image's memory location is wrong! %s\n", name);
        return NULL;
    }

    if (0 == processId)
    {
        processId = generateProcessId();
    }

    if (0 == threadId)
    {
        threadId = generateThreadId();
    }

    Process* process = (Process*)kmalloc(sizeof(Process));
    memset((uint8*)process, 0, sizeof(Process));
    strncpy(process->name, name, PROCESS_NAME_MAX);
    process->name[PROCESS_NAME_MAX - 1] = 0;
    process->pid = processId;
    process->pd = acquirePageDirectory();
    process->workingDirectory = getFileSystemRootNode();

    Thread* thread = (Thread*)kmalloc(sizeof(Thread));
    memset((uint8*)thread, 0, sizeof(Thread));

    thread->owner = process;

    thread->threadId = threadId;

    thread->userMode = 1;

    resumeThread(thread);

    thread->birthTime = getUptimeMilliseconds();

    thread->messageQueue = FifoBuffer_create(sizeof(SosoMessage) * MESSAGE_QUEUE_SIZE);
    Spinlock_Init(&(thread->messageQueueLock));

    thread->signals = FifoBuffer_create(SIGNAL_QUEUE_SIZE);

    thread->regs.cr3 = (uint32) process->pd;

    if (parent)
    {
        process->parent = parent;

        process->workingDirectory = parent->workingDirectory;

        process->tty = parent->tty;
    }

    if (tty)
    {
        process->tty = tty;
    }

    if (process->tty)
    {
        //TODO: unlock below when the old TTY system removed
        /*
        TtyDev* ttyDev = (TtyDev*)process->tty->privateNodeData;

        if (ttyDev->controllingProcess == -1)
        {
            ttyDev->controllingProcess = process->pid;
        }

        if (ttyDev->foregroundProcess == -1)
        {
            ttyDev->foregroundProcess = process->pid;
        }
        */
    }

    //clone to kernel space since we are changing page directory soon
    char** newArgv = cloneStringArray(argv);
    char** newEnvp = cloneStringArray(envp);

    //Change memory view (page directory)
    CHANGE_PD(process->pd);

    initializeProcessPages(process);

    uint32 sizeInMemory = imageDataEndInMemory - USER_OFFSET;

    //printkf("image sizeInMemory:%d\n", sizeInMemory);

    initializeProgramBreak(process, sizeInMemory);


    const uint32 stackPageCount = 5;
    char* vAddressStackPage = (char *) (USER_STACK - PAGESIZE_4K * stackPageCount);
    uint32 stackFrames[stackPageCount];
    for (uint32 i = 0; i < stackPageCount; ++i)
    {
        stackFrames[i] = acquirePageFrame4K();
    }
    void* stackVMem = mapMemory(process, (uint32)vAddressStackPage, stackFrames, stackPageCount, TRUE);
    if (NULL == stackVMem)
    {
        for (uint32 i = 0; i < stackPageCount; ++i)
        {
            releasePageFrame4K(stackFrames[i]);
        }
    }

    uint32 pAddressArgsEnvAux[1];
    pAddressArgsEnvAux[0] = acquirePageFrame4K();
    char* vAddressArgsEnvAux = (char *) (USER_STACK);
    void* mapped = mapMemory(process, (uint32)vAddressArgsEnvAux, pAddressArgsEnvAux, 1, TRUE);
    if (NULL == mapped)
    {
        releasePageFrame4K(pAddressArgsEnvAux[0]);
    }
    else
    {
        copyArgvEnvToProcess(USER_STACK, elfData, newArgv, newEnvp);
    }

    destroyStringArray(newArgv);
    destroyStringArray(newEnvp);

    uint32 selector = 0x23;

    thread->regs.ss = selector;
    thread->regs.eflags = 0x0;
    thread->regs.cs = 0x1B;
    thread->regs.eip = (uint32)func;
    thread->regs.ds = selector;
    thread->regs.es = selector;
    thread->regs.fs = selector;
    thread->regs.gs = selector; //48 | 3;

    uint32 stackPointer = USER_STACK - 4;

    thread->regs.esp = stackPointer;



    thread->kstack.ss0 = 0x10;
    uint8* stack = (uint8*)kmalloc(KERN_STACK_SIZE);
    thread->kstack.esp0 = (uint32)(stack + KERN_STACK_SIZE - 4);
    thread->kstack.stackStart = (uint32)stack;

    Thread* p = gCurrentThread;

    while (p->next != NULL)
    {
        p = p->next;
    }

    p->next = thread;

    if (elfData)
    {
        uint32 startLocation = loadElf((char*)elfData);

        //printkf("process start location:%x\n", startLocation);

        if (startLocation > 0)
        {
            thread->regs.eip = startLocation;
        }
    }

    //Restore memory view (page directory)
    CHANGE_PD(gCurrentThread->regs.cr3);

    open_fs_forProcess(thread, process->tty, 0);//0: standard input
    open_fs_forProcess(thread, process->tty, 0);//1: standard output
    open_fs_forProcess(thread, process->tty, 0);//2: standard error

    return process;
}

//This function should be called in interrupts disabled state
void destroyThread(Thread* thread)
{
    //TODO: signal the process somehow
    Thread* previousThread = getPreviousThread(thread);
    if (NULL != previousThread)
    {
        previousThread->next = thread->next;

        kfree((void*)thread->kstack.stackStart);

        Spinlock_Lock(&(thread->messageQueueLock));
        FifoBuffer_destroy(thread->messageQueue);

        FifoBuffer_destroy(thread->signals);

        Debug_PrintF("destroying thread %d\n", thread->threadId);

        kfree(thread);

        if (thread == gCurrentThread)
        {
            gCurrentThread = NULL;
        }
    }
    else
    {
        printkf("Could not find previous thread for thread %d\n", thread->threadId);
        PANIC("This should not be happened!\n");
    }
}

//This function should be called in interrupts disabled state
void destroyProcess(Process* process)
{
    Thread* thread = gFirstThread;
    Thread* previous = NULL;
    while (thread)
    {
        if (process == thread->owner)
        {
            if (NULL != previous)
            {
                previous->next = thread->next;

                kfree((void*)thread->kstack.stackStart);

                Spinlock_Lock(&(thread->messageQueueLock));
                FifoBuffer_destroy(thread->messageQueue);

                FifoBuffer_destroy(thread->signals);

                Debug_PrintF("destroying thread id:%d (owner process %d)\n", thread->threadId, process->pid);

                kfree(thread);

                if (thread == gCurrentThread)
                {
                    gCurrentThread = NULL;
                }

                thread = previous->next;
                continue;
            }
        }

        previous = thread;
        thread = thread->next;
    }

    //Cleanup opened files
    for (int i = 0; i < MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] != NULL)
        {
            close_fs(process->fd[i]);
        }
    }

    if (process->parent)
    {
        thread = gFirstThread;
        while (thread)
        {
            if (process->parent == thread->owner)
            {
                if (thread->state == TS_WAITCHILD)
                {
                    resumeThread(thread);
                }
            }

            thread = thread->next;
        }
    }

    Debug_PrintF("destroying process %d\n", process->pid);

    uint32 physicalPD = (uint32)process->pd;

    kfree(process);

    destroyPageDirectoryWithMemory(physicalPD);
}

void changeProcessState(Process* process, ThreadState state)
{
    Thread* thread = gFirstThread;

    while (thread)
    {
        if (process == thread->owner)
        {
            changeThreadState(thread, state, NULL);
        }

        thread = thread->next;
    }
}

void changeThreadState(Thread* thread, ThreadState state, void* privateData)
{
    thread->state = state;
    thread->state_privateData = privateData;
}

void resumeThread(Thread* thread)
{
    thread->state = TS_RUN;
    thread->state_privateData = NULL;
}

//must be called in interrupts disabled
BOOL signalThread(Thread* thread, uint8 signal)
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

        if (FifoBuffer_getFree(thread->signals) > 0)
        {
            FifoBuffer_enqueue(thread->signals, &signal, 1);
            thread->pendingSignalCount = FifoBuffer_getSize(thread->signals);

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

void signalProcess(uint32 pid, uint8 signal)
{
    Thread* t = getMainKernelThread();

    while (t != NULL)
    {
        if (t->owner->pid == pid)
        {
            if (signalThread(t, signal))
            {
                //only one thread should receive a signal per process!
                return;
            }
        }
        t = t->next;
    }
}

void threadStateToString(ThreadState state, uint8* buffer, uint32 bufferSize)
{
    if (bufferSize < 1)
    {
        return;
    }

    buffer[0] = '\0';

    switch (state)
    {
    case TS_RUN:
        strncpyNull((char*)buffer, "run", bufferSize);
        break;
    case TS_SLEEP:
        strncpyNull((char*)buffer, "sleep", bufferSize);
        break;
    case TS_SUSPEND:
        strncpyNull((char*)buffer, "suspend", bufferSize);
        break;
    case TS_WAITCHILD:
        strncpyNull((char*)buffer, "waitchild", bufferSize);
        break;
    case TS_WAITIO:
        strncpyNull((char*)buffer, "waitio", bufferSize);
        break;
    case TS_SELECT:
        strncpyNull((char*)buffer, "select", bufferSize);
        break;
    case TS_CRITICAL:
        strncpyNull((char*)buffer, "critical", bufferSize);
        break;
    case TS_DEAD:
        strncpyNull((char*)buffer, "dead", bufferSize);
        break;
    case TS_UNINTERRUPTIBLE:
        strncpyNull((char*)buffer, "uninterruptible", bufferSize);
        break;
    default:
        break;
    }
}

void waitForSchedule()
{
    //Screen_PrintF("Waiting for a schedule()\n");

    enableInterrupts();
    while (TRUE)
    {
        halt();
    }
    disableInterrupts();
    PANIC("waitForSchedule(): Should not be reached here!!!\n");
}

int32 getEmptyFd(Process* process)
{
    int32 result = -1;

    beginCriticalSection();

    for (int i = 0; i < MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] == NULL)
        {
            result = i;
            break;
        }
    }

    endCriticalSection();

    return result;
}

int32 addFileToProcess(Process* process, File* file)
{
    int32 result = -1;

    beginCriticalSection();

    //Screen_PrintF("addFileToProcess: pid:%d\n", process->pid);

    for (int i = 0; i < MAX_OPENED_FILES; ++i)
    {
        //Screen_PrintF("addFileToProcess: i:%d fd[%d]:%x\n", i, i, process->fd[i]);
        if (process->fd[i] == NULL)
        {
            result = i;
            file->fd = i;
            process->fd[i] = file;
            break;
        }
    }

    endCriticalSection();

    return result;
}

int32 removeFileFromProcess(Process* process, File* file)
{
    int32 result = -1;

    beginCriticalSection();

    for (int i = 0; i < MAX_OPENED_FILES; ++i)
    {
        if (process->fd[i] == file)
        {
            result = i;
            process->fd[i] = NULL;
            break;
        }
    }

    endCriticalSection();

    return result;
}

Thread* getThreadById(uint32 threadId)
{
    Thread* p = gFirstThread;

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

Thread* getPreviousThread(Thread* thread)
{
    Thread* t = gFirstThread;

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

Thread* getMainKernelThread()
{
    return gFirstThread;
}

Thread* getCurrentThread()
{
    return gCurrentThread;
}

BOOL isThreadValid(Thread* thread)
{
    Thread* p = gFirstThread;

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

BOOL isProcessValid(Process* process)
{
    Thread* p = gFirstThread;

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

static void switchToTask(Thread* current, int mode);

static void updateUsageMetrics()
{
    uint32 seconds = getUptimeSeconds();

    if (seconds > gUsageMarkPoint)
    {
        gUsageMarkPoint = seconds;

        const uint32 millisecondsPassed = 1000;

        Thread* t = gFirstThread;
        while (NULL != t)
        {
            uint32 consumedFromMark = t->consumedCPUTimeMs - t->consumedCPUTimeMsAtPrevMark;

            t->usageCPU = (100 * consumedFromMark) / millisecondsPassed;

            t->consumedCPUTimeMsAtPrevMark = t->consumedCPUTimeMs;
            
            t = t->next;
        }
    }    
}

static void updateThreadState(Thread* t)
{
    if (t->state == TS_SLEEP)
    {
        uint32 uptime = getUptimeMilliseconds();
        uint32 target = (uint32)t->state_privateData;

        if (uptime >= target)
        {
            resumeThread(t);
        }
    }
    else if (t->state == TS_SELECT)
    {
        updateSelect(t);

        if (t->select.selectState == SS_FINISHED)
        {
            resumeThread(t);
        }
    }
}

static Thread* lookThreads(Thread* current)
{
    Thread* t = current->next;
    while (NULL != t)
    {
        updateThreadState(t);

        if (t->state == TS_RUN)
        {
            //We definetly found a good one!
            return t;
        }
        t = t->next;
    }


    //Reached the last thread. Let's try from first thread up to the current including.
    //Of course this makes sense if current is not the first one already.

    if (current == gFirstThread)
    {
        //We already searched for all threads

        //Desperately return idle thread
        return gFirstThread;
    }
    else
    {
        t = gFirstThread->next;
        while (NULL != t)
        {
            updateThreadState(t);

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
    return gFirstThread;
}

static void endContext(TimerInt_Registers* registers, Thread* thread)
{
    thread->contextEndTime = getUptimeMilliseconds();
    thread->consumedCPUTimeMs += thread->contextEndTime - thread->contextStartTime;

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
        //Debug_PrintF("schedule() - 2.1\n");
        thread->regs.esp = registers->esp_if_privilege_change;
        thread->regs.ss = registers->ss_if_privilege_change;
    }
    else
    {
        //Debug_PrintF("schedule() - 2.2\n");
        thread->regs.esp = registers->esp + 12;
        thread->regs.ss = gTss.ss0;
    }

    //Save the TSS from the old process
    thread->kstack.ss0 = gTss.ss0;
    thread->kstack.esp0 = gTss.esp0;
}

static void startContext(Thread* thread)
{
    gCurrentThread = thread;//Now gCurrentThread is the thread we are about to schedule to

    thread->contextStartTime = getUptimeMilliseconds();

    ++gSystemContextSwitchCount;

    ++thread->contextSwitchCount;

    if (thread->regs.cs != 0x08)
    {
        switchToTask(thread, USERMODE);
    }
    else
    {
        switchToTask(thread, KERNELMODE);
    }
}

void schedule(TimerInt_Registers* registers)
{
    Thread* current = gCurrentThread;

    Thread* readyThread = NULL;

    if (NULL != current)
    {
        if (current->next == NULL && current == gFirstThread)
        {
            //We are the only thread, no need to schedule
            return;
        }

        if (current->state == TS_UNINTERRUPTIBLE)
        {
            return;
        }

        endContext(registers, current);

        readyThread = lookThreads(current);
    }
    else
    {
        //current is NULL. This means the thread is destroyed.

        readyThread = lookThreads(gFirstThread);
    }

    if (readyThread != gFirstThread)
    {
        if (FifoBuffer_getSize(readyThread->signals) > 0)
        {
            uint8 signal = 0;
            FifoBuffer_dequeue(readyThread->signals, &signal, 1);
            readyThread->pendingSignalCount = FifoBuffer_getSize(readyThread->signals);

            printkf("Signal %d proccessing for pid:%d in scheduler!\n", (uint32)signal, readyThread->owner->pid);

            //TODO: call signal handlers

            switch (signal)
            {
            case SIGTERM:
            case SIGKILL:
            case SIGSEGV:
            case SIGINT:
            case SIGILL:
                printkf("Killing pid:%d in scheduler!\n", readyThread->owner->pid);
            
                destroyProcess(readyThread->owner);

                readyThread = lookThreads(gFirstThread);
                break;
            case SIGSTOP:
            case SIGTSTP:
                readyThread->state = TS_SUSPEND;

                readyThread = lookThreads(gFirstThread);
                break;
            
            default:
                break;
            }
            
        }
    }

    updateUsageMetrics();

    startContext(readyThread);
}



//The mode indicates whether this process was in user mode or kernel mode
//When it was previously interrupted by the scheduler.
static void switchToTask(Thread* current, int mode)
{
    uint32 kesp, eflags;
    uint16 kss, ss, cs;

    //Set TSS values
    gTss.ss0 = current->kstack.ss0;
    gTss.esp0 = current->kstack.esp0;

    ss = current->regs.ss;
    cs = current->regs.cs;
    eflags = (current->regs.eflags | 0x200) & 0xFFFFBFFF;

    if (mode == USERMODE)
    {
        kss = current->kstack.ss0;
        kesp = current->kstack.esp0;
    }
    else
    {
        kss = current->regs.ss;
        kesp = current->regs.esp;
    }

    //switchTask is in task.asm

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
        ljmp $0x08, $switchTask"
        :: \
        "m"(kss), \
        "m"(kesp), \
        "m"(ss), \
        "m"(current->regs.esp), \
        "m"(eflags), \
        "m"(cs), \
        "m"(current->regs.eip), \
        "m"(current), \
        [KMODE] "i"(KERNELMODE), \
        [mode] "g"(mode)
        );
}
