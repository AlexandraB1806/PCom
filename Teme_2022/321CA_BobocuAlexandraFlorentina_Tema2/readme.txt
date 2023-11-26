Bobocu Alexandra-Florentina, 321CA

# FISIERE ARHIVA
- subscriber.cpp, care contine functionalitatea subscriber-ului TCP;
- server.cpp, care contine functionalitatea serverului;
- helpers.h, pentru macroul DIE, define-uri si structuri aditionale;
- Makefile;
- fisierul readme.txt.


# STRUCTURI ADITIONALE FOLOSITE

In fisierul server.cpp, am creat o structura pentru un client TCP, numita
tcp_client, in care am considerat, ca si campuri relevante:
- ID-ul clientului;
- adresa IP;
- portul;
- socketul;
- statusul: 1 (client conectat) / 0 (client neconectat);
- un map de la topic pana la SF, astfel ca atunci cand voi cauta un anumit
topic, voi avea la dispozitie si SF-ul pentru acel topic.

In helpers.h, am definit si alte structuri utile:
* 2 structuri refera la mesajele de tip TCP / UDP. Pachetele de tip UDP
respecta forma descrisa in enuntul temei, iar cele de TCP au in plus
adresa IP si portul.
* In functie de tipul de date (INT, SHORT_REAL, FLOAT, STRING), am creat,
pentru primele 3 tipuri, cate o structura specifica, astfel:
- Pentru tipul INT: structura int_message;
- Pentru tipul SHORT_REAL: structura short_real_message;
- Pentru tipul FLOAT: structura float_message.
Fiecare din aceste structuri are specific bitul de semn (INT / FLOAT),
un numar fie pe 32 de biti (INT / FLOAT), fie pe 16 biti (SHORT_REAL), si in
plus fata de celelealte structuri, FLOAT mai are un camp pentru puterea unui
numar.


# FLOW-UL PROGRAMULUI:

~ subscriber.cpp ~

- Creez socketul clientului TCP, completez corespunzator campurile structurii
specifice adresei serverului, dupa care voi realiza conexiunea clientului
catre server;
- Opresc algoritmul Nagle;
- Se trimite ID-ul clientului pe socket-ul TCP creat la conectare;
- Se actualizeaza multimea de file descriptori.

In bucla while(1):
- Realizez multiplexarea intre citirea de la tastatura si citirea de pe
socket-ul de TCP;
- Tratez urmatoarele cazuri ale subscriber-ului care asteapta comenzi:
  -> CAZ I: Daca comenzile sunt primite de la tastatura, le citesc cu ajutorul
  buffer-ului setat in prealabil pe 0. Intampin 3 situatii:
     * Daca primesc "exit": dau break;
     * Daca primesc "subscribe": trimit prin socket-ul de TCP mesaj serverului
     ca respectivul client s-a abonat la un anumit topic si afisez in client
     mesajul corespunzator;
     * Daca primesc "unsubscribe": procedez similar ca in cazul anterior, doar
     ca de data aceasta, clientul se dezaboneaza de la un anumit topic.
  -> CAZ II: Daca comenzile sunt primite de pe socket-ul de TCP:
     * Creez un pachet de tip tcp_client_message care primeste mesajul de pe
     socket-ul TCP;
     * Daca functia recv() mi-a returnat 0, inseamna ca s-a inchis conexiunea
     deci voi da break;
     * Am salvat in topicul mesajului creat stringul "EXIT" (pentru protocolul
     pe socket-ul de TCP);
     * Prelucrez pachetele primite, cu ajutorul functiei process_packets_socket().

- Inchid socket-ul de TCP.

~ server.cpp ~

- Ca si containere, voi folosi pentru usurinta si eficienta map-uri.
Construiesc 4 map-uri:
-> connected_clients: Retine clientii nou-conectati la server. Socket-ul
clientului, reprezentat de un intreg, conduce la clientul propriu-zis de tip
tcp_client.
-> topics: Numele unui topic are asociat un vector de clienti abonati la acel topic.
-> id_clients: O baza de date, unde cheia o reprezinta ID-ul, sub forma de string,
iar valoarea este clientul de tip tcp_client.
-> socket_clients: ID-ul unui client conduce catre file descriptorul sau.

In primele 3 map-uri trimit pointer catre un client de tip tcp_client, astfel
ca o modificare vizibila intr-un map va fi vizibila si din celelealte map-uri.

- Creez 2 socketi: pe cel al clientului TCP si pe cel al clientului UDP.
Completez corespunzator campurile structurii specifice adresei serverului;
- Setez portul;
- Opresc algoritmul Nagle;
- De pe socketul de TCP voi face atat bind(), cat si listen(), pentru
eventualele acceptari ale cererilor de conexiune venite din partea clientilor.
In schimb, de pe socket-ul de UDP, voi face doar bind();
- Se actualizeaza multimea de file descriptori si se alege cel maxim.

