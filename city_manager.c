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
    char inspector_name[32];
    float latitude, longitude;
    char category[16];
    int severity_level;
    time_t timestamp;
    char description[128];
} report;

// variabile globale pentru a retine cine foloseste programul momentan
char current_role[32] = "";
char current_user[32] = "";

// transformam drepturile octale in format text de tip "rw-r--r--"
// aplicam masca cu si logic (&) ca sa verificam starea fiecarui bit de permisiune
void print_permissions(mode_t mode) {
    char p[10] = "---------";

    // verificam bitii specifici proprietarului fisierului
    if (mode & S_IRUSR) p[0] = 'r';
    if (mode & S_IWUSR) p[1] = 'w';
    if (mode & S_IXUSR) p[2] = 'x';

    // verificam bitii specifici grupului din care face parte fisierul
    if (mode & S_IRGRP) p[3] = 'r';
    if (mode & S_IWGRP) p[4] = 'w';
    if (mode & S_IXGRP) p[5] = 'x';

    // verificam bitii specifici altor utilizatori
    if (mode & S_IROTH) p[6] = 'r';
    if (mode & S_IWOTH) p[7] = 'w';
    if (mode & S_IXOTH) p[8] = 'x';

    printf("%s", p);
}

// extragem metadatele fisierului cu stat() pentru a verifica permisiunile
// managerul este tratat ca proprietar (owner), iar inspectorii ca membri ai grupului
int check_permission(const char *filepath, char mode_cerut) {
    struct stat st;

    // daca fisierul nu a fost creat inca, permitem operatia pentru a nu bloca rularea
    if (stat(filepath, &st) == -1) {
        printf("eroare: fisierul '%s' nu a fost gasit.\n", filepath);
        return 0;
    }

    mode_t m = st.st_mode;

    if (strcmp(current_role, "manager") == 0) {
        if (mode_cerut == 'r') return (m & S_IRUSR) ? 1 : 0;
        if (mode_cerut == 'w') return (m & S_IWUSR) ? 1 : 0;
    }
    else if (strcmp(current_role, "inspector") == 0) {
        if (mode_cerut == 'r') return (m & S_IRGRP) ? 1 : 0;
        if (mode_cerut == 'w') return (m & S_IWGRP) ? 1 : 0;
    }
    else {
        printf("eroare: rol necunoscut la apelarea programului.\n");
        return 0;
    }

    return 0;
}

// salvam actiunea curenta in jurnal
// folosim flagul O_APPEND pentru a adauga datele la finalul fisierului fara a suprascrie
void log_action(const char *district, const char *action) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/logged_district", district);

    int fd = open(filepath, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%ld %s %s %s\n",
             (long)time(NULL), current_user, current_role, action);
    write(fd, buffer, strlen(buffer));
    close(fd);
}

// cream fisierele necesare inainte de rularea efectiva a operatiilor
void setup_district(const char *district) {
    struct stat st;
    char filepath[256];

    // cream directorul cu permisiuni de citire/scriere/executie pt manager si r/x pt inspector
    if (stat(district, &st) == -1) {
        mkdir(district, 0750);
        chmod(district, 0750);
    }

    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);
    int fd = open(filepath, O_CREAT | O_RDWR | O_APPEND, 0664);
    if (fd != -1) close(fd);
    chmod(filepath, 0664);

    snprintf(filepath, sizeof(filepath), "%s/district.cfg", district);
    if (stat(filepath, &st) == -1) {
        fd = open(filepath, O_CREAT | O_WRONLY, 0640);
        if (fd != -1) {
            write(fd, "severity_threshold=2\n", 21);
            close(fd);
        }
    }
    chmod(filepath, 0640);

    snprintf(filepath, sizeof(filepath), "%s/logged_district", district);
    fd = open(filepath, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd != -1) close(fd);
    chmod(filepath, 0644);

    // stergem si recream scurtatura pentru fisierul de rapoarte
    // apelam lstat() pentru a detecta corect daca un link este 'atarnat' (fara destinatie)
    char symlink_name[256];
    char symlink_target[256];

    snprintf(symlink_name,   sizeof(symlink_name),   "active_reports-%s", district);
    snprintf(symlink_target, sizeof(symlink_target), "%s/reports.dat", district);

    if (lstat(symlink_name, &st) == 0) unlink(symlink_name);
    symlink(symlink_target, symlink_name);

    if (stat(symlink_name, &st) == -1) {
        printf("avertisment: linkul simbolic %s nu duce nicaieri.\n", symlink_name);
    }
}

