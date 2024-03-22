#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int count = getprocs();
    printf("There are %d active processes.\n", count);
    exit(0);
}