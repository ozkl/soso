#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <termios.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>  // For PATH_MAX

// If PATH_MAX is not defined in limits.h, define it ourselves
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#include <soso.h>


int g_background = 0;
struct termios orig_termios;

// Add a global flag to indicate if we're in the shell or a child process
int is_child_process = 0;

// Command history
#define HISTORY_MAX 100
char *command_history[HISTORY_MAX];
int history_count = 0;
int history_position = 0;
char current_line[1024]; // Store the current line before navigating history

// Tab completion structures
#define MAX_COMPLETIONS 100
typedef struct {
    char *matches[MAX_COMPLETIONS];
    int count;
} completion_t;

// Function declarations
void handle_tab_completion(char *buffer, int *len, int *cursor);

// Add a command to history
void add_to_history(const char *cmd) {
    if (cmd[0] == '\0') return; // Don't add empty commands
    
    // If we already have this command as the most recent one, don't add it again
    if (history_count > 0 && strcmp(command_history[history_count-1], cmd) == 0) {
        return;
    }
    
    // If we've reached max history, free the oldest entry
    if (history_count == HISTORY_MAX) {
        free(command_history[0]);
        // Shift all entries down
        for (int i = 1; i < HISTORY_MAX; i++) {
            command_history[i-1] = command_history[i];
        }
        history_count--;
    }
    
    // Add the new command
    command_history[history_count] = strdup(cmd);
    history_count++;
    // Reset history position to point to the end
    history_position = history_count;
}

// Signal handlers
void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    int status;
    
    // Reap all available zombie children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            // Normal termination
        } else if (WIFSIGNALED(status)) {
            printf("\nChild %d terminated by signal %d\n", pid, WTERMSIG(status));
        }
    }
    
    errno = saved_errno;
}

void sigint_handler(int sig) {
    // Ctrl-C - just print a newline and continue
    printf("\n");
    fflush(stdout);
}

void sigtstp_handler(int sig) {
    // Ctrl-Z - just print a message and continue
    printf("\nCan't suspend the shell.\n");
    fflush(stdout);
}

void disableRawMode()
{
  // Only print and restore terminal if we're in the shell process
  if (!is_child_process) {
    printf("returning original termios\n");
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    
    // Free history
    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
        command_history[i] = NULL;
    }
  }
}

void enableRawMode()
{
    struct termios raw = orig_termios;
    // Turn off ICRNL to prevent \r from being translated to \n
    // Turn off INLCR to prevent \n from being translated to \r
    raw.c_iflag &= ~(ICRNL | INLCR | IXON);
    
    // Turn off output post-processing (like CR to NL translation)
    raw.c_oflag &= ~(OPOST);
    
    // Turn off canonical mode, echo, and signals
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    // Read returns when min characters are available
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Move cursor left
void move_cursor_left() {
    write(STDOUT_FILENO, "\x1b[D", 3);
}

// Move cursor right
void move_cursor_right() {
    write(STDOUT_FILENO, "\x1b[C", 3);
}

static void listDirectory2(const char* path)
{
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (d)
    {
        char buffer[256];
        // Print files one per line
        while ((dir = readdir(d)) != NULL)
        {
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "%s/%s", path, dir->d_name);
            struct stat st;
            memset(&st, 0, sizeof(st));

            const char * entry_type = " ";
            uint32_t size = 0;

            if (stat(buffer, &st) == 0)
            {
                size = (uint32_t)st.st_size;

                if (S_ISDIR(st.st_mode))
                    entry_type = "d";
                else if (S_ISREG(st.st_mode))
                    entry_type = "-";
                else if (S_ISCHR(st.st_mode))
                    entry_type = "c";
                else if (S_ISBLK(st.st_mode))
                    entry_type = "b";
                else if (S_ISFIFO(st.st_mode))
                    entry_type = "p";
                else if (S_ISLNK(st.st_mode))
                    entry_type = "l";
                else
                    entry_type = "?";
            }

            printf("%s %d\t\t%s\n", entry_type, size, dir->d_name);

            // Force output to display immediately
            fflush(stdout);
        }
        closedir(d);
    } else {
        //perror("opendir");
        printf("Could not open directory:%s\n", path);
        fflush(stdout);
    }
}


extern char **environ;

static void printEnvironment()
{
    int i = 1;
    char *s = *environ;

    for (; s; i++)
    {
        printf("%s\n", s);
        s = *(environ+i);
    }
}

