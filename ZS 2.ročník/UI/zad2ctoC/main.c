#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <math.h>


typedef struct bod  {
    int x;
    int y;
    unsigned char assigned;
    struct bod *next;
} BOD;

typedef struct hashtable {
    int capacity;
    int n_of_children;
    BOD **children;
} HASHTABLE;

typedef struct seach_res {
    int found;
    BOD* tail;
} SEARCH_RES;

typedef struct bod_simple {
    short x;
    short y;
} BOD_SIMPLE;

typedef struct cluster {
    BOD_SIMPLE *children;
    int n_of_children;
} CLUSTER;

typedef struct cluster_list {
    int n_clusters;
    CLUSTER *clusters;
} CLUSTER_LIST;


typedef struct heap_child {
    int i;
    int j;
    uint16_t dist;
} HEAP_CHILD;


typedef struct heap {
    int heapsize;
    int capacity;
    HEAP_CHILD *children;
} HEAP;



//operácie LL
SEARCH_RES *ll_search(BOD* head, BOD *searched, SEARCH_RES *res) {
    res->found = 0;
    BOD* act = head;
    while (act->next != NULL) {
        if (act->x == searched->x && act->y == searched->y) {
            res->found = 1;
            break;
        }
        act = act->next;
    }
    if (act == searched) {
        res->found = 1;
    }
    res->tail = act;
    return res;
}

BOD *create_bod(int x, int y) {
    BOD *new_b = malloc(sizeof(BOD));
    new_b->x = x;
    new_b->y = y;
    new_b->assigned = 0; //0 znamená alokovaný, ale nepoužitý, náhrada za NULL
    new_b->next = NULL;
    return new_b;
}

HASHTABLE *create_table(int n) {
    HASHTABLE* new_table = malloc(sizeof(HASHTABLE));
    new_table->capacity = n;
    new_table->n_of_children = 0;
    new_table->children = malloc(sizeof(BOD*)*n);
    return new_table;
}

HASHTABLE *init_table(HASHTABLE *table) {
    for (int i=0;i<table->capacity;i++) {
        table->children[i] = create_bod(0,0);
    }
    return table;
}

HASHTABLE *clear_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    for (int i=0;i<table->capacity;i++) {
        if (table->children[i]->assigned == 1) {
            if (table->children[i]->next != NULL) {
                //dealokovanie LL okrem head

                BOD *current = table->children[i]->next; //začneme od druhého prvku
                BOD *next_node;

                while (current != NULL) {
                    next_node = current->next;
                    free(current);
                    current = next_node;

                }
                table->children[i]->next = NULL;

            }
            table->children[i]->assigned = 0;
        }
    }
    return table;
}


void free_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    for (int i=0;i<table->capacity;i++) {
        //dealokovanie LL okrem head
        BOD *current = table->children[i]; //začneme od druhého prvku
        BOD *next_node;

        if (current->next != NULL) {
            BOD *act = current;
            //while (act->next != NULL) {
                next_node = act->next;
                free(act);
                act = next_node;
            //}
        }
        else {
            free(table->children[i]);
        }
    }
    free(table->children);
    free(table);
}

HASHTABLE *add_hash(HASHTABLE *table, BOD *dummy, SEARCH_RES *res, BOD *extracted) {
    int x = dummy->x;
    int y = dummy->y;
    int success = 0;
    BOD *new_b = NULL;
    uint64_t hash = (x * 73856093) ^ (y * 19349663);
    size_t index = hash % table->capacity;

    //printf("adding numbers\n");
    if (table->children[index]->assigned == 1) { //kolízia
        res = ll_search(table->children[index],dummy, res);
        if (!res->found) {
            new_b = create_bod(x,y);
            new_b->assigned = 1;
            res->tail->next = new_b;
            table->n_of_children += 1;
            success = 1;
        }
        //deleted DUPLICATED print

    }
    else {
        //printf("assigned\n");
        new_b = table->children[index];
        new_b->x = x;
        new_b->y = y;
        new_b->assigned = 1;

        table->n_of_children += 1;
        success = 1;
    }

    if (success && extracted != NULL) {
        extracted[table->n_of_children] = *new_b;
    }

    return table;
}