// separam campul, operatorul si valoarea din formatul string transmis ca filtru
int parse_condition(const char *input, char *field, char *op, char *value) {
    if (sscanf(input, "%31[^:]:%3[^:]:%31s", field, op, value) == 3) return 1;
    return 0;
}

// aplicam conditiile logice pe structura raportului extras din fisierul binar
int match_condition(report *r, const char *field, const char *op, const char *value) {
    if (strcmp(field, "severity") == 0) {
        int val = atoi(value);
        if (strcmp(op, "==") == 0) return r->severity_level == val;
        if (strcmp(op, "!=") == 0) return r->severity_level != val;
        if (strcmp(op, ">=") == 0) return r->severity_level >= val;
        if (strcmp(op, "<=") == 0) return r->severity_level <= val;
        if (strcmp(op, ">")  == 0) return r->severity_level >  val;
        if (strcmp(op, "<")  == 0) return r->severity_level <  val;
        return 0;
    }
    else if (strcmp(field, "category") == 0) {
        int cmp = strcmp(r->category, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
        return 0;
    }
    else if (strcmp(field, "inspector") == 0) {
        int cmp = strcmp(r->inspector_name, value);
        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
        return 0;
    }
    else if (strcmp(field, "timestamp") == 0) {
        long val = atol(value);
        if (strcmp(op, "==") == 0) return (long)r->timestamp == val;
        if (strcmp(op, "!=") == 0) return (long)r->timestamp != val;
        if (strcmp(op, ">=") == 0) return (long)r->timestamp >= val;
        if (strcmp(op, "<=") == 0) return (long)r->timestamp <= val;
        if (strcmp(op, ">")  == 0) return (long)r->timestamp >  val;
        if (strcmp(op, "<")  == 0) return (long)r->timestamp <  val;
        return 0;
    }

    // evitam excluderea raportului in caz de parametru invalid
    return 1;
}

// inseram structura cu date in fisierul binar
void cmd_add(const char *district) {
    setup_district(district);

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

    if (!check_permission(filepath, 'w')) {
        printf("acces refuzat: lipsesc drepturile de scriere in fisier.\n");
        return;
    }

    int fd = open(filepath, O_WRONLY | O_APPEND);
    if (fd == -1) return;

    report r;
    // folosim memset pt a scrie zero pe toata dimensiunea structurii, evitand memorie reziduala
    memset(&r, 0, sizeof(report));

   // generam un id unic combinand timpul unix cu id-ul procesului curent (pid)
    // getpid() garanteaza unicitatea chiar daca ruleaza multiple add-uri in aceeasi secunda
    r.ID = (int)time(NULL) ^ getpid();
    r.timestamp = time(NULL);
    strncpy(r.inspector_name, current_user, sizeof(r.inspector_name) - 1);
    r.inspector_name[sizeof(r.inspector_name) - 1] = '\0';

    printf("x (latitude): ");   scanf("%f", &r.latitude);
    printf("y (longitude): ");  scanf("%f", &r.longitude);
    printf("category (road/lighting/flooding/other): "); scanf("%15s", r.category);
    printf("severity level (1/2/3): "); scanf("%d", &r.severity_level);

    // golim bufferul de input (stdin) caracter cu caracter pentru a evita intreruperea functiei fgets
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    printf("description: ");
    fgets(r.description, sizeof(r.description), stdin);
    r.description[strcspn(r.description, "\n")] = '\0'; // stergem caracterul enter (\n)

    // serializam si scriem intreg blocul de date in fisier (200 octeti)
    write(fd, &r, sizeof(report));
    close(fd);

    log_action(district, "add");
    printf("inregistrare completa cu id-ul #%d.\n", r.ID);
}

// citim si parcurgem secvential toate rapoartele adaugate
void cmd_list(const char *district) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

    struct stat st;
    if (stat(filepath, &st) == -1) {
        printf("districtul precizat nu contine fisiere de date.\n");
        return;
    }

    if (!check_permission(filepath, 'r')) {
        printf("acces refuzat: lipsesc permisiunile de citire.\n");
        return;
    }

    printf("sursa: reports.dat | drepturi octale: ");
    print_permissions(st.st_mode);
    printf(" | marime totala: %ld bytes | ultimul update: %s", (long)st.st_size, ctime(&st.st_mtime));

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;
    int nr = 0;

    // bucla proceseaza exact atatia octeti cat masoara structura definita
    while (read(fd, &r, sizeof(report)) == sizeof(report)) {
        printf("[%d] insp: %s | cat: %s | sev: %d | data: %s",
               r.ID, r.inspector_name, r.category, r.severity_level,
               ctime(&r.timestamp));
        nr++;
    }

    if (nr == 0) printf("lista de rapoarte este momentan goala.\n");

    close(fd);
    log_action(district, "list");
}

