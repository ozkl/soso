#ifndef CONSOLE_H
#define CONSOLE_H

#include "common.h"

#define TERMINAL_COUNT 10

typedef struct Terminal Terminal;

typedef struct FileSystemNode FileSystemNode;

extern Terminal* g_active_terminal;

void console_initialize(BOOL graphicMode);

void console_send_key(uint8_t scancode);

void console_set_active_terminal_index(uint32_t index);

void console_set_active_terminal(Terminal* terminal);

Terminal* console_get_terminal(uint32_t index);

Terminal* console_get_terminal_by_master(FileSystemNode* master_node);

Terminal* console_get_terminal_by_slave(FileSystemNode* slave_node);

#endif // CONSOLE_H