BOD *extract_items_from_hs(HASHTABLE *table, BOD *extracted, int last_n) {
    int index = 0;
    for (int i=0;i<table->capacity;i++) {
        BOD *selected = table->children[i];
        if (selected->assigned == 1) { //ináč nás to nezaujíma
            while (selected != NULL) {
                extracted[last_n+index] = *selected;
                index++;
                selected = selected->next;
            }
        }
    }
    //printf("index %d\n",index);
    return extracted;
}


//HEAP operácie
HEAP *create_heap(int n) {
    HEAP *heap = malloc(sizeof(HEAP));
    heap->children = malloc(sizeof(HEAP_CHILD)*n);
    heap->heapsize = 0;
    heap->capacity = n;
    return heap;
}





HEAP *heapify_up(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne

    int p_idx = floor(idx/2);
    //printf("p %d",p_idx);
    if (p_idx < heap->heapsize && heap->children[p_idx].dist > heap->children[idx].dist) { //ak je platný a
        //swap mna a parenta, swap ala C

        HEAP_CHILD tmp = heap->children[p_idx];
        heap->children[p_idx] = heap->children[idx];
        heap->children[idx] = tmp;

        heapify_up(heap,p_idx);
    }

    return heap;
}


HEAP *heapify_down(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne

    int left = 2*idx+1;
    int right = left+1; //2i+1, optimalizácia xd

    //hladame smallest idx
    int smallest = idx;

    if (left < heap->heapsize && heap->children[left].dist < heap->children[idx].dist) {
        smallest = left;
    }

    if (right < heap->heapsize && heap->children[right].dist < heap->children[idx].dist) {
        smallest = right;
    }

    //heap down pre smallest idx
    if (smallest != idx) {
        //swap mna a childa
        HEAP_CHILD tmp = heap->children[smallest];
        heap->children[smallest] = heap->children[idx];
        heap->children[idx] = tmp;

        heapify_down(heap,smallest);
    }

    return heap;
}

HEAP *add_child(HEAP *heap, short i, short j, uint16_t dist) {
    heap->children[heap->heapsize].i = i;
    heap->children[heap->heapsize].j = j;
    heap->children[heap->heapsize].dist = dist;

    heapify_up(heap,heap->heapsize);
    heap->heapsize += 1;
    return heap;
}


HEAP_CHILD remove_min(HEAP *heap) {
    HEAP_CHILD min = heap->children[0];
    heap->children[0] = heap->children[heap->heapsize];
    heap->heapsize -= 1;
    heapify_down(heap,0);

    return min;
}





