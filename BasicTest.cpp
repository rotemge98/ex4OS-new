#include "VirtualMemory.h"

#include <cstdio>
#include "PhysicalMemory.h"
// #include <cassert>

int main(int argc, char **argv) {
    printf("###### step 1 ######\n");
    VMinitialize();
    printRam();

    printf("###### step 2 ######\n");
    VMwrite(13, 3);
    printRam();

    printf("###### step 3 ######\n");
    word_t value;
    VMread(6, &value);
    printRam();

    printf("###### step 4 ######\n");
    VMread(31, &value);
    printRam();

    printf("###### done ######\n");

    return 0;
}