static int getArrayItemCount(char *const array[])
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

static char** createArray(const char* cmdline)
{
    //TODO: make 50 below n
    char** array = malloc(sizeof(char*)*50);
    int i = 0;

    const char* bb = cmdline;

    char single[128];

    memset(array, 0, sizeof(char*)*50);

    while (bb != NULL)
    {
        const char* token = strstr(bb, " ");

        if (NULL == token)
        {
            int l = strlen(bb);
            if (l > 0)
            {
                token = bb + l;
            }
        }

        if (token)
        {
            int len = token - bb;

            if (len > 0)
            {
                strncpy(single, bb, len);
                single[len] = '\0';

                //printf("%d:[%s]\n", len, single);

                char* str = malloc(len + 1);
                strcpy(str, single);

                array[i++] = str;
            }

            bb = token + 1;
        }
        else
        {
            break;
        }
    }

    return array;
}

static void destroyArray(char** array)
{
    char* a = array[0];

    int i = 0;

    while (NULL != a)
    {
        free(a);

        a = array[++i];
    }

    free(array);
}

int run_command_unix(const char* path, char **argv, char **env)
{
    // Set up signal handling
    sigset_t mask_all, prev_mask;
    sigfillset(&mask_all);
    sigprocmask(SIG_BLOCK, &mask_all, &prev_mask);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        return -1;
    }

    if (pid == 0)
    {
        // Child process
        // Set the flag to indicate we're in a child process
        is_child_process = 1;
        
        // Restore signal mask in child
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        
        // Set child in its own process group
        setpgid(0, 0);
        
        // Take control of terminal
        tcsetpgrp(STDIN_FILENO, getpid());
        
        // Reset terminal to canonical mode for child
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        
        // Execute the command
        int result = execvpe(path, argv, env);
        
        // If execvp returns, it failed
        printf("Could not execute:%s\n", path);
        fflush(stdout);
        exit(1);
    }
    else
    {
        // Parent process
        
        // Set child in its own process group
        setpgid(pid, pid);
        
        // Give terminal control to child
        tcsetpgrp(STDIN_FILENO, pid);
        
        // Restore signal mask in parent
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);

        // Wait for child
        int status;
        waitpid(pid, &status, WUNTRACED);

        // Reclaim terminal control
        tcsetpgrp(STDIN_FILENO, getpgrp());

        // Check if child was stopped
        if (WIFSTOPPED(status))
        {
            printf("[%d]+ Stopped                 %s\n", pid, path);
            return 0;
        }

        return status;
    }
}

#ifdef SOSO_H
int run_command_soso(const char* command, char **argv, char **env)
{
    const char* tty = NULL;
    if (g_background)
    {
        tty = "/dev/null";
    }

    int executed_pid = execute_on_tty(command, argv, env, tty);

    if (g_background)
    {
        printf("Running in background %s [%d]\n", command, executed_pid);
    }

    if (executed_pid >= 0 && g_background == 0)
    {
        tcsetpgrp(0, executed_pid);
        int waitStatus = 0;
        wait(&waitStatus);

        tcsetpgrp(0, getpid());

        //printf("Exited pid:%d\n", result);
        fflush(stdout);
    }

    return executed_pid;
}
#endif

int run_command(const char* path, char **argv, char **env)
{
#ifdef SOSO_H
    return run_command_soso(path, argv, env);
#else
    return run_command_unix(path, argv, env);
#endif
}

// Clear line and print buffer
void refresh_line(const char *buf, int len, int cursor) {
    char cwd[256];
    char prompt[256];
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(prompt, sizeof(prompt), "[%s]$ ", cwd);
    } else {
        snprintf(prompt, sizeof(prompt), "$ ");
    }
    
    // Calculate cursor position for screen
    int prompt_len = strlen(prompt);
    
    // Move to beginning of line and clear it
    write(STDOUT_FILENO, "\r", 1);            // Return to beginning of line
    write(STDOUT_FILENO, "\x1b[K", 3);        // Clear line from cursor to end
    
    // Write prompt and buffer contents
    write(STDOUT_FILENO, prompt, prompt_len);
    write(STDOUT_FILENO, buf, len);
    
    // Move cursor to the correct position
    // First move to beginning
    write(STDOUT_FILENO, "\r", 1);
    
    // Then move cursor to prompt + cursor position
    if (prompt_len + cursor > 0) {
        char cursor_buf[32];
        snprintf(cursor_buf, sizeof(cursor_buf), "\x1b[%dC", prompt_len + cursor);
        write(STDOUT_FILENO, cursor_buf, strlen(cursor_buf));
    }
}

