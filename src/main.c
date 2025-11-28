#include "../include/sshmgr.h"


int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage();
        return 1;
    }

    ssh_main(argv[1], argv[2]);
    return 0;
}
