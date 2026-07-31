/*#include <cstring>
#include <iostream>

#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
    Initialize the Run Copy of Disk 
    Disk disk_run;

    // These are used in later stages.
    // StaticBuffer buffer;
    // OpenRelTable cache;

    // Create a 2048-byte buffer.
    unsigned char buffer[BLOCK_SIZE];

    // Read block 7000 into memory.
    Disk::readBlock(buffer, 7000);

    // Write "hello" starting at byte offset 20.
    char message[] = "hello";
    memcpy(buffer + 20, message, 6);

    // Write the modified block back to disk.
    Disk::writeBlock(buffer, 7000);

    // Read the block again into a second buffer.
    unsigned char buffer2[BLOCK_SIZE];
    char message2[6];

    Disk::readBlock(buffer2, 7000);

    // Copy the message back from the block.
    memcpy(message2, buffer2 + 20, 6);

    // Print the message.
    std::cout << message2 << std::endl;

    // We will use the frontend from Stage 2 onwards.
    // return FrontendInterface::handleFrontend(argc, argv);

    return 0;
}*/
/**/
#include <iostream>

#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {
    /* Initialize the Run Copy of Disk */
    Disk disk_run;

    // These will be used in later stages.
    // StaticBuffer buffer;
    // OpenRelTable cache;

    unsigned char buffer[BLOCK_SIZE];

    // Read Block Allocation Map (Block 0)
    Disk::readBlock(buffer, 0);

    // Print the first 8 bytes
    for (int i = 0; i < 8; i++) {
        std::cout << (int)buffer[i] << " ";
    }

    std::cout << std::endl;

    // We will enable the frontend in later stages.
    // return FrontendInterface::handleFrontend(argc, argv);

    return 0;
}