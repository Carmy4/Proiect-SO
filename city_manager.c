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

    // creare logged_district(644)
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
// functie cu care transform permisiunile din sistem intr-un text citibil (rw-r--r--)
void afiseaza_permisiuni(mode_t mode) {
    // pornesc cu un sir de caractere care reprezinta lipsa totala a permisiunilor
    char p[10] = "---------";

    // parametrul 'mode' vine de la stat(). este un numar care contine toate permisiunile
    // codate la nivel de biti. ca sa verific daca o permisiune anume exista, folosesc '&' (si logic).
    // & suprapune bitii din mode cu bitii constantei (ex: S_IRUSR). daca se potrivesc, rezultatul e adevarat.

    // verific permisiunile pentru owner (managerul proiectului)
    if (mode & S_IRUSR) p[0] = 'r'; // S_IRUSR = read user
    if (mode & S_IWUSR) p[1] = 'w'; // S_IWUSR = write user
    if (mode & S_IXUSR) p[2] = 'x'; // S_IXUSR = execute user

    // verific permisiunile pentru grup (inspectori)
    if (mode & S_IRGRP) p[3] = 'r';
    if (mode & S_IWGRP) p[4] = 'w';
    if (mode & S_IXGRP) p[5] = 'x';

    // verific permisiunile pentru ceilalti utilizatori (others)
    if (mode & S_IROTH) p[6] = 'r';
    if (mode & S_IWOTH) p[7] = 'w';
    if (mode & S_IXOTH) p[8] = 'x';

    // printez rezultatul direct in terminal
    printf("%s", p);
}

// functia care citeste din fisierul binar si imi afiseaza datele
void listeaza_rapoarte(char* district) {
    char filepath[150];

    // generez calea spre fisier lipind numele districtului cu restul adresei
    sprintf(filepath, "%s/reports.dat", district);

    // 'struct stat' este un tip de date predefinit in libraria <sys/stat.h> din C.
    // este o structura special gandita sa stocheze informatii tehnice despre fisiere.
    // eu imi declar o variabila numita 'info' de acest tip, pe care deocamdata o tin goala.
    struct stat info;

    // functia stat() ia adresa fisierului meu si imi umple automat variabila 'info'
    // cu detaliile de pe hard disk (dimensiune, permisiuni), fara ca eu sa fiu nevoit sa folosesc open() inainte.
    // functia stat() returneaza valoarea 0 daca a reusit sa citeasca cu succes de pe disc.
    if (stat(filepath, &info) == 0) {
        // info.st_size imi scoate dimensiunea in byti direct din structura
        printf("Detalii fisier: %ld bytes | Permisiuni: ", info.st_size);
        // info.st_mode contine bitii de permisiuni, asa ca trimit valoarea asta functiei mele de mai sus
        afiseaza_permisiuni(info.st_mode);
        printf("\n------------------------------------------------\n");
    }

    // deschid fisierul in mod O_RDONLY (read only) pentru ca vreau doar sa il parcurg, nu sa scriu in el
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) {
        printf("Eroare: Nu am putut deschide fisierul.\n");
        return;
    }

    // declar o variabila in care o sa extrag datele la fiecare iteratie a buclei
    report r;

    // read(file_descriptor, unde_salvez, cat_citesc)
    // functia read decupeaza din fisier o bucata fixa de byti (sizeof(report) o forteaza sa citeasca fix cat tine structura mea)
    // dupa ce citeste, incarca datele in variabila 'r'. cand nu mai gaseste date in fisier, read() returneaza 0 si bucla se opreste.
    while (read(fd, &r, sizeof(report)) > 0) {
        printf("ID: %d | Inspector: %s | Categoria: %s | Gravitate: %d\n",
               r.ID, r.inspector_name, r.category, r.severity_level);
    }

    // eliberez fisierul din memorie
    close(fd);
}