short int generate_range(int upper, int lower) {
    short int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

static inline uint16_t calculate_distance(BOD *bod1, BOD *bod2) {
    //dáme rovno štvorcovú vzdialenosť
    const int diff1 = bod2->x-bod1->x;
    const int diff2 = bod2->y-bod1->y;


    const int distance = diff1*diff1 + diff2*diff2;
    return (distance > 2500) ? 0 : distance;
}

float calculate_distance_simple(BOD_SIMPLE bod1, BOD_SIMPLE bod2) {
    float distance = pow(bod2.x-bod1.x,2) + pow(bod2.y-bod1.y,2);
    return distance;
}


HEAP *create_matica_vzd(int capacity, BOD *extracted) {
    //napadla ma optimalizácia: ak vypočítam, že vzdialenosť je viac ako 500, tak to ani nedám do matice, ďalšie ušetrené miesto
    uint64_t n = capacity;
    uint64_t size = (n*n-n)/2; //počet prvkov dolneho trojuholníku mínus diagonála


    //postupné zvyšovanie veľkosti lebo neviem koľko toho je, tak dajme na začiatok 50 000
    int initial_size = capacity*40;
    HEAP *heap = create_heap(initial_size);


    //rovno spravím dolny trojuholnik, bez diagonaly
    for (int i=0;i<n;i++) {
        BOD *i1 = &extracted[i];
        for (int j=0;j<i;j++) {
            uint16_t distance = calculate_distance(i1,&extracted[j]);

            if (distance != 0) {
                if (heap->heapsize == initial_size) {
                    initial_size = initial_size << 1;

                    heap->children = realloc(heap->children, initial_size*sizeof(HEAP_CHILD));
                    if (!heap->children) {
                        printf("rwalocacia zlyhala");
                    }
                }
                add_child(heap,i,j,distance);
            }
        }
    }


    return heap;
}

CLUSTER create_cluster(BOD *bod) {
    CLUSTER new;
    new.children = malloc(sizeof(BOD_SIMPLE)*10);

    BOD_SIMPLE new_b;
    new_b.x = (short)bod->x;
    new_b.y = (short)bod->y;

    new.children[0] = new_b;
    new.n_of_children = 1;
    return new;
}

CLUSTER_LIST* init_cluster_list(BOD *extracted, int capacity) {

    CLUSTER_LIST *c_list = malloc(sizeof(CLUSTER_LIST));
    c_list->n_clusters = capacity;

    c_list->clusters = malloc(sizeof(CLUSTER)*capacity);

    for (int i=0;i<capacity;i++) {
        c_list->clusters[i] = create_cluster(&extracted[i]);
    }

    return c_list;
}






HASHTABLE* prvotne_body(int n,HASHTABLE *table) {
    //nesmu byť rovnaké tie body

    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));
    while (table->n_of_children != n) {
        short int x = generate_range(5000,-5000);
        short int y = generate_range(5000,-5000);

        dummy->x = x;
        dummy->y = y;

        //cek ci je v zozname
        table = add_hash(table, dummy, res, NULL);
    }
    return table;
}

HASHTABLE* druhotne_body(int n, HASHTABLE *table, BOD *extracted) {
    int target = table->n_of_children + n;
    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));

    int x_offset_1, x_offset_2;
    int y_offset_1, y_offset_2;


    while (table->n_of_children != target) {
        //nahodne číslo 0 až n_of_children
        short int x = generate_range(table->n_of_children-1,0);
        BOD selected = extracted[x]; //ušetrené 0.007s? 0.018s->0.011s bez alokácie

        register int bod_x = selected.x;
        register int bod_y = selected.y;


        //generujeme offset
        x_offset_1 = (bod_x - 100 < -5000) ? -5000 - bod_x : -100;
        x_offset_2 = (bod_x + 100 > 5000) ? 5000 - bod_x : 100;
        y_offset_1 = (bod_y - 100 < -5000) ? -5000 - bod_y : -100;
        y_offset_2 = (bod_y + 100 > 5000) ? 5000 - bod_y : 100;

        short x_offset = generate_range(x_offset_2,x_offset_1);
        short y_offset = generate_range(y_offset_2,y_offset_1);
        register int new_x = bod_x + x_offset, new_y = bod_y + y_offset;

        dummy->x = new_x;
        dummy->y = new_y;


        table = add_hash(table, dummy,res,extracted); //toto zase pridá veľa sekúnd a preváži zisk vyššie

    }
    return table;
}

CLUSTER *aglomeratne_clusterovanie(CLUSTER *clusters, HEAP *heap) {

    while (1) {
        HEAP_CHILD min = remove_min(heap);
        while (clusters[min.i].n_of_children == 0 || clusters->n_of_children) {
            min = remove_min(heap);
        }

        //merge clusters
        if (clusters[min.j].n_of_children == 0) { //nemám čo spájať
            break;
        }




    }






    return clusters;
}











