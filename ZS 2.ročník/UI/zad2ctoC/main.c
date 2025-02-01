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
    short int capacity;
    short int n_of_children;
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

SEARCH_RES ll_search(BOD* head, BOD *searched) {

    SEARCH_RES res;
    res.found = 0;

    BOD* act = head;
    while (act->next != NULL) {
        if (act == searched) {
            res.found = 1;
            break;
        }

        act = act->next;
    }
    if (act == searched) {
        res.found = 1;
    }
    res.tail = act;
    return res;
}

BOD* bring_me_the_head(BOD *head) { //tail v ll
    BOD *act = head;
    while (act->next != NULL) {
        act = act->next;
    }
    return act;
}


HASHTABLE *create_table(short n) {
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









HASHTABLE *add_hash(HASHTABLE *table, BOD *new_b) {

    //simple hash: &pointer1 + &pointer2 % max a potom kolízie

    //int index = ((unsigned int)&(new_b->x) + ((unsigned int)&(new_b->y))) % table->capacity;
    int index = (new_b->x * new_b->y) % table->capacity;

    if (table->children[index] != NULL) {
        //kolízia
        printf("kolizia");
        //insert head of ll

        SEARCH_RES found = ll_search(table->children[index],new_b);
        if (!found.found) {
            found.tail->next = new_b;
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




int* prvotne_body(int n,BOD** array, int* array_len) {
    //nesmu byť rovnaké tie body
    int i = 0;
    while (i < n) {

        short int x = rand() % (5000-(-5000)+1) - 5000;
        short int y = rand() % (5000-(-5000)+1) - 5000;

        BOD *new_b = malloc(sizeof(BOD));
        new_b->x = x;
        new_b->y = y;
        
        //cek ci je v zozname
        int found = 0;
        for (int j=0;j<*array_len;j++) {
            if (new_b == array[j]) {
                found = 1;
                break;
            }

        }

        if (!found) {
            array[i] = new_b;
            *array_len += 1;
            i++;
        }
        else {
            printf("Duplicate found.\n");
        }

    }



    return NULL;
}

void print_arr(BOD** array, int* array_len) {

    for (int i=0;i<*array_len;i++) {
        if (array[i] != NULL) {
            printf("BOD %d: (%d,%d)\n",i, array[i]->x,array[i]->y);
        }
    }

}




int main() {
    printf("Hello, World!\n");

    struct timeval start, end;
    gettimeofday(&start, NULL); // Začiatok merania

    int capacity = 100000;
    int arr_len = 0;
    int* arr_len_ptr = &arr_len;

    BOD** array = malloc(capacity*sizeof(BOD*));

    prvotne_body(100000,array,arr_len_ptr);

    //print_arr(array, arr_len_ptr);



    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time Elapsed: %fs",elapsed);

}

//before optim:
/*
 * 50000: 1.14s
 * 100000: 4.66s
 *
 *
 *
 *
 *
 */




