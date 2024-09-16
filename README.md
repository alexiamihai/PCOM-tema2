# README - TEMA 2 PCom

Acest proiect implementeaza un sistem de comunicare client-server. Serverul gestioneaza cereri si distribuie informatii catre clienti, in timp ce clientii pot interactiona cu serverul trimitand cereri si primind date.

Am dezvoltat acest cod respectand arhitectura descrisa in cerintele proiectului.

Serverul serveste ca punte de legatura intre clientii UDP si cei TCP.

Structura aplicatiei este divizata in trei componente principale:

- server.c (care include logica serverului si managementul clientilor UDP)
- subscriber.c (care se ocupa de gestionarea clientilor TCP)

### DETALII SERVER

Asa cum am explicat, serverul coordoneaza fluxul de informatii intre clientii UDP si TCP, functionand ca un broker de date. Serverul primeste pachete de la clientii UDP si le redirectioneaza catre clientii TCP abonati la topicurile respective.

**Procesarea datelor din pachetele UDP**

> Serverul preia informatiile esentiale din pachetele primite, cum ar fi tipul de date si continutul mesajului, convertindu-le in structuri compatibile pentru a fi relayate catre clientii TCP. Aceasta permite clientilor TCP sa primeasca date organizate si eficientizate de la clientii UDP.

**Managementul comenzi de abonare/dezabonare**

> Serverul gestioneaza si conexiunile TCP, procesand comenzi de abonare si dezabonare la diverse topicuri. Folosind aceste comenzi, serverul isi actualizeaza listele de topicuri ale fiecarui client si filtreaza mesajele care trebuie trimise clientilor TCP in functie de abonamentele lor.

**Reconectarea clientilor TCP**

> Serverul detecteaza si administreaza reconectarile clientilor TCP. Daca un client TCP incearca sa se reconecteze, serverul ii reinnoieste conexiunea si statusul pentru a asigura o comunicare fluida si continua.

### SUBSCRIBERII

Codul din subscriber.c permite clientilor sa se aboneze sau sa se dezaboneze si sa primeasca mesaje de la server.

Aplicatia necesita trei argumente la lansare: adresa IP a serverului, portul de conexiune si portul de ascultare.

> Se initializeaza un socket TCP si se stabileste conexiunea cu serverul folosind adresa IP si portul specificat. Apoi, adresa IP este trimisa serverului pentru initializarea structurii de mesaj care va fi transmisa.

> Se desfasoara o bucla principala in care se asteapta evenimente fie de la server, fie de la intrarea standard (STDIN). Informatiile primite de la server sunt afisate conform formatului definit in cerinta. Comenzile de la STDIN (abonare/dezabonare) sunt procesate si expediate catre server.

**Protocol eficient pentru structurarea mesajelor peste TCP**

> Protocolul conceput utilizeaza structuri de mesaje pentru a aranja si a transmite eficient datele intre client si server. Aceste structuri, definite in cod (in header.h), standardizeaza comunicatia si fac schimbul de informatii mai accesibil pentru ambele parti.

> Protocolul este optimizat pentru performanta si eficienta, reducand dimensiunea mesajelor si utilizarea excesiva a retelei pentru a imbunatati timpul de raspuns si experienta utilizatorului.

> Mai mult, protocolul include gestionarea erorilor, tratand corespunzator problemele de conectivitate sau alte intamplari neasteptate ce pot aparea in timpul comunicarii intre client si server.

**Formatul mesajelor:**

- Mesajele sunt serializate si impachetate in structuri de date specificate (command_tcp, tcp_struct, udp_struct).
- Mesajele sunt transmise prin conexiunea TCP folosind operatiuni de send() si recv().

**Tratarea erorilor:**

- Protocolul include verificari de erori la fiecare etapa critica, cum ar fi conectarea la socket, trimiterea si primirea de date.
- In caz de eroare, se afiseaza un mesaj adecvat si se termina executia programului.

**Note:**
- Am folosit un macro DIE definit in header.h, preluat din resursele OCW.
- Structurile sunt bazate pe scheletele din laboratoarele 7-9 si pe fluxul de comenzi din server.c si subscriber.c.
- Nu am implementat functiile pentru wildcard-uri.
- Toate testele de pe checker trec, in afara de cele pt wildcard-uri.