// afisam in detaliu campurile raportului daca gasim id-ul solicitat
void cmd_view(const char *district, int id_cautat) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

    if (!check_permission(filepath, 'r')) {
        printf("acces refuzat: lipsesc permisiunile de citire.\n");
        return;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;
    int gasit = 0;

    while (read(fd, &r, sizeof(report)) == sizeof(report)) {
        if (r.ID == id_cautat) {
            printf("\n--- date extrase pt id: %d ---\n", r.ID);
            printf("adaugat de: %s\n", r.inspector_name);
            printf("locatie gps: %.4f, %.4f\n", r.latitude, r.longitude);
            printf("tipul problemei: %s | nivel severitate: %d\n", r.category, r.severity_level);
            printf("timestamp: %ld\n", (long)r.timestamp);
            printf("inregistrat la: %s", ctime(&r.timestamp));
            printf("detalii: %s\n", r.description);
            printf("------------------------------\n\n");
            gasit = 1;
            break;
        }
    }

    if (!gasit) printf("id %d nu exista in baza de date.\n", id_cautat);

    close(fd);
    log_action(district, "view");
}

// suprimam logic si fizic un raport din fisier
void cmd_remove_report(const char *district, int id_de_sters) {
    if (strcmp(current_role, "manager") != 0) {
        printf("eroare securitate: stergerea apartine strict managerului.\n");
        return;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

    if (!check_permission(filepath, 'w')) return;

    int fd = open(filepath, O_RDWR);
    if (fd == -1) return;

    report r;
    off_t pozitie_stearsa = -1;
    int gasit = 0;

    // cautam offset-ul la care este stocata structura pe care vrem sa o eliminam
    while (read(fd, &r, sizeof(report)) == sizeof(report)) {
        if (r.ID == id_de_sters) {
            gasit = 1;
            // repozitionam pointerul cu o dimensiune de struct inapoi pt a suprascrie
            off_t pozitie_curenta = lseek(fd, 0, SEEK_CUR);
            pozitie_stearsa = pozitie_curenta - (off_t)sizeof(report);
            break;
        }
    }

    if (!gasit) {
        printf("nu se poate executa: id negasit in structura de date.\n");
        close(fd);
        return;
    }

    report urmatorul;
    off_t poz_scriere = pozitie_stearsa;
    off_t poz_citire  = pozitie_stearsa + (off_t)sizeof(report);

    // mutam consecutiv la stanga toate blocurile de date ramase pt a pastra ordinea
    while (1) {
        lseek(fd, poz_citire, SEEK_SET);
        if (read(fd, &urmatorul, sizeof(report)) != (ssize_t)sizeof(report)) break;

        lseek(fd, poz_scriere, SEEK_SET);
        write(fd, &urmatorul, sizeof(report));

        poz_scriere += sizeof(report);
        poz_citire  += sizeof(report);
    }

    // trunchiem fisierul pt a scoate dublura din capat rezultata in urma mutarii
    ftruncate(fd, poz_scriere);

    close(fd);
    printf("raportul a fost sters definitiv din sistem.\n");
    log_action(district, "remove_report");
}

// actualizam fisierul configurativ, cu o formatare O_TRUNC care inlocuieste direct linia curenta
int cmd_update_threshold(const char *district, int prag_nou) {
    if (strcmp(current_role, "manager") != 0) {
        printf("privilegii respinse pt acest tip de modificare.\n");
        return 0;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/district.cfg", district);

    struct stat st;
    if (stat(filepath, &st) == -1) {
        printf("eroare: fisierul '%s' nu a fost gasit.\n", filepath);
        return 0;
    }

    // ne asiguram ca valoarea ramane setata strict la 0640, dupa cum impune documentatia
    if ((st.st_mode & 0777) != 0640) {
        printf("eroare structura: permisiunea 0640 nu mai este aplicata pe fisierul cfg.\n");
        return 0;
    }

    if (!check_permission(filepath, 'w')) return 0;

    // optiunea O_TRUNC ne scuteste sa cautam pe linie offsetul, goleste direct memoria fisierului text
    int fd = open(filepath, O_WRONLY | O_TRUNC);
    if (fd == -1) return 0;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "severity_threshold=%d\n", prag_nou);
    write(fd, buffer, strlen(buffer));
    close(fd);

    printf("limita actualizata cu succes la valoarea: %d.\n", prag_nou);
    return 1;
}

// rulam operatiile de interogare pe multiple filtre introduse in linie de comanda
void cmd_filter(const char *district, int nr_conditii, char **conditii) {
    if (nr_conditii == 0) {
        printf("lipsesc parametrii necesari filtrarii ex: categorie:==:drumuri.\n");
        return;
    }

    char fields[16][32], ops[16][4], values[16][32];
    int nr_valide = 0;

    for (int i = 0; i < nr_conditii && i < 16; i++) {
        if (parse_condition(conditii[i], fields[i], ops[i], values[i])) {
            nr_valide++;
        } else {
            printf("structura argumentelor e eronata: %s.\n", conditii[i]);
            return;
        }
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/reports.dat", district);

   if (!check_permission(filepath, 'r')) {
        printf("acces refuzat: lipsesc permisiunile de citire.\n");
        return;
    }

    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;

    report r;
    int nr_gasite = 0;

    while (read(fd, &r, sizeof(report)) == sizeof(report)) {
        int match = 1;

        for (int i = 0; i < nr_valide; i++) {
            if (!match_condition(&r, fields[i], ops[i], values[i])) {
                match = 0;
                break; // intrerupem evaluarea la prima conditie neindeplinita
            }
        }

        if (match) {
            printf("id: %d | insp: %s | cat: %s | sev: %d\n",
                   r.ID, r.inspector_name, r.category, r.severity_level);
            nr_gasite++;
        }
    }

    if (nr_gasite == 0) printf("datele filtrului nu genereaza niciun match logic.\n");

    close(fd);
    log_action(district, "filter");
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("lansare incorecta. nu ai oferit numarul de stringuri necesar.\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strncpy(current_role, argv[++i], sizeof(current_role) - 1);
            current_role[sizeof(current_role) - 1] = '\0';
        }
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strncpy(current_user, argv[++i], sizeof(current_user) - 1);
            current_user[sizeof(current_user) - 1] = '\0';
        }
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            cmd_add(argv[++i]);
        }
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            cmd_list(argv[++i]);
        }
        else if (strcmp(argv[i], "--view") == 0 && i + 2 < argc) {
            char *dist = argv[++i];
            int   id   = atoi(argv[++i]);
            cmd_view(dist, id);
        }
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) {
            char *dist = argv[++i];
            int   id   = atoi(argv[++i]);
            cmd_remove_report(dist, id);
        }
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) {
            char *dist = argv[++i];
            int   prag = atoi(argv[++i]);
            if (cmd_update_threshold(dist, prag))
                log_action(dist, "update_threshold");
        }
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc) {
            char *district = argv[++i];
            char **conditii = &argv[i + 1];
            int nr_conditii = 0;

            // numaram parametri pana identificam inceputul de flag urmator cu "--"
            while (i + 1 < argc && !(argv[i + 1][0] == '-' && argv[i + 1][1] == '-')) {
                nr_conditii++;
                i++;
            }

            cmd_filter(district, nr_conditii, conditii);
        }
    }

    return 0;
}
