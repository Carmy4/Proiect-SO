#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> //ne da acces la O_CREAT, O_WRONLY
#include <unistd.h>

typedef struct report {
    int ID;
    char inspector_name[32];
    float latitude, longitude;
    char category[16];
    int severity_level;
    time_t timestamp;
    char description[128];
} report;

//variabile globale
char current_role[32] = "";
char current_user[32] = "";

//functia care traduce permisiunile
//transform permisiunile din biti in text "rw-rw-r"
void print_permissions(mode_t mode) {
    //initial nimeni nu face nimic, totul e blocat
    char p[10] = "---------";

    //verific ce are voie sa faca proprietarul (managerul)
    //folosesc & ca sa verific daca bitul pentru o anumita permisiune (ex read) e activ in varaibila mea mode
    if (mode & S_IRUSR) p[0] = 'r';
    if (mode & S_IWUSR) p[1] = 'w';
    if (mode & S_IXUSR) p[2] = 'x';

    //verific ce are voie sa faca grupul (inspectorii)
    if (mode & S_IRGRP) p[3] = 'r';
    if (mode & S_IWGRP) p[4] = 'w';
    if (mode & S_IXGRP) p[5] = 'x';

    //verific ce permisiuni au altii (restul lumii)
    if (mode & S_IROTH) p[6] = 'r';
    if (mode & S_IWOTH) p[7] = 'w';
    if (mode & S_IXOTH) p[8] = 'x';
    printf("%s", p);
}

//functia care noteaza toate actiunile
// scriu in logged_district tot ce fac. Daca sunt inspector, dau drop direct.
void log_action(const char* district, const char* action) {
    if (strcmp(current_role, "inspector") == 0) return; // inspectorii nu scriu in log

    //construiesc adresa unde se afla log-ul pentru cartierul primit
    char filepath[256];
    sprintf(filepath, "%s/logged_district", district);

    // Deschid fisierul
    // O_WRONLY = il deschid doar ca sa bag date in el, nu citesc
    // O_CREAT = daca cineva a sters log-ul din greseala, il creez eu acum la loc
    // O_APPEND = nu suprascriu, ci ma duc fix la coada fisierului si scriu acolo, ca sa pastrez istoricul
    //fac fisierul cu permisiune 644 rw-r-r, oricine poate citi, doar managerul poate scrie
    int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);

    //daca s-a deschis cu succes
    if (fd != -1) {
        char buffer[256];
        // construiesc propozitia pe care o s-o lipesc in fisier
        // adaug timpul in secunde (time(NULL)),userul si actiunea lui
        sprintf(buffer, "%ld\n%s\n%s\n", time(NULL), current_user, action);
        write(fd, buffer, strlen(buffer));
        close(fd);
        chmod(filepath, 0644); // fortez permisiunea ceruta
    }
}

// functie pentru a pregati folderul si fisierele necesare pentru un cartier
void setup_district(const char* district) {
    //stat aduce metadatele fisierului(marime, existenta, permisiuni)
    struct stat st;
    char filepath[256];

    // 1. folderul cartierului (req: 750)
    // stat returneaza -1 daca folderul nu exista pe disc
    if (stat(district, &st) == -1) {
        mkdir(district, 0750); // creez folderul
        chmod(district, 0750); // pun permisiunea ceruta
    }

    // 2. fisierul binar cu rapoarte (664)
    sprintf(filepath, "%s/reports.dat", district);
    // o_creat = il face daca lipseste. o_rdwr = read/write. o_append = adauga la coada
    int fd = open(filepath, O_CREAT | O_RDWR | O_APPEND, 0664);
    if (fd != -1) close(fd); // il inchid instant, voiam doar sa ma asigur ca exista
    chmod(filepath, 0664);

    // 3. fisierul de configurare (640)
    sprintf(filepath, "%s/district.cfg", district);
    if (stat(filepath, &st) == -1) {
        fd = open(filepath, O_CREAT | O_WRONLY, 0640);
        if (fd != -1) {
            // daca abia l-am creat, ii scriu o valoare de pornire (pragul 2)
            write(fd, "severity_threshold=2\n", 21);
            close(fd);
        }
    }
    chmod(filepath, 0640);

    // 4. jurnalul (644)
    sprintf(filepath, "%s/logged_district", district);
    fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd != -1) close(fd);
    chmod(filepath, 0644);

    // 5. scurtatura (symlink)
    char symlink_name[256];
    sprintf(symlink_name, "active_reports-%s", district);

    // lstat se uita fix la scurtatura, nu la fisierul original
    // daca returneaza 0, inseamna ca exista deja o scurtatura (poate una stricata/veche).
    if (lstat(symlink_name, &st) == 0) {
        unlink(symlink_name); // o stergem
    }
    // facem scurtatura proaspata care arata spre fisierul reports.dat
    symlink(filepath, symlink_name);
}