#define MAX_LINE 1024

void read_line(char *buffer) {
    int len = 0;
    int cursor = 0;
    int using_history = 0; // Flag to indicate if we're browsing history
    
    memset(buffer, 0, MAX_LINE);
    memset(current_line, 0, sizeof(current_line)); // Clear saved line
    
    // Initialize with empty buffer
    refresh_line(buffer, 0, 0);

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;

        if (c == '\r' || c == '\n') {
            // Don't echo the carriage return as ^M
            write(STDOUT_FILENO, "\n", 1);
            memset(buffer + len, 0, MAX_LINE);
            return;
        } else if (c == 127 || c == '\b') {  // Backspace
            if (cursor > 0) {
                memmove(&buffer[cursor - 1], &buffer[cursor], len - cursor);
                len--;
                cursor--;
                refresh_line(buffer, len, cursor);
                
                // We're editing the line, so update current_line
                if (using_history) {
                    strncpy(current_line, buffer, sizeof(current_line)-1);
                    using_history = 0;
                }
            }
        } else if (c == 27) {  // Escape sequence
            char seq[5] = {0}; // Expanded to handle longer sequences
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;
            
            // Arrow keys: ESC [ A/B/C/D
            if (seq[0] == '[') {
                if (seq[1] == 'D') {  // Left arrow
                    if (cursor > 0) {
                        cursor--;
                        refresh_line(buffer, len, cursor);
                    }
                } else if (seq[1] == 'C') {  // Right arrow
                    if (cursor < len) {
                        cursor++;
                        refresh_line(buffer, len, cursor);
                    }
                } else if (seq[1] == 'A') {  // Up arrow - history previous
                    if (history_count > 0) {
                        // First up arrow press - save current line
                        if (!using_history && history_position == history_count) {
                            strncpy(current_line, buffer, sizeof(current_line)-1);
                        }
                        
                        if (history_position > 0) {
                            history_position--;
                            strncpy(buffer, command_history[history_position], MAX_LINE-1);
                            len = strlen(buffer);
                            cursor = len;
                            refresh_line(buffer, len, cursor);
                            using_history = 1;
                        }
                    }
                } else if (seq[1] == 'B') {  // Down arrow - history next
                    if (history_position < history_count) {
                        history_position++;
                        
                        if (history_position == history_count) {
                            // At the end of history, restore the saved line
                            strncpy(buffer, current_line, MAX_LINE-1);
                            using_history = 0;
                        } else {
                            strncpy(buffer, command_history[history_position], MAX_LINE-1);
                            using_history = 1;
                        }
                        
                        len = strlen(buffer);
                        cursor = len;
                        refresh_line(buffer, len, cursor);
                    }
                } else if (seq[1] == 'H') {  // Home key
                    cursor = 0;
                    refresh_line(buffer, len, cursor);
                } else if (seq[1] == 'F') {  // End key
                    cursor = len;
                    refresh_line(buffer, len, cursor);
                } else if (seq[1] == '1' || seq[1] == '7') {  // Home key - alternate sequences
                    // Some terminals send ESC [ 1 ~ or ESC [ 7 ~ for Home
                    if (read(STDIN_FILENO, &seq[2], 1) != 1) continue;
                    if (seq[2] == '~') {
                        cursor = 0;
                        refresh_line(buffer, len, cursor);
                    }
                } else if (seq[1] == '4' || seq[1] == '8') {  // End key - alternate sequences
                    // Some terminals send ESC [ 4 ~ or ESC [ 8 ~ for End
                    if (read(STDIN_FILENO, &seq[2], 1) != 1) continue;
                    if (seq[2] == '~') {
                        cursor = len;
                        refresh_line(buffer, len, cursor);
                    }
                }
            }
        } else if (c == 4) {  // Ctrl-D
            if (len == 0) {
                // EOF on empty line
                write(STDOUT_FILENO, "exit\n", 5);
                strcpy(buffer, "exit");
                return;
            }
        } else if (c == 3) {  // Ctrl-C
            // Clear current line
            write(STDOUT_FILENO, "^C\n", 3);
            buffer[0] = '\0';
            return;
        } else if (c == 1) {  // Ctrl-A (beginning of line)
            cursor = 0;
            refresh_line(buffer, len, cursor);
        } else if (c == 5) {  // Ctrl-E (end of line)
            cursor = len;
            refresh_line(buffer, len, cursor);
        } else if (c == 9) {  // Tab - completion
            handle_tab_completion(buffer, &len, &cursor);
            refresh_line(buffer, len, cursor);
        } else if (c >= 32 && c < 127) {  // Printable ASCII
            if (len < MAX_LINE - 1) {
                memmove(&buffer[cursor + 1], &buffer[cursor], len - cursor);
                buffer[cursor] = c;
                len++;
                cursor++;
                refresh_line(buffer, len, cursor);
                
                // We're editing the line, so update current_line
                if (using_history) {
                    strncpy(current_line, buffer, sizeof(current_line)-1);
                    using_history = 0;
                }
            }
        }
    }
}

