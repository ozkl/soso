#include "log.h"
#include "common.h"
#include "fs.h"

static File* g_file = NULL;

void log_initialize(const char* file_name)
{
    FileSystemNode* node = fs_get_node(file_name);

    if (node)
    {
      g_file = fs_open(node, 0);
    }
}

void log_printf(const char *format, ...)
{
    char **arg = (char **) &format;
    char c;
    char buf[20];
    char buffer[512];

    int buffer_index = 0;

    //arg++;
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

    if (g_file)
    {
        fs_write(g_file, strlen(buffer), (uint8_t*)buffer);
    }

    __builtin_va_end(vl);
}