void print_arr(HASHTABLE *table) {
    for (int i=0;i<table->n_of_children;i++) {
        if (table->children[i]->assigned == 1) {

            if (table->children[i]->next == NULL) {
                printf("BOD %d: (%d,%d)\n",i, table->children[i]->x,table->children[i]->y);
            }
            else {
                printf("Printing LL:\n");
                BOD *act = table->children[i];
                while (act->next != NULL) {
                    printf(": %d,%d\n",act->x,act->y);
                    act = act->next;
                }
                printf(": %d,%d\n",act->x,act->y);
                printf("End of LL\n");

            }


        }

    }
}




int main() {
    printf("Hello, World!\n");

    struct timeval start, end;

    int prvotne = 20;
    int druhotne = 20000;


    int capacity = prvotne + druhotne;
    HASHTABLE *table = create_table(capacity);
    table = init_table(table);

    BOD *extracted = malloc(sizeof(BOD)*capacity);


    table = prvotne_body(prvotne,table); //prvotne body
    extracted = extract_items_from_hs(table,extracted,0); //extrakcia po prvom generovaní
    table = clear_table(table); //recyklácia tabuľky

    table = druhotne_body(druhotne,table, extracted); //druhotne generovanie
    extracted = extract_items_from_hs(table,extracted,prvotne); //extrakcia, finalne body

    free_table(table); //dealokuj tabuľku

    HEAP *heap = create_matica_vzd(capacity,extracted); //vytvor maticu vzdialeností




    CLUSTER_LIST *clusters = init_cluster_list(extracted,capacity);


    gettimeofday(&start, NULL); // Začiatok merania




    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time Elapsed: %fs",elapsed);





}

//GENERATE_BODY_PRVOTNE
//before optim: moj C kod
/*
 * 50000: 1.14s
 * 100000: 4.66s-5.2s
 */


//after hashtable + ll
/*
 * Python: set (hashtable)
 * 50000: 0.04s
 * 100000: 0.107s
 *
 * C: hash funkcia: abs((int)((new_b->x * new_b->y) % table->capacity)) 45% kolízii
 * 50000: 0.0058s
 * 100000: 0.01526s
 *
 * C: hash funkcia: (new_b->x * 73856093) ^ (new_b->y * 19349663) % capacity 37% kolízií
 * 50000: 0.0046s
 * 100000: 0.0105s
 *
 *
 * C: hash funkcia: FNV hash
 * 50000: 0.005s
 * 100000: 0.01s
 *
 * pre max rýchlosť by to chcelo Perfect Hashing, ale nechcem prísť o pamäť :D, necháme to tak teraz
 * necháme si tú druhú funkciu, je to viac menej jedno tisícinky sekundy
 *
 */

