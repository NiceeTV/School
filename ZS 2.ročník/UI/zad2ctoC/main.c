#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>


typedef struct bod  {
    int x;
    int y;
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


//operácie LL
BOD* ll_add(BOD* head, BOD* new) {
    BOD* act = head;
    while (act->next != NULL) { //idem na koniec
        act = act->next;
    }

    act->next = new;
    return head;
}

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




HASHTABLE *create_table(int n) {
    HASHTABLE* new_table = malloc(sizeof(HASHTABLE));
    new_table->capacity = n;
    new_table->n_of_children = 0;
    new_table->children = malloc(sizeof(BOD*)*n);

    return new_table;
}

HASHTABLE *init_table(HASHTABLE *table) {
    for (int i=0;i<table->capacity;i++) {
        table->children[i] = NULL;
    }
    return table;
}

BOD *create_bod(int x, int y) {
    BOD *new_b = malloc(sizeof(BOD));
    new_b->x = x;
    new_b->y = y;
    new_b->next = NULL;

    return new_b;
}


short int generate_range(int upper, int lower) {
    short int x = rand() % (upper-(lower)+1) + lower;
    return x;
}




HASHTABLE *add_hash(HASHTABLE *table, BOD *new_b, SEARCH_RES *res) {

    uint64_t hash = (new_b->x * 73856093) ^ (new_b->y * 19349663);
    size_t index = hash % table->capacity;
    //printf("index: %zu",index);
    if (table->children[index] != NULL) {
        //kolízia
        //printf("kolizia\n");

        res = ll_search(table->children[index],new_b, res);

        if (!res->found) {
            res->tail->next = new_b;
            table->n_of_children += 1;
        }
        else {
            printf("DUPLICATE");
        }

    }
    else {
        table->children[index] = new_b;
        table->n_of_children += 1;
    }

    return table;
}




HASHTABLE* prvotne_body(int n,HASHTABLE *table) {
    //nesmu byť rovnaké tie body

    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    while (table->n_of_children <= n) {
        short int x = generate_range(5000,-5000);
        short int y = generate_range(5000,-5000);

        BOD *new_b = create_bod(x,y);
        
        //cek ci je v zozname
        table = add_hash(table, new_b,res);
        //printf("Elements: %d/%d\n",table->n_of_children,table->capacity);
    }

    return table;
}

HASHTABLE* druhotne_body(int n, HASHTABLE *table) {
    int target = table->n_of_children + n;
    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    while (table->n_of_children != target) {
        //nahodne číslo 0 až n_of_children

        short int x = generate_range(table->n_of_children,0);
        while (table->children[x] == NULL) {
            x = generate_range(table->capacity,0);
            //printf("x: %d",x);
        }

        int bod_x = table->children[x]->x;
        int bod_y = table->children[x]->y;

        //printf("nahodny bod: %d,%d. Kapacita: %d/%d\n",bod_x,bod_y, table->n_of_children,table->capacity);

        //generujeme offset
        int x_offset_1 = -100, x_offset_2 = 100;
        int y_offset_1 = -100, y_offset_2 = 100;

        if (bod_x + x_offset_1 < -5000) {
            x_offset_1 = -(bod_x + 5000);
        }

        if (bod_x + x_offset_2 > 5000) {
            x_offset_2 = 5000 - bod_x;
        }

        if (bod_y + y_offset_1 < -5000) {
            y_offset_1 = -(bod_y + 5000);
        }

        if (bod_y + y_offset_2 > 5000) {
            y_offset_2 = 5000 - bod_y;
        }

        short x_offset = generate_range(x_offset_2,x_offset_1);
        short y_offset = generate_range(y_offset_2,y_offset_1);
        int new_x = bod_x + x_offset, new_y = bod_y + y_offset;

        BOD *new_b = create_bod(new_x,new_y);

        table = add_hash(table, new_b,res);
    }
    return table;
}








void print_arr(HASHTABLE *table) {

    for (int i=0;i<table->n_of_children;i++) {
        if (table->children[i] != NULL) {
            printf("BOD %d: (%d,%d)\n",i, table->children[i]->x,table->children[i]->y);
        }
    }

}




int main() {
    printf("Hello, World!\n");

    struct timeval start, end;

    int prvotne = 20000;
    int druhotne = 20000;


    int capacity = prvotne + druhotne;
    HASHTABLE *table = create_table(capacity);
    table = init_table(table);


    table = prvotne_body(prvotne,table);

    //print_arr(table);

    gettimeofday(&start, NULL); // Začiatok merania

    table = druhotne_body(druhotne,table);

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
 */







