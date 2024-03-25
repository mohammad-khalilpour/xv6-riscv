#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void
counting_process(){
    int pid = getpid();
    for (int i = 0; i < 10000; i++){
        printf("process pid %d: %d\n",pid, i);
    }
}

int
main(int argc, char *argv[])
{
    int i;

    for (i = 0; i < 5; i++) {
        int pid = priority_fork(1);

        if (pid < 0) {
            printf("fork failed\n");
            exit(-1);
        } else if (pid == 0) {
            counting_process();
            printf("process %d finished\n", getpid());
            exit(1);
        }
    }
    for (i = 0; i < 5; i++) {
        int pid = priority_fork(0);

        if (pid < 0) {
            printf("fork failed\n");
            exit(-1);
        } else if (pid == 0) {
            counting_process();
            printf("process %d finished\n", getpid());
            exit(1);
        }
    }


    for (i = 0; i < 10; i++) {
        wait(0);
    }

    exit(1);

    return 0;
}