//GENERATE_BODY_DRUHOTNE
/* pokracujeme dalej v rovnakom kode
 * dalo by sa v povodnom kode namiesto konverzie na list pri každom generovaní spraviť nejaký
 *
 * Zaklad + Dodatocne:
 *
 * Python:
 * 20+20000: 1.84s
 * 1000+20000: 2.08s
 * 10000+20000: 4.3s
 * 20000+20000: 7s-7.4s
 *
 * C: generuje sa náhodný index pokial netrafím číslo v zozname, čiže je to efektívnejšie ak je v nom viac čísel, ináč veľa generujem
 * 20+20000: 0.0035s
 * 1000+20000: 0.003s
 * 10000+20000: 0.0025s
 * 20000+20000: 0.0026s
 *
 * 20000+100000: 0.016-0.02s
 * 50000+100000: 0.025s
 * 100000+100000: 0.032s
 *
 * BREAKDOWN: 0.035s
 * generovanie random čísel (3): 0.006s
 * porovnávanie hraníc: 0.0002s
 * prve generovanie + offsety: 0.003s
 * všetko okrem create bod a hash: 0.007s
 * všetko + vytváranie bodu a bez hashu: 0.011s, trvá kým sa to alokuje
 * pristupovanie do tabuľky childov som vypol, ostatné zostalo: 0.006s-0.007s
 * bez prístupov do pamäte a hashu: 0.003s
 *
 *
 * UPGRADE: predalokovanie hashtabulky, zatial sa nerieši LL, ten je rovnaký, ale ušetril som kopu času na alokáciach
 * Total: (druhotné generovanie): 0.018s skoro 50% ušetrené
 *
 * Benchmark:
 * 20000+20000: 0.0011s
 * 50000+50000: 0.005s cca
 * 50000+100000: 0.012s
 * 100000+100000: 0.018s /maybe false lebo som tam mal riadok, kde sa pripočítal n_of_children, čiže tam bola polovica operácii len
 *
 *
 * ZMENA: odstránil som indexy na komplet, spravil som extrahovanie hodnot aj z kolízii ale čas podobný, ideme sa na to pozrieť
 * všetko okrem hashu: 0.01s cca
 * čiže hash je okolo: 0.025s cca čo je dosť
 *
 *
 * CISTENIE A RECYKLACIA HASHTABLE: vyčistí sa hashtable a vie sa do nej vkladať druhotne generovanie, problem je, že sa beru nahodne body len z prvej generácie a nepridávajú sa nové
 * body z druhotnej generácie, ale to sa dá pohode fixnut asi, zatial mám vzácny error raz za 1000 zapnutí a neviem kde
 *
 * zatial: 100000+100000: 0.025s cca, niekedy menej niekedy viac, ale zrazil som to trochu, tým že som recykloval hashtable a mal som menej kolízií
 */


//CALCULATE DISTANCE
/*
 * Zmeny: používam štvorcovú vzdialenosť
 * používame int16 na vzdialenosti, ak je to > 2500, tak pošle 0
 * odstránil som pow a sqrt, trvajú príliš dlho a tým, že chcem len **2, tak je to nepotrebné, to zrýchlilo veľmi moc
 *
 * Teraz: 16000+16000: 2.9s
 *
 * Zmena parametrov na pointery, aby sa nerobili kópie vo funkcii, zbytočný čas
 *
 * Pointery+používam len int:
 * 16000+16000: 2.08s-2.2s
 *
 * pridanie "inline" slovíčka, kompilátor vloží kód priamo do funkcie, naštudovať, optimalizácia
 * použitie const, pomáha kompilátoru optimalizovať
 *
 * 16000+16000: 2.05s-2.1s minimálne zlepšenie, pre create_matica aj s týmto, nie len čas výpočtu
 *
 */




//CREATE_MATICA_VZD
/*
 *
 * alokovanie polovice matice bez diagonaly:
 * 10+10: 0.0015s
 * 10000+10000: 0.5s
 * 20000+20000: 2s cca
 *
 * pocitanie vzdialenosti + alokacia:
 * pokračovanie od calc_dist:
 * bolo:
 * 16000+16000: 2.05s-2.1s
 *
 * uloženie i-teho prvku a znova použitie v j-cykle: (geniálne)
 * 16000+16000: 1.9s-2s fajný improvement kvôli takému niečomu
 * 20+20000 (s týmto budem robiť): 0.8s
 *
 * Po výpise je tam strašne veľa núl, hlavne ak je >2500, čiže by bolo extrémne optimálne vybrať len tie dobré hodnoty
 */

//VLOZENIE DO CLUSTEROV A CLUSTEROVANI SYSTEM



