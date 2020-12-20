#ifndef SERIAL_H
#define SERIAL_H

void initializeSerial();
void writeSerial(char a);
char readSerial();
void Serial_PrintF(const char *format, ...);
void Serial_Write(const char *buffer, int n);

#endif // SERIAL_H