// functia care scoate un raport din fisierul binar
void sterge_raport(char* district, int id_de_sters, char* rol) {
    // doar rolul de manager are permisiunea sa foloseasca functia
    if (strcmp(rol, "manager") != 0) {
        printf("Eroare: Doar managerul are voie sa stearga rapoarte!\n");
        return;
    }

    char filepath[150];
    sprintf(filepath, "%s/reports.dat", district);

    // de data asta deschid cu O_RDWR (read and write) pentru ca am nevoie sa citesc rapoartele ca sa caut id-ul,
    // dar si sa scriu peste ele cand le mut locatia in fisier
    int fd = open(filepath, O_RDWR);
    if (fd == -1) {
        printf("Eroare: Nu pot deschide fisierul pentru stergere.\n");
        return;
    }

    report r;
    int gasit = 0;

    // off_t este un tip special de variabila in C folosit pentru a retine locatii (offset-uri) din interiorul fisierelor
    off_t pozitie_stearsa = 0;

    // parcurg fisierul secvential
    while (read(fd, &r, sizeof(report)) > 0) {
        if (r.ID == id_de_sters) {
            gasit = 1;
            // am gasit id-ul. in acest moment, cursorul meu intern este fixat DUPA raportul pe care tocmai l-am citit.
            // folosesc lseek pentru a misca acest cursor.
            // ii zic sa se duca inapoi (cu valoare negativa) cu fix o structura de raport, pornind de la locatia curenta (SEEK_CUR).
            // asa ajung sa salvez in variabila 'pozitie_stearsa' adresa exacta unde INCEPE raportul meu.
            pozitie_stearsa = lseek(fd, -sizeof(report), SEEK_CUR);
            break;
        }
    }

    if (gasit == 0) {
        printf("Nu am gasit niciun raport cu ID-ul %d\n", id_de_sters);
        close(fd);
        return;
    }

    report raport_urmator;

    // repozitionez cursorul. plec de la inceputul fisierului (SEEK_SET) si ma duc pana la locatia raportului gasit,
    // apoi mai sar inca un raport ca sa ajung fix la datele pe care vreau sa incep sa le mut spre stanga.
    lseek(fd, pozitie_stearsa + sizeof(report), SEEK_SET);

    // incep sa citesc tot ce a mai ramas in fisier dupa id-ul sters
    while (read(fd, &raport_urmator, sizeof(report)) > 0) {

        // mut cursorul inapoi la pozitia pe care vreau sa o suprascriu (sa o sterg)
        lseek(fd, pozitie_stearsa, SEEK_SET);

        // scriu datele de la raportul din dreapta peste locatia din stanga
        write(fd, &raport_urmator, sizeof(report));

        // actualizez pozitia, adunand o dimensiune de raport, ca sa stiu unde suprascriu pe tura urmatoare
        pozitie_stearsa += sizeof(report);

        // sar peste zona in care tocmai am scris, altfel bucla while o sa citeasca acelasi lucru la nesfarsit
        lseek(fd, pozitie_stearsa + sizeof(report), SEEK_SET);
    }

    // dupa ce am copiat totul cu un pas spre stanga, la sfarsitul fisierului a ramas raportul original netaiat (o dublura).
    // ftruncate ia fisierul si il taie scurt fix la locatia la care s-a oprit algoritmul meu de mai sus, eliminand restul.
    ftruncate(fd, pozitie_stearsa);

    close(fd);
    printf("Am sters raportul cu ID-ul %d!\n", id_de_sters);
}
// functia creeaza o scurtatura (symlink) directa catre fisierul cu rapoarte al unui district
void creeaza_scurtatura_district(char* district) {
    char cale_originala[150];
    char nume_scurtatura[150];

    // generez calea catre fisierul binar real (ex: "centru/reports.dat")
    sprintf(cale_originala, "%s/reports.dat", district);

    // generez cum vreau sa apara scurtatura mea in folderul curent (ex: "rapoarte_centru.dat")
    sprintf(nume_scurtatura, "rapoarte_%s.dat", district);

    // functia symlink() din sistemul de operare face legatura simbolica.
    // primeste ca argumente unde duce scurtatura si cum se numeste ea.
    // returneaza 0 daca s-a creat cu succes pe disk.
    if (symlink(cale_originala, nume_scurtatura) == 0) {
        printf("Am creat scurtatura '%s' pentru districtul '%s'!\n", nume_scurtatura, district);
    } else {
        printf("Eroare: Nu am putut crea scurtatura. Poate exista deja?\n");
    }
}

