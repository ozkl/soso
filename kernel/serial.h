#ifndef SERIAL_H
#define SERIAL_H

void serial_initialize();
void serial_initialize_file_device();
void serial_printf(const char *format, ...);

#endif // SERIAL_H
