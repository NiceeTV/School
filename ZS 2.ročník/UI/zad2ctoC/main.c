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
    int *indexes;
    BOD **children;
} HASHTABLE;

typedef struct seach_res {
    int found;
    BOD* tail;
} SEARCH_RES;


//operácie LL
SEARCH_RES *ll_search(BOD* head, BOD *searched, SEARCH_RES *res) {
    res->found = 0;
    BOD* act = head;
    while (act->next != NULL) {
        if (act == searched) {
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
    new_table->indexes = malloc(sizeof(int)*n);
    new_table->children = malloc(sizeof(BOD*)*n);
    return new_table;
}

HASHTABLE *init_table(HASHTABLE *table) {
    for (int i=0;i<table->capacity;i++) {
        table->children[i] = create_bod(0,0);
        table->indexes[i] = 0;
    }
    return table;
}

short int generate_range(int upper, int lower) {
    short int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

float calculate_distance(BOD *bod1, BOD *bod2) {
    //akože dá sa používať štvrocová vzdialenosť, ale to dáme ako potom improvement
    //použijeme klasiku s odmocninou

    float distance = pow(bod2->x-bod1->x,2) + pow(bod2->y-bod1->y,2);

    return sqrtf(distance);
}


float **create_matica_vzd(HASHTABLE *table) {

    //napadla ma optimalizácia: ak vypočítam, že vzdialenosť je viac ako 500, tak to ani nedám do matice, ďalšie ušetrené miesto

    int n = table->n_of_children;
    float **matica_vzd = malloc(sizeof(float*)*n);



    //rovno spravím dolny trojuholnik
    for (int i=0;i<n;i++) {
        float *row = malloc(sizeof(float)*n);
        for (int j=0;j<=i;j++) {
            if (i==j) {
                row[j] = 10000; //random hodnota, aj tak ju nebudem cekovat
                continue;
            }

            float distance = calculate_distance(table->children[table->indexes[j]],table->children[table->indexes[i]]);

            printf("%g ",distance);
            row[j] = distance;

        }
        matica_vzd[i] = row;
        printf("\n");
    }


    return matica_vzd;
}





HASHTABLE *add_hash(HASHTABLE *table, BOD *dummy, SEARCH_RES *res) {
    int x = dummy->x;
    int y = dummy->y;
    uint64_t hash = (x * 73856093) ^ (y * 19349663);
    size_t index = hash % table->capacity;
    if (table->children[index]->assigned == 1) { //kolízia
        //printf("kolizia\n");

        res = ll_search(table->children[index],dummy, res);

        if (!res->found) {
            BOD *new_b = create_bod(x,y);
            new_b->assigned = 1;
            res->tail->next = new_b;
            table->indexes[table->n_of_children] = (int)index;
            table->n_of_children += 1;
        }
        else {
            printf("DUPLICATE");
        }
    }
    else {
        BOD *new = table->children[index];
        new->x = x;
        new->y = y;
        new->assigned = 1;

        table->indexes[table->n_of_children] = (int)index;
        table->n_of_children += 1;
    }
    return table;
}


HASHTABLE* prvotne_body(int n,HASHTABLE *table) {
    //nesmu byť rovnaké tie body

    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));
    while (table->n_of_children <= n) {
        short int x = generate_range(5000,-5000);
        short int y = generate_range(5000,-5000);

        dummy->x = x;
        dummy->y = y;

        //cek ci je v zozname
        table = add_hash(table, dummy, res);
    }
    return table;
}

HASHTABLE* druhotne_body(int n, HASHTABLE *table) {
    int target = table->n_of_children + n;
    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));

    int x_offset_1, x_offset_2;
    int y_offset_1, y_offset_2;


    while (table->n_of_children != target) {
        //nahodne číslo 0 až n_of_children

        short int x = generate_range(table->n_of_children-1,0);
        BOD *selected = table->children[table->indexes[x]];

        register int bod_x = selected->x;
        register int bod_y = selected->y;

        //generujeme offset
        x_offset_1 = (bod_x - 100 < -5000) ? -5000 - bod_x : -100;
        x_offset_2 = (bod_x + 100 > 5000) ? 5000 - bod_x : 100;
        y_offset_1 = (bod_y - 100 < -5000) ? -5000 - bod_y : -100;
        y_offset_2 = (bod_y + 100 > 5000) ? 5000 - bod_y : 100;

        short x_offset = generate_range(x_offset_2,x_offset_1);
        short y_offset = generate_range(y_offset_2,y_offset_1);
        register int new_x = bod_x + x_offset, new_y = bod_y + y_offset;

        //BOD *new_b = create_bod(new_x,new_y);
        dummy->x = new_x;
        dummy->y = new_y;

        table = add_hash(table, dummy,res);

    }
    return table;
}








void print_arr(HASHTABLE *table) {
    for (int i=0;i<table->n_of_children;i++) {
        if (table->children[table->indexes[i]]) {
            printf("BOD %d: (%d,%d)\n",i, table->children[table->indexes[i]]->x,table->children[table->indexes[i]]->y);
        }
    }
}




int main() {
    printf("Hello, World!\n");

    struct timeval start, end;

    int prvotne = 5;
    int druhotne = 5;


    int capacity = prvotne + druhotne;
    HASHTABLE *table = create_table(capacity);
    table = init_table(table);


    table = prvotne_body(prvotne,table);
    table = druhotne_body(druhotne,table);

    print_arr(table);
    printf("heo");
    gettimeofday(&start, NULL); // Začiatok merania


    create_matica_vzd(table);

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
 * 100000+100000: 0.018s
 */


//CALCULATE DISTANCE



//CREATE_MATICA_VZD
/*
 *
 * alokovanie polovice matice bez diagonaly:
 * 10+10: 0.0015s
 * 10000+10000: 0.5s
 * 20000+20000: 2s cca
 *
 * pocitanie vzdialenosti + alokacia:
 *
 * TO-DO: premenit hash table na tabulku normálnu predtým ako budem robiť maticu_vzd, kvoli nedostupnym prvkom v LL
 *
 *
 */





