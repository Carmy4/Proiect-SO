#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct report {
    int ID;
    char inspector_name[31];
    float latitude, longitude;
    char category[12];
    int severity_level;
    time_t timestamp;
    char description[101];
} report;

//functie pentru a crea fisierele cerute
void creaza_fisier_gol(const char* cale, int permisiuni) {
    //creez fisierul daca nu exista, permisiuniune sa scriu in el
    //si pun tot ce scriu la sfarsitul lui
    int fd = open(cale, O_CREAT | O_WRONLY | O_APPEND, permisiuni);

    // daca s-a deschis cu succes il inchid, ma intereseaza doar sa il creez
    if(fd != -1) {
        close(fd);
    }

    // ii dau permisiunile cerute
    chmod(cale, permisiuni);
}


void setup_district(char* district) {
    char filepath[150];

    // creez folderul districtului
    mkdir(district, 0750);
    chmod(district, 0750);

    //creare reports.dat (664)
    sprintf(filepath, "%s/reports.dat", district);
    creaza_fisier_gol(filepath, 0664);

    //creare district.cfg (640)
    sprintf(filepath, "%s/district.cfg", district);
    creaza_fisier_gol(filepath, 0640);

    // creare (644)
    sprintf(filepath, "%s/logged_district", district);
    creaza_fisier_gol(filepath, 0644);
}
void adauga_raport(char* district, char* user_name) {
    report r;
    char filepath[150];

    // completez datele care se pun automat
    r.ID = time(NULL); // folosim timpul curent ca ID unic
    r.timestamp = time(NULL); // timpul la care s-a facut raportul
    strcpy(r.inspector_name, user_name); // salvam numele celui care a dat comanda

    // cerem restul datelor de la tastatura
    printf("X: ");
    scanf("%f", &r.latitude);

    printf("Y: ");
    scanf("%f", &r.longitude);

    printf("Category (road/lighting/flooding/other): ");
    scanf("%11s", r.category);

    printf("Severity level (1/2/3): ");
    scanf("%d", &r.severity_level);

    // pentru descriere, curatam enter-ul ramas
    getchar();
    printf("Description: ");
    fgets(r.description, 101, stdin); // citim toata propozitia
    r.description[strcspn(r.description, "\n")] = 0; // taiem enter-ul de la final

    // salvam  in fisierul binar
    sprintf(filepath, "%s/reports.dat", district);

    // deschidem fisierul doar pentru scriere si adaugare la final
    int fd = open(filepath, O_WRONLY | O_APPEND);
    if (fd != -1) {
        // write ia tot struct-ul si il scrie dintr-o lovitura
        write(fd, &r, sizeof(report));
        close(fd);
        printf("Raportul a fost salvat cu succes in %s!\n", filepath);
    } else {
        printf("Eroare: Nu am gasit fisierul reports.dat!\n");
    }
}
int main(int argc, char *argv[]) {
    //variabilele in care salvez argumentele date de utilizator
    char role[20] = "";
    char user[31] = "";
    char district[50] = "";
    char action[20] = "";

    // Trecem prin toate argumentele primite din terminal
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strcpy(role, argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strcpy(user, argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            strcpy(action, "add");
            strcpy(district, argv[i+1]); // numele districtului (ex: downtown)
            setup_district(district);
            adauga_raport(district, user);
            i++;
        }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            strcpy(action, "list");
            strcpy(district, argv[i+1]);
            i++;
        }
    }


    return 0;
}
