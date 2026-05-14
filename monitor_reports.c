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
#define PID_FILE ".monitor_pid"

// flag volatile pentru a semnala buclei principale ca trebuie sa iasa
// sig_atomic_t garanteaza acces atomic din handler
static volatile sig_atomic_t running = 1;

void creare_monitor_pid(const char *district) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/.monitor_pid", district);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return;

    printf("Fisierul .monitor_pid creat cu succes!\n");
    char buffer[256];
    pid_t p = getpid();
    int lungime = snprintf(buffer, sizeof(buffer), "%d", p);
    write(fd, buffer, lungime);

    close(fd);
}

// handler pentru semnale
// folosim doar functii async-signal-safe (write, unlink, time, ctime)
// nu folosim printf din handler - nu este async-signal-safe
void handle_signals(int sig) {
    if (sig == SIGUSR1) {
        // obtinem timestamp-ul pentru a sti cand a fost adaugat raportul
        time_t now = time(NULL);
        char *t = ctime(&now);

        const char *msg = "[monitor] A fost adaugat un raport nou! ";
        write(STDOUT_FILENO, msg, strlen(msg));

        // afisam timpul (ctime include '\n' la final)
        if (t) write(STDOUT_FILENO, t, strlen(t));
    }
    else if (sig == SIGINT) {
        const char *msg = "\n[monitor] SIGINT primit. Sterg fisierul .monitor_pid si ies!\n";
        write(STDOUT_FILENO, msg, strlen(msg));

        // stergem fisierul pid inainte de iesire
        unlink(".monitor_pid");

        // semnalam buclei principale sa se opreasca
        running = 0;
    }
}

int main() {
    // scriem PID-ul in .monitor_pid la pornire
    int pid_check=open(PID_FILE, O_RDONLY);
    if(pid_check!=-1){
        char pid_aux[32];
        read(pid_check,pid_aux,sizeof(pid_aux));
        close(pid_check);
        printf("Eroare: monitor is already running with PID %s\n",pid_aux);
        fflush(stdout);
        exit(1);
    }
    
    creare_monitor_pid(".");
   


    // configuram handler-ul pentru SIGUSR1
    struct sigaction action_usr1;
    memset(&action_usr1, 0x00, sizeof(struct sigaction));
    action_usr1.sa_handler = handle_signals;
    sigemptyset(&action_usr1.sa_mask);
    action_usr1.sa_flags = SA_RESTART; // reluam apelurile de sistem intrerupte

    if (sigaction(SIGUSR1, &action_usr1, NULL) < 0) {
        perror("sigaction SIGUSR1");
        exit(-1);
    }

    // configuram handler-ul pentru SIGINT
    struct sigaction action_int;
    memset(&action_int, 0x00, sizeof(struct sigaction));
    action_int.sa_handler = handle_signals;
    sigemptyset(&action_int.sa_mask);
    action_int.sa_flags = 0; // fara SA_RESTART - vrem ca pause() sa fie intrerupt

    if (sigaction(SIGINT, &action_int, NULL) < 0) {
        perror("sigaction SIGINT");
        exit(-1);
    }

    printf("[monitor] Pornit cu PID %d. Astept semnale...\n", (int)getpid());
    fflush(stdout);

    // bucla principala - asteptam semnale
    // pause() suspenda procesul pana cand primeste un semnal
    while (running) {
        pause();
    }

    // am iesit din bucla dupa SIGINT
    printf("[monitor] Oprit.\n");
    return 0;
}