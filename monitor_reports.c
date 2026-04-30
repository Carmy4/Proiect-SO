#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>


void creare_monitor_pid(const char * district){
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/.monitor_pid", district);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;

    printf("Fisierul .monitor_pid creat cu succes!");
    char buffer[256];
    pid_t p= getpid();
    int lungime = snprintf(buffer, sizeof(buffer), "%s",p);
    write(fd,buffer,lungime);

}