// Find the full path of a command by searching the PATH environment variable
const char* find_command_path(const char* command)
{
    static char command_path[PATH_MAX]; // Static buffer to store the result
    
    // If command contains a slash, it's already a path
    if (strchr(command, '/') != NULL)
    {
        strncpy(command_path, command, PATH_MAX-1);
        command_path[PATH_MAX-1] = '\0';
        return command_path;
    }
    
    // Get the PATH environment variable
    char* path_env = getenv("PATH");
    if (path_env == NULL)
    {
        return NULL;
    }
    
    // Make a copy of PATH since strtok modifies the string
    static char path_copy[PATH_MAX];
    strncpy(path_copy, path_env, PATH_MAX-1);
    path_copy[PATH_MAX-1] = '\0';
    
    char* path_token = strtok(path_copy, ":");
    
    // Try each directory in PATH
    while (path_token != NULL)
    {
        // Construct the full path
        snprintf(command_path, PATH_MAX, "%s/%s", path_token, command);
        
        // Check if the file exists and is executable
        struct stat st;
        if (stat(command_path, &st) == 0)
        {
            // Check if it's a regular file and has execute permission
            if (S_ISREG(st.st_mode) /*&& (st.st_mode & S_IXUSR)*/)
            {
                return command_path;
            }
        }
        
        path_token = strtok(NULL, ":");
    }
    
    return NULL; // Command not found
}

void handle_ls(char** args)
{
    char* path = NULL;
    if (getArrayItemCount(args) > 1)
    {
        path = args[1];
    }

    if (path)
    {
        listDirectory2(path);
    }
    else
    {
        char cwd[256];
        memset(cwd, 0, sizeof(cwd));
        getcwd(cwd, sizeof(cwd));
        listDirectory2(cwd);
    }
}

void handle_pwd(char** args)
{
    char cwd[256];
    memset(cwd, 0, sizeof(cwd));
    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
    fflush(stdout);
}

void handle_cd(char** args)
{
    char* path = NULL;
    if (getArrayItemCount(args) > 1)
    {
        path = args[1];
    }

    if (NULL == path)
    {
        path = getenv("HOME");
    }

    if (NULL != path)
    {
        if (chdir(path) < 0)
        {
            printf("Directory unavailable:%s\n", path);
            fflush(stdout);
        }
    }
}

void handle_kill(char** args)
{
    int pid = -1;
    int signal = SIGTERM;

    if (getArrayItemCount(args) > 1)
    {
        pid = strtol(args[1], NULL, 10);
    }

    if (getArrayItemCount(args) > 2)
    {
        signal = strtol(args[1], NULL, 10);
        pid = strtol(args[2], NULL, 10);
    }
    
    if (pid < 0)
    {
        printf("usage: kill [signal] pid\n");
        fflush(stdout);
        return;
    }
    
    printf("Signal %d to pid %d\n", signal, pid);
    fflush(stdout);

    if (kill(pid, signal) < 0)
    {
        printf("kill(%d, %d) failed!\n", pid, signal);
    }
    else
    {
        printf("kill(%d, %d) success!\n", pid, signal);
    }

    fflush(stdout);
}

void handle_which(char** args)
{
    char* cmd = NULL;
    if (getArrayItemCount(args) > 1)
    {
        cmd = args[1];

        const char* full_path = find_command_path(cmd);
        if (full_path != NULL)
        {
            printf("%s\n", full_path);
        }
    }
}