// functia prin care managerul schimba pragul de severitate in fisierul de configurare
void actualizeaza_prag(char* district, int valoare_noua, char* rol) {
    // verific restrictia de rol impusa de cerinta
    if (strcmp(rol, "manager") != 0) {
        printf("Eroare: Doar managerul poate actualiza pragul!\n");
        return;
    }

    char filepath[150];
    sprintf(filepath, "%s/district.cfg", district);

    struct stat st;
    // folosesc stat() pentru a verifica permisiunile curente ale fisierului inainte sa scriu in el.
    // cerinta imi cere sa refuz modificarea daca permisiunile nu sunt exact 640.
    if (stat(filepath, &st) == 0) {
        // st.st_mode contine mai multi biti (inclusiv tipul fisierului).
        // aplicand masca & 0777, pastrez strict ultimii 9 biti care reprezinta efectiv permisiunile (rwx).
        // compar rezultatul cu 0640 (0 in fata inseamna ca e un numar octal - in baza 8, asa cum cere Linux)
        if ((st.st_mode & 0777) != 0640) {
            printf("Eroare securitate: Permisiunile fisierului nu sunt 640! Refuz modificarea.\n");
            return;
        }
    } else {
        printf("Eroare: Nu gasesc fisierul district.cfg\n");
        return;
    }

    // deschid fisierul in mod scriere (O_WRONLY).
    // folosesc flag-ul O_TRUNC pentru a sterge absolut tot ce era inainte in fisier (il trunchiez la 0 byti),
    // deoarece vreau sa ramana in el doar noul text cu pragul proaspat actualizat.
    int fd = open(filepath, O_WRONLY | O_TRUNC);
    if (fd != -1) {
        char text_de_scris[50];
        // construiesc textul exact asa cum il cere formatul: "severity_threshold=VALOARE"
        sprintf(text_de_scris, "severity_threshold=%d\n", valoare_noua);

        // scriu string-ul generat direct in fisier
        write(fd, text_de_scris, strlen(text_de_scris));

        close(fd);
        printf("Pragul a fost actualizat la %d in %s!\n", valoare_noua, filepath);
    } else {
        printf("Eroare la deschiderea fisierului de configurare.\n");
    }
}
// functia care functioneaza ca o camera de supraveghere si noteaza tot in fisierul de log
void scrie_in_jurnal(char* district, char* rol, char* utilizator, char* actiune) {
    char filepath[150];
    sprintf(filepath, "%s/logged_district", district);

    // deschid fisierul in mod adaugare la final (O_APPEND).
    // nu vreau sa sterg istoricul vechi (O_TRUNC), vreau sa adun liniile una sub alta.
    int fd = open(filepath, O_WRONLY | O_APPEND);
    if (fd == -1) {
        return; // daca nu am gasit fisierul, pur si simplu ies (nu dau eroare ca sa nu blochez programul principal)
    }

    // preiau timpul curent de la sistem
    time_t acum = time(NULL);
    char* timp_text = ctime(&acum); // ctime transforma secundele in text citibil (ex: Wed Apr 22 10:15:00 2026)

    // mica smecherie: ctime pune un "\n" (enter) automat la final. il sterg ca sa imi iasa o linie frumoasa in fisier.
    timp_text[strcspn(timp_text, "\n")] = 0;

    char linie_jurnal[300];
    // construiesc exact propozitia pe care o voi nota in fisier
    sprintf(linie_jurnal, "[%s] Rol: %s | User: %s | Actiune: %s\n", timp_text, rol, utilizator, actiune);

    // scriu propozitia generata in fisier
    write(fd, linie_jurnal, strlen(linie_jurnal));

    close(fd); // inchid fisierul
}

