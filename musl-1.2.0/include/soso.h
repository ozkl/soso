#ifndef SOSO_H
#define SOSO_H

#include <stdint.h>

#define SOSO_MAX_OPENED_FILES 20
#define SOSO_PROCESS_NAME_MAX 32

typedef struct ThreadInfo
{
    uint32_t thread_id;
    uint32_t process_id;
    uint32_t state;
    uint32_t user_mode;

    uint32_t birth_time;
    uint32_t context_switch_count;
    uint32_t context_start_time;
    uint32_t context_end_time;
    uint32_t consumed_cpu_time_ms;
    uint32_t usage_cpu;
    uint32_t called_syscall_count;
} ThreadInfo;

typedef struct ProcInfo
{
    uint32_t process_id;
    int32_t parent_process_id;
    uint32_t fd[SOSO_MAX_OPENED_FILES];

    char name[SOSO_PROCESS_NAME_MAX];
    char tty[128];
    char working_directory[128];
} ProcInfo;

typedef enum FileType
{
    FT_File               = 1,
    FT_CharacterDevice    = 2,
    FT_BlockDevice        = 3,
    FT_Pipe               = 4,
    FT_SymbolicLink       = 5,
    FT_Directory          = 128,
    FT_MountPoint         = 256
} FileType;

typedef struct FileSystemDirent
{
    char name[128];
    FileType fileType;
    int32_t inode;
} FileSystemDirent;

typedef struct SosoMessage
{
    uint32_t message_type;
    uint32_t parameter1;
    uint32_t parameter2;
    uint32_t parameter3;
} SosoMessage;

int32_t getthreads(ThreadInfo* threads, uint32_t max_count, uint32_t flags);
int32_t getprocs(ProcInfo* procs, uint32_t max_count, uint32_t flags);

int32_t execute_on_tty(const char *path, char *const argv[], char *const envp[], const char *ttyPath);
int32_t execute(const char *path, char *const argv[], char *const envp[]);
int32_t executep(const char *filename, char *const argv[], char *const envp[]);

int32_t soso_read_dir(int32_t fd, void *dirent, int32_t index);

void sleep_ms(uint32_t ms);

uint32_t get_uptime_ms();

int32_t manage_message(int32_t command, SosoMessage* message);
int get_message_count();
void send_message(SosoMessage* message);
int get_next_message(SosoMessage* message);

int32_t manage_pipe(const char *pipe_name, int32_t operation, int32_t data);

#endif //SOSO_H