void handle_exit(char** args)
{
    exit(0);
}

void handle_env(char** args)
{
    printEnvironment();
    fflush(stdout);
}

void handle_putenv(char** args)
{
    char* assignment = NULL;
    if (getArrayItemCount(args) > 1)
    {
        assignment = args[1];

        putenv(assignment);
    }
}

void handle_file(char** args)
{
    char* path = NULL;
    if (getArrayItemCount(args) > 1)
    {
        path = args[1];

        struct stat s;
        if (stat(path, &s) == 0)
        {
            printf("Exists: %s (%d)\n", path, s.st_mode);
        }
        else
        {
            printf("Not Exists: %s\n", path);
        }
    }
}

typedef struct
{
    const char * command;
    void (*Function)(char** args);
} BuiltinCommand;

BuiltinCommand g_builtin_commands[] =
{
    {"ls", handle_ls},
    {"pwd", handle_pwd},
    {"cd", handle_cd},
    {"kill", handle_kill},
    {"which", handle_which},
    {"exit", handle_exit},
    {"env", handle_env},
    {"putenv", handle_putenv},
    {"file", handle_file}
};

BuiltinCommand * get_command_handler(const char * command)
{
    size_t command_len = strlen(command);

    size_t num_commands = sizeof(g_builtin_commands) / sizeof(BuiltinCommand);
    for (size_t i = 0; i < num_commands; ++i)
    {
        BuiltinCommand * c = &g_builtin_commands[i];

        if (strncmp(c->command, command, command_len) == 0 && (c->command[command_len] == '\0'))
        {
            return c;
        }
    }

    return NULL;
}

// Tab completion functions
void free_completion(completion_t *comp) {
    for (int i = 0; i < comp->count; i++) {
        free(comp->matches[i]);
    }
    comp->count = 0;
}

// Find common prefix of all matches
int find_common_prefix(completion_t *comp, char *prefix, int max_len) {
    if (comp->count == 0) return 0;
    if (comp->count == 1) {
        strncpy(prefix, comp->matches[0], max_len - 1);
        prefix[max_len - 1] = '\0';
        return strlen(prefix);
    }
    
    int common_len = 0;
    int min_len = strlen(comp->matches[0]);
    
    // Find minimum length
    for (int i = 1; i < comp->count; i++) {
        int len = strlen(comp->matches[i]);
        if (len < min_len) min_len = len;
    }
    
    // Find common prefix
    for (int pos = 0; pos < min_len && pos < max_len - 1; pos++) {
        char c = comp->matches[0][pos];
        int all_match = 1;
        
        for (int i = 1; i < comp->count; i++) {
            if (comp->matches[i][pos] != c) {
                all_match = 0;
                break;
            }
        }
        
        if (all_match) {
            prefix[pos] = c;
            common_len++;
        } else {
            break;
        }
    }
    
    prefix[common_len] = '\0';
    return common_len;
}

// Complete builtin commands
void complete_builtin_commands(const char *prefix, completion_t *comp) {
    int prefix_len = strlen(prefix);
    size_t num_commands = sizeof(g_builtin_commands) / sizeof(BuiltinCommand);
    
    for (size_t i = 0; i < num_commands && comp->count < MAX_COMPLETIONS; i++) {
        if (strncmp(g_builtin_commands[i].command, prefix, prefix_len) == 0) {
            comp->matches[comp->count] = strdup(g_builtin_commands[i].command);
            comp->count++;
        }
    }
}

// Complete files in a directory
void complete_files_in_dir(const char *dir_path, const char *prefix, completion_t *comp) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    
    int prefix_len = strlen(prefix);
    struct dirent *entry;
    
    while ((entry = readdir(d)) != NULL && comp->count < MAX_COMPLETIONS) {
        // Skip . and .. unless explicitly typed
        if (entry->d_name[0] == '.' && prefix_len == 0) continue;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            if (prefix_len == 0 || (prefix[0] != '.' || 
                (prefix_len == 1 && strcmp(entry->d_name, ".") != 0) ||
                (prefix_len == 2 && strcmp(entry->d_name, "..") != 0))) {
                continue;
            }
        }
        
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            comp->matches[comp->count] = strdup(entry->d_name);
            comp->count++;
        }
    }
    
    closedir(d);
}

