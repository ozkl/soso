#ifndef PTDRIVER_H
#define PTDRIVER_H

#include "fs.h"

void initializePseudoTerminal();

FileSystemNode* createPseudoTerminal();
int getSlavePath(FileSystemNode* masterNode, char* buffer, uint32 bufferSize);

#endif // PTDRIVER_H