// functiile de filtrare generate de ai

// sparge textul (ex: "severity:>=:2") in 3 variabile: field, op, value
int parse_condition(const char *input, char *field, char *op, char *value) {
    //%[^:] citeste tot textul pana da de caracterul :
    if (sscanf(input, "%[^:]:%[^:]:%s", field, op, value) == 3) return 1;
    return 0; // a esuat formatul
}

// verifica daca un raport corespunde conditiei de mai sus
int match_condition(report *r, const char *field, const char *op, const char *value) {
    // daca verificam severitatea
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);

        // facem comparatia in functie de operator
        if (strcmp(op, "==") == 0) return r->severity_level == val;
        if (strcmp(op, ">=") == 0) return r->severity_level >= val;
        if (strcmp(op, "<=") == 0) return r->severity_level <= val;
    }
    // daca verificam categoria
    else if (strcmp(field, "category") == 0) {
        // compar texte cu strcmp. daca strcmp da 0, inseamna ca sunt identice
        if (strcmp(op, "==") == 0) return strcmp(r->category, value) == 0;
    }

    return 0; // daca nu s-a potrivit nimic, raportul asta e respins de filtru
}

// functia prin care adaug un raport nou in fisierul binar.
void cmd_add(const char* district) {
    // mai intai ma asigur ca folderul si fisierele exista, apeland functia de setup.
    setup_district(district);

    char filepath[256];
    sprintf(filepath, "%s/reports.dat", district);

    // deschid fisierul in mod scriere (O_WRONLY) si setez O_APPEND ca sa adaug
    // datele noi fix la sfarsitul fisierului, fara sa suprascriu datele anterioare.
    int fd = open(filepath, O_WRONLY | O_APPEND);
    if (fd == -1) return;

    report r;

    // pentru a genera un ID unic si secvential (1, 2, 3...), aflu marimea totala a fisierului in byti folosind stat().
    //impart aceasta marime la dimensiunea fixa a unei structuri 'report'
    //rezultatul imi da numarul de rapoarte deja existente in fisier
    //adun 1 si obtin ID-ul pentru noul meu raport.
    struct stat st;
    stat(filepath, &st);
    r.ID = (st.st_size / sizeof(report)) + 1;

    // preiau timpul curent de la sistem si numele din argumentele liniei de comanda.
    r.timestamp = time(NULL);
    strcpy(r.inspector_name, current_user);

    // citesc datele de la tastatura si le stochez direct in membrii structurii.
    printf("X (latitude): "); scanf("%f", &r.latitude);
    printf("Y (longitude): "); scanf("%f", &r.longitude);
    printf("Category (road/lighting/flooding/other): "); scanf("%15s", r.category);
    printf("Severity level (1/2/3): "); scanf("%d", &r.severity_level);

    //elimin \n
    getchar();
    printf("Description: ");
    fgets(r.description, 128, stdin);

    // elimin caracterul '\n' introdus de fgets la finalul sirului.
    r.description[strcspn(r.description, "\n")] = 0;

    // scriu toata structura in fisierul binar folosind un singur apel write().
    write(fd, &r, sizeof(report));
    close(fd);

    // formatez string-ul de actiune si il trimit catre functia de logging.
    char actiune[64];
    sprintf(actiune, "%s add", current_role);
    log_action(district, actiune);
}

