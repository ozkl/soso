#ifndef PROCESS_H
#define PROCESS_H

#define KERNELMODE	0
#define USERMODE	1

#define MAX_OPENED_FILES 20

#define PROCESS_NAME_MAX 32

#include "common.h"
#include "fs.h"
#include "syscall_select.h"
#include "fifobuffer.h"
#include "spinlock.h"
#include "signal.h"

typedef enum ThreadState
{
    TS_RUN,
    TS_WAITIO,
    TS_WAITCHILD,
    TS_SLEEP,
    TS_SELECT,
    TS_SUSPEND,
    TS_CRITICAL,        //When a driver is in spinlock.

    TS_UNINTERRUPTIBLE, //Not recommended to be used. Other threads cannot be scheduled to. Timer continues to work.

    TS_DEAD,            //This means a bug in the kernel code (could be a driver) caused a fault to the thread which is in CRITICAL or UNINTERRUPTIBLE state
                        //the fault handler did not kill the thread but changed its state to DEAD.
                        //So that the bug can be traced. 

    
} ThreadState;

typedef enum SelectState
{
    SS_NOTSTARTED,
    SS_STARTED,
    SS_FINISHED,
    SS_ERROR
} SelectState;

struct Process
{
    char name[PROCESS_NAME_MAX];

    uint32 pid;


    uint32 *pd;

    uint32 b_exec;
    uint32 e_exec;
    uint32 b_bss;
    uint32 e_bss;

    char *brkBegin;
    char *brkEnd;
    char *brkNextUnallocatedPageBegin;

    uint8 mmappedVirtualMemory[RAM_AS_4K_PAGES / 8];

    FileSystemNode* tty;

    FileSystemNode* workingDirectory;

    Process* parent;

    File* fd[MAX_OPENED_FILES];

} __attribute__ ((packed));

typedef struct Process Process;

struct Thread
{
    uint32 threadId;

    struct
    {
        uint32 eax, ecx, edx, ebx;
        uint32 esp, ebp, esi, edi;
        uint32 eip, eflags;
        uint32 cs:16, ss:16, ds:16, es:16, fs:16, gs:16;
        uint32 cr3;
    } regs __attribute__ ((packed));

    struct
    {
        uint32 esp0;
        uint16 ss0;
        uint32 stackStart;
    } kstack __attribute__ ((packed));

    struct
    {
        int nfds;
        fd_set readSet;
        fd_set writeSet;
        fd_set readSetResult;
        fd_set writeSetResult;
        time_t targetTime;
        SelectState selectState;
        int result;
    } select;

    uint32 userMode;

    FifoBuffer* signals;//no need to lock as this always accessed in interrupts disabled
    uint32 pendingSignalCount;

    ThreadState state;
    void* state_privateData;

    Process* owner;

    uint32 birthTime;
    uint32 contextSwitchCount;
    uint32 contextStartTime;
    uint32 contextEndTime;
    uint32 consumedCPUTimeMs;
    uint32 consumedCPUTimeMsAtPrevMark;
    uint32 usageCPU; //FromPrevMark
    uint32 calledSyscallCount;


    FifoBuffer* messageQueue;
    Spinlock messageQueueLock;

    struct Thread* next;

};

typedef struct Thread Thread;

typedef struct TimerInt_Registers
{
    uint32 gs, fs, es, ds;
    uint32 edi, esi, ebp, esp, ebx, edx, ecx, eax; //pushed by pushad
    uint32 eip, cs, eflags, esp_if_privilege_change, ss_if_privilege_change; //pushed by the CPU
} TimerInt_Registers;

typedef void (*Function0)();

void initializeTasking();
void createKernelThread(Function0 func);
Process* createUserProcessFromElfData(const char* name, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
Process* createUserProcessFromFunction(const char* name, Function0 func, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
Process* createUserProcessEx(const char* name, uint32 processId, uint32 threadId, Function0 func, uint8* elfData, char *const argv[], char *const envp[], Process* parent, FileSystemNode* tty);
void destroyThread(Thread* thread);
void destroyProcess(Process* process);
void changeProcessState(Process* process, ThreadState state);
void changeThreadState(Thread* thread, ThreadState state, void* privateData);
void resumeThread(Thread* thread);
BOOL signalThread(Thread* thread, uint8 signal);
BOOL signalProcess(uint32 pid, uint8 signal);
void threadStateToString(ThreadState state, uint8* buffer, uint32 bufferSize);
void waitForSchedule();
int32 getEmptyFd(Process* process);
int32 addFileToProcess(Process* process, File* file);
int32 removeFileFromProcess(Process* process, File* file);
Thread* getThreadById(uint32 threadId);
Thread* getPreviousThread(Thread* thread);
Thread* getMainKernelThread();
Thread* getCurrentThread();
void schedule(TimerInt_Registers* registers);
BOOL isThreadValid(Thread* thread);
BOOL isProcessValid(Process* process);
uint32 getSystemContextSwitchCount();

extern Thread* gCurrentThread;

#endif // PROCESS_H
