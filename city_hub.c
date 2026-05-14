#include <stdio.h>
#include <unistd.h>

void start_monitor(){
    int fd[2];
    pipe(fd);
    pid_t hub_mon=fork();
    if(hub_mon==0)
    {
        //child
        close(fd[0]);
        dup2(fd[1],STDOUT_FILENO);
        close(fd[1]);
        execlp("./monitor_reports","./monitor_reports",NULL);
        
           
       

    }
    else{
        //parinte
        close(fd[1]);
        char b[128];
        while(1){
            int n=read(fd[0],b,sizeof(b)-1);
            b[n]='\0';
            printf(" %s",b);

            fflush(stdout);

        }
    }
}
int main(){
    return 0;
}