// functia prin care afisez sub forma de lista scurta toate rapoartele.
void cmd_list(const char* district) {
    char filepath[256];
    sprintf(filepath, "%s/reports.dat", district);

    struct stat st;
    // daca stat() returneaza -1, inseamna ca fisierul nu a putut fi accesat.
    if (stat(filepath, &st) == -1) {
        printf("Nu exista date pt districtul %s\n", district);
        return;
    }

    // afisez permisiunile formatate si
    // metadatele fisierului (marime si data ultimei modificari).
    print_permissions(st.st_mode);
    printf(" | Size: %ld bytes | Last mod: %s", st.st_size, ctime(&st.st_mtime));

    // deschid fisierul in mod exclusiv de citire.
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;

    while (read(fd, &r, sizeof(report)) > 0) {
        printf("[%d]  %s | Cat: %s | Sev: %d\n", r.ID, r.inspector_name, r.category, r.severity_level);
    }
    close(fd);
    log_action(district, "list");
}

// functia prin care afisez toate detaliile unui singur raport.
void cmd_view(const char* district, int id_cautat) {
    char filepath[256];
    sprintf(filepath, "%s/reports.dat", district);

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;
    int gasit = 0; // folosesc un flag pentru a verifica daca raportul exista.

    // parcurg fisierul secvential.
    while (read(fd, &r, sizeof(report)) > 0) {
        // verific daca ID-ul raportului curent corespunde cu parametrul cerut.
        if (r.ID == id_cautat) {
            // afisez campurile
            //folosesc ctime() pentru a converti formatul timestamp (secunde) intr-o data calendaristica.
            printf("\n=== Raport %d ===\n", r.ID);
            printf("Inspector: %s\n", r.inspector_name);
            printf("GPS: %.4f, %.4f\n", r.latitude, r.longitude);
            printf("Categorie: %s | Severitate: %d\n", r.category, r.severity_level);
            printf("Data: %s", ctime(&r.timestamp));
            printf("Descriere: %s\n=================\n\n", r.description);

            gasit = 1;
            break;
        }
    }

    if(!gasit) {
        printf("Raportul %d nu a fost gasit.\n", id_cautat);
    }

    close(fd);
    log_action(district, "view");
}

// functia prin care sterg fizic un raport din fisierul binar.
// operatiunea este permisa exclusiv rolului de manager.
void cmd_remove_report(const char* district, int id_de_sters) {
    // verificarea restrictiei de securitate
    if (strcmp(current_role, "manager") != 0) {
        printf("Eroare: Doar managerii pot sterge rapoarte.\n");
        return;
    }

    char filepath[256];
    sprintf(filepath, "%s/reports.dat", district);

    // deschid in modul o_rdwr (read/write) deoarece trebuie sa citesc fisierul
    // pentru a localiza raportul, apoi sa suprascriu datele in interiorul lui.
    int fd = open(filepath, O_RDWR);
    if (fd == -1) return;

    report r;
    // variabila in care voi stoca adresa (offset-ul) unde incepe raportul gasit
    off_t pozitie_stearsa = 0;
    int gasit = 0;

    // 1. faza de cautare: parcurg fisierul secvential
    while (read(fd, &r, sizeof(report)) > 0) {
        if (r.ID == id_de_sters) {
            gasit = 1;
            // am gasit id-ul. in acest moment cursorul se afla dupa raportul citit
            // folosesc lseek cu o valoare negativa pentru a muta cursorul inapoi la inceputul raportului
            //salvez aceasta pozitie absoluta in variabila.
            pozitie_stearsa = lseek(fd, -sizeof(report), SEEK_CUR);
            break;
        }
    }

    if (!gasit) {
        printf("Nu am gasit raportul.\n");
        close(fd);
        return;
    }

    report urmatorul;
    // 2. faza de mutare (shift left): setez cursorul la inceputul raportului imediat
    // urmator fata de cel pe care vreau sa il sterg.
    lseek(fd, pozitie_stearsa + sizeof(report), SEEK_SET);

    // citesc pe rand restul rapoartelor din dreapta
    while (read(fd, &urmatorul, sizeof(report)) > 0) {
        // ma intorc cu cursorul la pozitia unde era raportul anterior
        lseek(fd, pozitie_stearsa, SEEK_SET);
        // suprascriu cu datele raportului din dreapta
        write(fd, &urmatorul, sizeof(report));
        // adaug marimea unui raport la variabila de adresa pentru a avansa
        pozitie_stearsa += sizeof(report);
        // sar peste raportul tocmai scris pentru a-l citi pe urmatorul in bucla
        lseek(fd, pozitie_stearsa + sizeof(report), SEEK_SET);
    }

    // 3. faza de trunchiere
    //dupa mutarea datelor la stanga ultimul raport din fisier a ramas duplicat la coada
    //apelez ftruncate() pentru a taia dimensiunea fisierului exact la pozitia unde s-a terminat mutarea datelor.
    ftruncate(fd, pozitie_stearsa);

    close(fd);
    printf("Sters cu succes!\n");
    log_action(district, "manager remove_report");
}