//find min
/*
 * proste cyklenie cez maticu_vzd a porovnávanie s minimom
 * 20+20000: 0.2s sakra hodne daleko to bolo, veľa núl
 *
 * skipovanie núl:
 * 20+20000: 0.13s-0.14s
 *
 * kľúčom bude ostrániť tieto nuly a hľadať len to čo je relevantné, chcem použiť heap pre nenulové hodnoty, rozmýšľam nad typom
 * zložitosť: O(log n) čo je celkom dobre oproti O(n) čo je teraz viac menej
 *
 *
 * TODO - sprav heap vektorový a nejak vymysli vymazávanie pre určité klustery
 * teraz ma napadlo short pre index že ku ktorému clusteru patrí a potom by sa len podľa neho mazalo z heapu, akože aj tak cyklenie, ale it is wat it is
 * tam sa zmestí 65535 indexov pre clustery, hlavný problém tam je mazanie, to už rovno môžem skipnút maticu vzd a rovno to šupnúť do heap, to by bolo svižnejšie?
 */


//HEAP - min heap
/*
 * IDEA: vytváranie: bude sa postupne pridávať prvok z výpočtu matice_vzd, ktorý je nenulový
 * ADD: pridá sa na koniec array -> heapsize, heap-up
 * DELETE-REMOVE-MIN: root sa nahradí s posledným prvkom a heap-down
 * MOJ DELETE: cize chcem odstraniť nejaký prvok napr. zo stredu stromu: nahradim s posledným prvom, zistím okolie, či treba heap-up/down
 * Je z dvoch častí: search a delete, search som vymyselel tak,
 *
 * Problem searchu: heap nie je optimalizovaný na search, keby to bol AVL strom, tak by sme mali O(logn) aj na vyhľadávanie, ale nemáme, čiže riešenie
 * bude použiť Hashtable pointerov, ktorá bude mapovať flat indexy (polovičné ako sme sa bavili) a keď budem chcieť vyhľadať niečo, tak pozriem do HT a vymením to za
 * posledný prvok a potom podľa sitácie ako som písal vyššie
 *
 * Extra? áno, ale rýchle a neberie zas tak veľa pamäte
 * Kolízie nie su, čiže netreba ani LL, lebo flat index je unikátny, problém je miesto, lebo musíš mať technicky celú tabuľku alokovanú
 * riešenie nie %, ale potom sú kolízie zas, tak teda budú kolízie, bude menší strom, ale bude search ako doteraz v tej prvej HT
 *
 * Čiže search bude: O(1) pre index + O(b), čiže priemerný počet prvkov v buckete
 * Ako bude vyzerať prvok? netreba distance, netreba súradnice, treba len pointer na HEAP_CHILD a pointer na NEXT, čiže: 8B (pointer) + 8B? = 16B
 * čiže nech je tam 880 000 prvkov * 16B = 13.4 MB a toto bude konštantné stále to je pri veľkosti n keď sa zaplní každé políčko, + kolízie, čiže to je
 * výsledná veľkosť, celkom dosť, ale stále nič katastrofálne
 * do 1 GB sa zmestí 67 108 864 nenulových hodnôt a zvyšok nezaberá veľa miesta, výhoda je, že máme strašne veľa nenulových hodnôt
 *
 *
 *
 * Idea je taká, že nebudeme to hľadať, ale budeme vyberať kým to nie je ok, otázka je ako rýchlo čekovať či patrí medzi odstránené clustery alebo nie
 * dá sa to tak, že keď my máme ten list clusterov, pozriem sa na jeho index a ak tam je null, tak je to neplatné, to by mohlo byť fajn.
 * Lebo mať akože množinu pointerov a hľadať tam niečo je banger. Takto nemusíme ani hľadať, proste sa to vyondí a ono sa to samé vyhodí
 * typický delete, že sa nahradí s koncom a potom je heapify down ...
 *
 * hrubý odhad prvkov v heape - najhorší prípad
 * 20+20000: 880 000
 * 20+50000: 4 116 987
 * 20+100000: 16 741 000 prvkov
 * tip: x40? to máš na pár hopov do limitu, časté realokácie mi dávajú error a zas nechcem všetko prerátavať znova a znova
 */




