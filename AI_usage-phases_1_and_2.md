# AI Usage Documentation – Phase 1

# Tool folosit
Claude (claude.ai)

# Prompt 1 — parse_condition

Ce am cerut:
Am descris structura report din C si am cerut o functie
parse_condition care primeste un string de forma
field:operator:value si il imparte in trei parti separate.

Ce a generat Claude:

int parse_condition(const char *input, char *field, char *op, char *value) {
    if (sscanf(input, "%31[^:]:%3[^:]:%31s", field, op, value) == 3) return 1;
    return 0;
}

Ce am modificat:
Nu am modificat functia. Am verificat ca limitele de buffer
corespund dimensiunilor din arrays-urile din cmd_filter si ca
formatul sscanf cu [^:] citeste corect pana la delimitatorul ':'.

Ce am invatat:
Cum functioneaza sscanf cu seturi de caractere negat ([^:])
pentru a citi un string pana la un caracter delimitator,
fara a-l include in rezultat.

---

# Prompt 2 — match_condition

Ce am cerut:
Am descris tipurile campurilor din structura report si am cerut
o functie match_condition care primeste un pointer la un raport
si o conditie parsata si returneaza 1 daca raportul o satisface.

Ce a generat Claude:

int match_condition(report *r, const char *field, const char *op, const char *value) {
    // functia completa generata, fara modificari
}

Ce am modificat:
Nu am modificat functia. Am verificat manual fiecare ramura:
- atoi() converteste corect string-ul la int pentru severity
- atol() e folosit pentru timestamp deoarece time_t poate
  depasi limitele unui int pe sisteme pe 64 de biti
- return 1 la final nu exclude rapoarte la camp necunoscut

Ce am invatat:
Ca trebuie sa convertim explicit valoarea din string in tipul
corect din C inainte de comparatie. Am inteles de ce comparatia
directa intre un string si un int nu functioneaza in C.

---

# Evaluare critica

Ambele functii au fost corecte din prima. Contributia mea principala
a fost verificarea si intelegerea lor linie cu linie, nu modificarea.
Logica de filtrare cu AND implicit din cmd_filter am scris-o eu
separat, conform cerintei din specificatie.

---

# AI Usage Documentation – Phase 2

# Tool folosit
Claude (claude.ai)

# Pentru ce am folosit AI-ul in Phase 2

In Phase 2 nu am generat cod cu ajutorul AI-ului. L-am folosit
doar pentru a intelege mai bine anumite concepte si functii de
sistem pe care le-am implementat apoi singur.

Concepte despre care am cerut explicatii:
- Cum functioneaza sigaction() si diferenta fata de signal()
- Ce face pause() si de ce este potrivit pentru asteptarea semnalelor
- Cum functioneaza kill() pentru trimiterea semnalelor intre procese
- De ce se foloseste write() in handlere in loc de printf()

# Ce am invatat

- Ca signal() este considerat nesigur si neportabil, iar sigaction() ofera control explicit asupra comportamentului la primirea unui semnal prin flag-uri precum SA_RESTART
- Ca printf() nu este async-signal-safe si poate provoca deadlockdaca este apelat dintr-un handler; solutia corecta este write() direct pe STDOUT_FILENO
- Ca pause() suspenda procesul fara a consuma CPU pana la primirea unui semnal, fiind solutia potrivita pentru o bucla de asteptare
- Ca kill() nu termina neaparat un proces, ci trimite orice semnal specificat catre procesul cu PID-ul dat