In bucla while(1):
* Intampin 4 situatii:
  1) Daca primesc de la tastatura comanda "exit", trebuie realizata inchiderea
  simultana a serverului si deconectarea tuturor clientilor TCP conectati
  pana in acel moment, deci voi inchide cele 2 socket-uri de TCP si UDP. Inainte
  de asta insa, voi itera prin toti clientii conectati in acel moment, anuntandu-i
  de inchiderea serverului, printr-un mesaj: "EXIT" (protocolul de nivel
  aplicatie). Voi trimite acest mesaj direct pe socketul clientului.

  2) Daca comanda vine de pe socketul de TCP, inseamna ca un nou client TCP
  doreste sa se conecteze:
  - Serverul accepta cererea venita din partea clientului;
  - Inainte de a adauga noul client in map-ul connected_clients, voi analiza
  ca ID-ul sau sa nu coincida cu ID-ul altui client deja conectat, in acest sens
  folosindu-ma de map-ul id_clients, in care caut dupa ID-ul primit de pe
  noul socket TCP si salvat in prealabil in buffer. Daca ID-ul extras din noul
  socket de TCP se regasestese in map-ul id_clients, se va afisa un mesaj
  corespunzator. Creez un mesaj de tip tcp_client_message al carui topic va fi
  completat cu mesajul "EXIT" si il trimit clientului;
  - Dupa verificarea de mai sus, actualizez multimea de file descriptori,
  incluzand noul socket si recalculez socketul maxim;
  - Salvez in prealabil ID-ul clientului in stringul client_id. Voi verifica
  in functie de ID in map-ul id_clients daca exista deja clientul creat.
    -> In caz afirmativ, trebuie doar sa completez map-ul connected_clients
    cu noul client creat, sa il marchez ca fiind conectat atat din
    connected_clients, cat si din id_clients (ambele map-uri trimit la un
    client TCP) si includ socketul noului client in map-ul socket_clients.
    -> In caz contrar, voi crea un nou client TCP si completez campurile
    structurii. Actualizez map-urile cu acest clientul tocmai creat
    (connected_clients si id_clients) si cu socketul (socket_clients).
  - Afisez mesajul corespunzator conform enuntului temei.

  3) Daca primesc prin intermediul protocolului UDP topicul, tipul si continutul
  mesajului, verific daca am clienti catre care trebuie sa trimit pachetul
  (clientii conectati).
  - Creez un mesaj de tip UDP pe care il completez cu informatiile venite de pe
  socketul corespunzator;
  - Creez un mesaj de tip TCP si ii populez campurile structurii specifice.
  - Extrag din mesajul TCP topicul si il salvez intr-o variabila auxiliara de
  tip string. Voi verifica daca topicul extras se gaseste in map-ul topics. In
  caz afirmativ, voi itera prin toti clientii abonati la topicul respectiv si
  daca dau peste un client care este conectat, ii voi salva socketul extras din
  map-ul socket_clients si trimit pe acest socket mesajul TCP.

  4) Daca un client TCP este deja conectat la server si primesc pachete direct
  de pe socketul sau:
  - Analizez pachetul primit. Daca pachetul este gol, adica functia recv()
  mi-a intors 0, asta implica inchiderea conexiunii.
  - Tratez cele 2 cazuri posibile: subscribe si unsubscribe:
    -> subscribe: Voi sparge mesajul primit din buffer in urma apelului
    functiei recv() in 3 bucati: comanda primita, topicul si SF-ul,
    delimitate prin spatii cu strtok si pe care le retin separat. Salvez numele
    topicului intr-un string name_topic.
    Voi cauta in map-ul topics dupa cheia name_topic.
    * Daca gasesc cheia: Iau o variabila separata ok care imi indica daca am
    gasit clientul in vectorul de clienti din map-ul topics sau nu. Parcurg
    toti clientii abonati la topic, iar in functie de ID, identific daca
    clientul conectat este totodata cel care este abonat la topic.
      ** Daca este: completez SF-ul cu cel primit din buffer.
      ** Daca nu am gasit clientul in vectorul de clienti din topics, trebuie
      creat un nou client, care ulterior va fi adaugat in map-ul topics.
    * Daca nu gasesc cheia: Voi crea un nou vector de clienti pentru map-ul
    topics, iar fiecarui client nou creat ii voi completa corespunzator
    campurile.
    -> unsubscribe: Procedez ca la subscribe, doar ca sparg mesajul in 2
    bucati: comanda primita si topicul, delimitate prin spatii cu strtok si pe
    care le retin separat. Salvez numele topicului intr-un string name_topic,
    pe care il voi folosi pentru a cauta in map-ul topics daca exista, si daca
    da, il voi sterge.