int main(int argc, char *argv[]) {
    // variabile in care salvez argumentele citite din comanda
    char role[20] = "";
    char user[31] = "";
    char district[50] = "";

    // iterez prin toate argumentele din terminal
    for (int i = 1; i < argc; i++) {

        // detectez rolul si il salvez (ex: manager)
        if (strcmp(argv[i], "--role") == 0 && i + 1 < argc) {
            strcpy(role, argv[i+1]);
            i++;
        }

        // detectez numele utilizatorului si il salvez
        else if (strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            strcpy(user, argv[i+1]);
            i++;
        }

        // procesez comanda de adaugare raport
        else if (strcmp(argv[i], "--add") == 0 && i + 1 < argc) {
            strcpy(district, argv[i+1]); // salvez cartierul

            setup_district(district); // creez fisierele initiale daca nu exista
            adauga_raport(district, user); // lansez functia care cere datele de la tastatura

            // chem "camera de supraveghere" ca sa noteze ca am dat add
            scrie_in_jurnal(district, role, user, "--add");
            i++; // sar peste numele cartierului din argumente
        }

        // procesez comanda de listare
        else if (strcmp(argv[i], "--list") == 0 && i + 1 < argc) {
            strcpy(district, argv[i+1]); // salvez cartierul

            listeaza_rapoarte(district); // afisez ce este in reports.dat

            // notez in jurnal ca s-a facut o listare
            scrie_in_jurnal(district, role, user, "--list");
            i++;
        }

        // procesez comanda de stergere a unui raport
        else if (strcmp(argv[i], "--remove_report") == 0 && i + 2 < argc) {
            strcpy(district, argv[i+1]); // salvez cartierul

            int id_tinta = atoi(argv[i+2]); // transform ID-ul dat in terminal din string in int
            sterge_raport(district, id_tinta, role); // dau mai departe id-ul si rolul curent

            // notez in jurnal stergerea
            scrie_in_jurnal(district, role, user, "--remove_report");
            i += 2; // sar peste cele 2 argumente procesate (cartier si id)
        }

        // procesez comanda de actualizare a fisierului de configurare
        else if (strcmp(argv[i], "--update_threshold") == 0 && i + 2 < argc) {
            strcpy(district, argv[i+1]); // salvez cartierul

            int prag_nou = atoi(argv[i+2]); // transform valoarea din string in int
            actualizeaza_prag(district, prag_nou, role); // lansez functia

            // chem "camera de supraveghere" ca sa noteze ca am dat update
            scrie_in_jurnal(district, role, user, "--update_threshold");
            i += 2; // sar peste cele 2 argumente procesate
        }

        // procesez comanda de creare a scurtaturilor (symlink)
        else if (strcmp(argv[i], "--manager_view") == 0) {
            // verific daca utilizatorul are rolul necesar
            if (strcmp(role, "manager") != 0) {
                printf("Eroare: Doar managerul poate genera o vedere de ansamblu!\n");
            } else {
                // comanda asta se da la final, dupa ce am citit deja un cartier (ex: --list centru --manager_view)
                // strlen(district) > 0 verifica practic daca variabila mea a salvat un nume de district pe iteratiile trecute
                if (strlen(district) > 0) {
                    creeaza_scurtatura_district(district);

                    // notez in jurnal crearea scurtaturilor
                    scrie_in_jurnal(district, role, user, "--manager_view");
                } else {
                    printf("Eroare: Trebuie sa specifici un district inainte de comanda asta.\n");
                }
            }
            // aici nu am i++ pentru ca --manager_view e doar un cuvant simplu (flag), nu e urmat de nicio valoare
        }
    }

    return 0;
}
