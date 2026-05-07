#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>


void creare_monitor_pid(const char * district){
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/.monitor_pid", district);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return;

    printf("Fisierul .monitor_pid creat cu succes!");
    char buffer[256];
    pid_t p= getpid();
    int lungime = snprintf(buffer, sizeof(buffer), "%d",p);
    write(fd,buffer,lungime);

    close(fd);

}

//functia pentru semnale
void handle_signals(int sig){
    if(sig==SIGUSR1){
        const char *msg="A fost adaugat un raport nou!";
        write(STDOUT_FILENO, msg, strlen(msg));
    }
    else if(sig==SIGINT){
        const char *msg="Sigint primit.Sterg fisierul si ies!";
        write(STDOUT_FILENO, msg, strlen(msg));
        unlink(".monitor_pid");
    }


}

int main(){
    creare_monitor_pid(".");
    struct sigaction action_ignore;
    memset(&action_ignore, 0x00, sizeof(struct sigaction));
    action_ignore.sa_handler = handle_signals;

    if (sigaction(SIGUSR1, &action_ignore, NULL) < 0)
    {
      perror("sigaction SIGUSR1 ignore");
      exit(-1);	     
    }
    struct sigaction action_ignore2;
    memset(&action_ignore2, 0x00, sizeof(struct sigaction));
    action_ignore2.sa_handler = handle_signals;
    if (sigaction(SIGINT, &action_ignore2, NULL) < 0)
    {
      perror("sigaction SIGINT ignore");
      exit(-1);	     
    }

}