// Complete commands in PATH
void complete_path_commands(const char *prefix, completion_t *comp) {
    char *path_env = getenv("PATH");
    if (!path_env) return;
    
    static char path_copy[PATH_MAX];
    strncpy(path_copy, path_env, PATH_MAX - 1);
    path_copy[PATH_MAX - 1] = '\0';
    
    char *path_token = strtok(path_copy, ":");
    
    while (path_token != NULL && comp->count < MAX_COMPLETIONS) {
        complete_files_in_dir(path_token, prefix, comp);
        path_token = strtok(NULL, ":");
    }
}

// Main tab completion function
void handle_tab_completion(char *buffer, int *len, int *cursor) {
    completion_t comp = {0};
    
    // Find the word we're completing
    int word_start = *cursor;
    int word_end = *cursor;
    
    // Find start of current word
    while (word_start > 0 && buffer[word_start - 1] != ' ') {
        word_start--;
    }
    
    // Find end of current word
    while (word_end < *len && buffer[word_end] != ' ') {
        word_end++;
    }
    
    // Extract the word to complete
    char word[256] = {0};
    int word_len = word_end - word_start;
    if (word_len > 0 && word_len < sizeof(word)) {
        strncpy(word, buffer + word_start, word_len);
        word[word_len] = '\0';
    }
    
    // Determine if we're completing the first word (command) or subsequent words (files)
    int is_first_word = 1;
    for (int i = 0; i < word_start; i++) {
        if (buffer[i] != ' ') {
            is_first_word = 0;
            break;
        }
    }
    
    // Variables for path handling
    char *slash_pos = NULL;
    char dir_path[256] = {0};
    char filename_prefix[256] = {0};
    int is_path_completion = 0;
    
    if (is_first_word) {
        // Complete commands (builtins + PATH)
        complete_builtin_commands(word, &comp);
        complete_path_commands(word, &comp);
    } else {
        // Complete files
        slash_pos = strrchr(word, '/');
        if (slash_pos) {
            // Word contains a path
            is_path_completion = 1;
            int dir_len = slash_pos - word;
            
            if (dir_len == 0) {
                strcpy(dir_path, "/");
            } else {
                strncpy(dir_path, word, dir_len);
                dir_path[dir_len] = '\0';
            }
            
            strcpy(filename_prefix, slash_pos + 1);
            complete_files_in_dir(dir_path, filename_prefix, &comp);
        } else {
            // Complete files in current directory
            complete_files_in_dir(".", word, &comp);
        }
    }
    
    if (comp.count == 0) {
        // No matches
        free_completion(&comp);
        return;
    } else if (comp.count == 1) {
        // Single match - complete it
        char *match = comp.matches[0];
        int match_len = strlen(match);
        
        if (is_path_completion) {
            // Replace just the filename part
            int dir_len = strlen(dir_path);
            if (strcmp(dir_path, "/") != 0) dir_len++; // Add 1 for the slash, except for root
            else dir_len = 1; // Root directory case
            
            int filename_start_pos = word_start + dir_len;
            int current_filename_len = strlen(filename_prefix);
            int add_len = match_len - current_filename_len;
            
            if (add_len > 0 && *len + add_len < MAX_LINE - 1) {
                // Make room for the completion
                memmove(buffer + word_end + add_len, buffer + word_end, *len - word_end);
                
                // Insert the completion
                memcpy(buffer + filename_start_pos + current_filename_len, 
                       match + current_filename_len, add_len);
                
                *len += add_len;
                *cursor = filename_start_pos + match_len;
            }
        } else {
            // Regular completion (command or simple filename)
            int add_len = match_len - word_len;
            
            if (add_len > 0 && *len + add_len < MAX_LINE - 1) {
                // Make room for the completion
                memmove(buffer + word_end + add_len, buffer + word_end, *len - word_end);
                
                // Insert the completion
                memcpy(buffer + word_start + word_len, match + word_len, add_len);
                
                *len += add_len;
                *cursor = word_start + match_len;
                
                // Add a space after command completion if it's the first word
                if (is_first_word && *len < MAX_LINE - 1) {
                    memmove(buffer + *cursor + 1, buffer + *cursor, *len - *cursor);
                    buffer[*cursor] = ' ';
                    (*len)++;
                    (*cursor)++;
                }
            }
        }
    } else {
        // Multiple matches - find common prefix and show options
        char common[256];
        int common_len = find_common_prefix(&comp, common, sizeof(common));
        
        if (is_path_completion) {
            // Handle path completion for common prefix
            int dir_len = strlen(dir_path);
            if (strcmp(dir_path, "/") != 0) dir_len++; // Add 1 for the slash, except for root
            else dir_len = 1; // Root directory case
            
            int filename_start_pos = word_start + dir_len;
            int current_filename_len = strlen(filename_prefix);
            
            if (common_len > current_filename_len) {
                int add_len = common_len - current_filename_len;
                
                if (*len + add_len < MAX_LINE - 1) {
                    // Make room for the completion
                    memmove(buffer + word_end + add_len, buffer + word_end, *len - word_end);
                    
                    // Insert the common prefix
                    memcpy(buffer + filename_start_pos + current_filename_len, 
                           common + current_filename_len, add_len);
                    
                    *len += add_len;
                    *cursor = filename_start_pos + common_len;
                }
            }
        } else {
            // Regular completion
            if (common_len > word_len) {
                int add_len = common_len - word_len;
                
                if (*len + add_len < MAX_LINE - 1) {
                    // Make room for the completion
                    memmove(buffer + word_end + add_len, buffer + word_end, *len - word_end);
                    
                    // Insert the common prefix
                    memcpy(buffer + word_start + word_len, common + word_len, add_len);
                    
                    *len += add_len;
                    *cursor = word_start + common_len;
                }
            }
        }
        
        // Show all possibilities
        // Temporarily disable raw mode for proper output formatting
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        
        write(STDOUT_FILENO, "\n", 1);
        for (int i = 0; i < comp.count; i++) {
            write(STDOUT_FILENO, comp.matches[i], strlen(comp.matches[i]));
            if ((i + 1) % 4 == 0 || i == comp.count - 1) {
                write(STDOUT_FILENO, "\n", 1);
            } else {
                write(STDOUT_FILENO, "\t", 1);
            }
        }
        
        // Re-enable raw mode
        enableRawMode();
    }
    
    free_completion(&comp);
}