// functia prin care managerul actualizeaza valoarea din fisierul district.cfg
void cmd_update_threshold(const char* district, int prag_nou) {
    if (strcmp(current_role, "manager") != 0) {
        printf("Refuz: Doar managerul face asta!\n");
        return;
    }

    char filepath[256];
    sprintf(filepath, "%s/district.cfg", district);

    struct stat st;
    // verificarea permisiunilor fisierului inainte de scriere
    if (stat(filepath, &st) == 0) {
        // aplic masca '& 0777' pe st.st_mode pentru a izola strict ultimii 9 biti
        // care definesc permisiunile (rwx) si ii compar cu constanta octala 0640.
        if ((st.st_mode & 0777) != 0640) {
            printf("Permisiunile nu sunt 640! Refuz operatiunea.\n");
            return;
        }
    }

    // deschid fisierul folosind o_wrongly si o_trunc.
    //flag-ul o_trunc trunchiaza instantaneu fisierul existent la 0 byti, permitandu-mi
    // sa scriu o valoare complet noua fara a lasa resturi din sirul anterior.
    int fd = open(filepath, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        char buffer[64];
        sprintf(buffer, "severity_threshold=%d\n", prag_nou);
        write(fd, buffer, strlen(buffer));
        close(fd);
        printf("Prag actualizat.\n");
    }
    log_action(district, "manager update_threshold");
}

// functia de listare filtrata, ce integreaza codul generat de ai
void cmd_filter(const char* district, const char* condition) {
    char field[32], op[4], value[32];

    // pasez sirul introdus in linia de comanda (ex: "severity:>=:2") functiei parse.
    // daca returneaza 0, formatarea argumentelor este gresita.
    if (!parse_condition(condition, field, op, value)) {
        printf("Format conditie gresit.\n");
        return;
    }

    char filepath[256];
    sprintf(filepath, "%s/reports.dat", district);

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;
    // execut o citire secventiala standard a fisierului binar
    while (read(fd, &r, sizeof(report)) > 0) {
        // validez fiecare structura 'r' extrasa prin functia de matching ai.
        // daca match_condition returneaza 1, printez continutul.
        if (match_condition(&r, field, op, value)) {
            printf("Gasit -> ID: %d | Cat: %s | Sev: %d\n", r.ID, r.category, r.severity_level);
        }
    }
    close(fd);
    log_action(district, "filter");
}

int main(int argc, char *argv[]) {
   //verificare argumente
    if (argc < 4) {
        printf("Eroare argumente.\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {

        // detectez parametrul role si extrag valoarea lui in variabila globala
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strcpy(current_role, argv[i+1]);
            // incrementez indexul pentru a sari peste valoarea pe care tocmai am citit-o
            i++;
        }
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strcpy(current_user, argv[i+1]);
            i++;
        }

        // detectez comenzile
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            cmd_add(argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            cmd_list(argv[i+1]);
            i++;
        }

        // pentru comenzile complexe ce necesita 2 parametri (cartier si id/valoare),
        // apelez atoi() pe ultimul argument pentru conversie in integer,
        // apoi folosesc i += 2 pentru a face un jump de 2 locatii in vectorul argv.
        else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) {
            cmd_view(argv[i+1], atoi(argv[i+2]));
            i += 2;
        }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) {
            cmd_remove_report(argv[i+1], atoi(argv[i+2]));
            i += 2;
        }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) {
            cmd_update_threshold(argv[i+1], atoi(argv[i+2]));
            i += 2;
        }
        else if (strcmp(argv[i], "--filter") == 0 && i + 2 < argc) {
            cmd_filter(argv[i+1], argv[i+2]);
            i += 2;
        }
    }

    return 0;
}
