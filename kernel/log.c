/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2017, ozkl
 * All rights reserved.
 *
 * This file is licensed under the BSD 2-Clause License.
 * See the LICENSE file in the project root for full license information.
 */

#include "log.h"
#include "common.h"
#include "fs.h"
#include "serial.h"

static File* g_file = NULL;
static BOOL g_log_serial = FALSE;

void log_initialize_serial()
{
  g_log_serial = TRUE;

  log_printf("Logging initialized for serial!\n");
}

void log_initialize(const char* file_name)
{
  if (file_name)
  {
    FileSystemNode* node = fs_get_node(file_name);

    BOOL success = FALSE;

    if (node)
    {
      g_file = fs_open(node, 0);

      if (g_file)
      {
        log_printf("Logging initialized for %s!\n", file_name);
        success = TRUE;
      }
    }

    if (!success)
    {
      log_printf("Logging failed to initialize for %s!!\n", file_name);
    }
  }
}

void log_printf(const char *format, ...)
{
    char **arg = (char **) &format;
    char c;
    char buf[20];
    char buffer[512];

    int buffer_index = 0;

    __builtin_va_list vl;
    __builtin_va_start(vl, format);

    while ((c = *format++) != 0)
      {
        if (buffer_index > 510)
        {
            break;
        }

        if (c != '%')
          buffer[buffer_index++] = c;
        else
          {
            char *p;

            c = *format++;
            switch (c)
              {
              case 'x':
                 buf[0] = '0';
                 buf[1] = 'x';
                 //itoa (buf + 2, c, *((int *) arg++));
                 itoa (buf + 2, c, __builtin_va_arg(vl, int));
                 p = buf;
                 goto string;
                 break;
              case 'd':
              case 'u':
                //itoa (buf, c, *((int *) arg++));
                itoa (buf, c, __builtin_va_arg(vl, int));
                p = buf;
                goto string;
                break;

              case 's':
                //p = *arg++;
                p = __builtin_va_arg(vl, char*);
                if (! p)
                  p = "(null)";

              string:
                while (*p)
                  buffer[buffer_index++] = (*p++);
                break;

              default:
                //buffer[buffer_index++] = (*((int *) arg++));
                buffer[buffer_index++] = __builtin_va_arg(vl, int);
                break;
              }
          }
      }

    buffer[buffer_index] = '\0';

    if (g_log_serial)
    {
      serial_printf(buffer);
    }

    if (g_file)
    {
        fs_write(g_file, strlen(buffer), (uint8_t*)buffer);
    }

    __builtin_va_end(vl);
}