int main(int argc, char **argv)
{
    char bufferIn[MAX_LINE];
    char cwd[128];

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    int shellPid = getpid();

    // Set up signal handlers
    struct sigaction sa;
    
    // Set up SIGCHLD handler to reap children
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    
    // Set up SIGINT handler
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    
    // Set up SIGTSTP handler
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
    
    // Ignore SIGTTOU to prevent stopping when we call tcsetpgrp
    signal(SIGTTOU, SIG_IGN);

    printf("User shell v0.7 (pid:%d)!\n", shellPid);
    fflush(stdout);
    
    // Make sure the shell is in the foreground process group
    tcsetpgrp(STDIN_FILENO, shellPid);

    enableRawMode();

    while (1)
    {
        // Initialize cwd before each command
        if (getcwd(cwd, sizeof(cwd)) == NULL)
        {
            strcpy(cwd, "unknown");
        }

        read_line(bufferIn);
        
        // Null-terminate the input
        int len = strlen(bufferIn);
        
        // Check for background operation
        g_background = 0;
        if (len > 1 && bufferIn[len-1] == '&')
        {
            g_background = 1;
            bufferIn[len-1] = '\0';
            len--;
        }

        // Skip empty commands
        if (len == 0)
        {
            continue;
        }

        // Temporarily disable raw mode while executing commands
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        
        // Add command to history
        add_to_history(bufferIn);

        char** argArray = createArray(bufferIn);
        int argArrayCount = getArrayItemCount(argArray);

        const char* exe = bufferIn;
        if (argArrayCount > 0)
        {
            exe = argArray[0];
        }

        printf("\r");
        fflush(stdout);

        BuiltinCommand * command_handler = get_command_handler(exe);

        if (NULL != command_handler)
        {
            command_handler->Function(argArray);
        }
        else
        {
            // Find the full path of the command using PATH
            const char* full_path = find_command_path(exe);
            if (full_path != NULL)
            {
                int result = run_command(full_path, argArray, environ);
                if (result < 0)
                {
                    printf("Could not execute: %s\n", exe);
                    fflush(stdout);
                }
            }
            else
            {
                printf("Command not found: %s\n", exe);
                fflush(stdout);
            }
        }
    
        destroyArray(argArray);
        
        // Re-enable raw mode for next command
        enableRawMode();
    }

    return 0;
}
