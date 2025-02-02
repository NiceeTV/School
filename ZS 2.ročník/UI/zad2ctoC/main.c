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


size_t hash_xy(int x, int y, size_t capacity) {
    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    hash ^= (uint64_t)x * 0x100000001b3; // FNV prime
    hash ^= (uint64_t)y * 0x100000001b3;
    hash ^= (hash >> 32); // Bit shuffle
    hash *= 0x85ebca6b;    // Murmur-inspired mix
    hash ^= (hash >> 16);
    return hash % capacity;
}




HASHTABLE *add_hash(HASHTABLE *table, BOD *new_b, SEARCH_RES *res) {

    uint64_t hash = (new_b->x * 73856093) ^ (new_b->y * 19349663);
    size_t index = hash % table->capacity;

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

        short int x = rand() % (5000-(-5000)+1) - 5000;
        short int y = rand() % (5000-(-5000)+1) - 5000;

        BOD *new_b = create_bod(x,y);
        
        //cek ci je v zozname
        table = add_hash(table, new_b,res);
        //printf("Elements: %d/%d\n",table->n_of_children,table->capacity);
    }

    return table;
}

HASHTABLE* druhotne_body(int n, HASHTABLE *table) {
    int target = table->n_of_children + n;
    while (table->n_of_children != target) {
        //nahodne číslo 0 až n_of_children






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
    gettimeofday(&start, NULL); // Začiatok merania

    int capacity = 100000;
    HASHTABLE *table = create_table(capacity);
    table = init_table(table);


    prvotne_body(100000,table);

    //print_arr(table);



    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time Elapsed: %fs",elapsed);

}

//GENERATE_BODY_PRVOTNE
//before optim:
/*
 * 50000: 1.14s
 * 100000: 4.66s-5.2s
 *
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








