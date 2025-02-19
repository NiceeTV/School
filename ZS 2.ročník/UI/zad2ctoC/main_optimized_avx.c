#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdalign.h>
#include <immintrin.h>

#define PRVOTNE_BODY 32000
#define DRUHOTNE_BODY 32000 //musí byť dokopy % 8, kvôli AVX, 20024 je % 8
#define THRESHOLD 250000.0f
#define COL_PTR_RATE 0.4


//Unit stride
typedef struct body {
    alignas(32) float x[PRVOTNE_BODY+DRUHOTNE_BODY]; //int je viac podporovaný v AVX ako short
    alignas(32) float y[PRVOTNE_BODY+DRUHOTNE_BODY];
} BODY;

typedef struct heap_child {
    int i;
    int j;
    float dist;
} HEAP_CHILD;


typedef struct bod  {
    struct bod *next;
    short x;
    short y;
    unsigned char assigned;
} BOD;

typedef struct hashtable {
    BOD *children;
    BOD **collision_pointers;
    int capacity;
    int n_of_children;
    int free_pointers;

} HASHTABLE;

typedef struct seach_res {
    BOD* tail;
    int found;
} SEARCH_RES;


//idem generovať body, tu ide nie o generovanie, ale o počítanie matice vzd


int generate_range(int upper, int lower) {
    int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}




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

BOD* create_bod(short x, short y) {
    BOD *new_b = malloc(sizeof(BOD));
    new_b->x = x;
    new_b->y = y;
    new_b->assigned = 0; //0 znamená alokovaný, ale nepoužitý, náhrada za NULL
    new_b->next = NULL;
    return new_b;
}

void init_table(HASHTABLE *table) {
    for (int i = 0; i < table->capacity; i++) {
        BOD *child = &table->children[i]; //uložíme pre efektivitu aby sa nemuselo pristupovať trilion krat
        child->x = 0;
        child->y = 0;
        child->assigned = 0;
        child->next = NULL;
    }

    for (int j=0;j<table->free_pointers;j++) {
        table->collision_pointers[j] = create_bod(0,0);
    }

}

HASHTABLE *clear_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    for (int i=0;i<table->capacity;i++) {
        if (table->children[i].assigned == 1) {
            table->children[i].next = NULL;
            table->children[i].assigned = 0;
        }
    }
    table->free_pointers = (int)(table->capacity*COL_PTR_RATE);
    return table;
}

void free_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    free(table->children);
    table->children = NULL;

    for (int j=0;j<table->capacity*0.4;j++) {
        free(table->collision_pointers[j]);
    }
    free(table->collision_pointers);
}


HASHTABLE *add_hash(HASHTABLE *table, BOD *dummy, SEARCH_RES *res, BOD *extracted) {
    short x = dummy->x;
    short y = dummy->y;
    short success = 0;
    BOD *new_b;
    uint64_t hash = ((x * 73856093) ^ (y * 19349663));
    hash = (hash ^ (hash >> 32)) * 0x9e3779b97f4a7c15; // Vylepšená hash funkcia
    size_t index = hash % table->capacity;

    if (table->children[index].assigned == 1) { //kolízia
        res = ll_search(&table->children[index],dummy, res);
        if (!res->found) {
            if (table->free_pointers) {
                new_b = table->collision_pointers[table->free_pointers-1]; //dávam z predalokovaného pola pointerov
                table->free_pointers--;
                new_b->next = NULL;
                new_b->x = x;
                new_b->y = y;
            }
            else {
                new_b = create_bod(x,y);
            }

            new_b->assigned = 1;
            res->tail->next = new_b;
            table->n_of_children += 1;
            success = 1;
        }
        //deleted DUPLICATED print
    }
    else {
        new_b = &table->children[index];
        new_b->x = x;
        new_b->y = y;
        new_b->assigned = 1;

        table->n_of_children += 1;
        success = 1;
    }

    if (success && extracted != NULL) {
        extracted[table->n_of_children-1] = *new_b;
    }
    return table;
}

BOD *extract_items_from_hs(HASHTABLE *table, BOD *extracted, int last_n) {
    int index = 0;
    for (int i=0;i<table->capacity;i++) {
        BOD *selected = &table->children[i];

        if (selected->assigned == 1) { //ináč nás to nezaujíma
            while (selected != NULL) {
                extracted[last_n+index] = *selected;
                index++;
                selected = selected->next;
            }
        }
    }
    return extracted;
}

HASHTABLE* prvotne_body(int n,HASHTABLE *table) {
    //nesmu byť rovnaké tie body

    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));
    while (table->n_of_children != n) {
        short x = generate_range(5000,-5000);
        short y = generate_range(5000,-5000);

        dummy->x = x;
        dummy->y = y;
        //cek ci je v zozname
        table = add_hash(table, dummy, res, NULL);
    }
    free(dummy);
    free(res);
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
        short x = generate_range(table->n_of_children-1,0);
        BOD selected = extracted[x]; //ušetrené 0.007s? 0.018s->0.011s bez alokácie


        short bod_x = selected.x;
        short bod_y = selected.y;


        //generujeme offset
        x_offset_1 = (bod_x - 100 < -5000) ? -5000 - bod_x : -100;
        x_offset_2 = (bod_x + 100 > 5000) ? 5000 - bod_x : 100;
        y_offset_1 = (bod_y - 100 < -5000) ? -5000 - bod_y : -100;
        y_offset_2 = (bod_y + 100 > 5000) ? 5000 - bod_y : 100;

        short x_offset = generate_range(x_offset_2,x_offset_1);
        short y_offset = generate_range(y_offset_2,y_offset_1);

        dummy->x = (short)(bod_x + x_offset);
        dummy->y = (short)(bod_y + y_offset);


        table = add_hash(table, dummy,res,extracted); //toto zase pridá veľa sekúnd a preváži zisk vyššie
    }
    free(dummy);
    free(res);
    return table;
}



int get_valid_values_n(BODY *body, int n) {
    int valid = 0;
    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, mask, mask2,dist, dist2,threshold;

    int size;
    threshold = _mm256_set1_ps(THRESHOLD);
    for (int i = 0; i < n; i++) {
        xi = _mm256_set1_ps(body->x[i]); //[xi,xi,xi,xi,,,xi] 8 krát pre každý prvok
        yi = _mm256_set1_ps(body->y[i]);

        size = i / 8 + 1;

        for (int j = 0; j < size * 8; j += 8) {
            xj = _mm256_load_ps(&body->x[j]); //načíta 8 čísel z pola, čiže [x1,x2,x3,...x8]
            yj = _mm256_load_ps(&body->y[j]); //načíta 8 čísel z pola, čiže [y1,y2,y3,...y8]

            dx = _mm256_sub_ps(xi, xj); //rozdiel xi-xj
            dy = _mm256_sub_ps(yi, yj); //rozdiel yi-yj

            dx2 = _mm256_mul_ps(dx, dx); //dx**2
            dy2 = _mm256_mul_ps(dy, dy); //dy**2

            dist = _mm256_add_ps(dx2, dy2); //dx**2 + dy**2
            mask = _mm256_cmp_ps(dist, threshold, _CMP_LE_OQ); //maska pre <= 250000

            mask2 = _mm256_cmp_ps(dist,_mm256_set1_ps(0.0f), _CMP_NEQ_OQ);
            mask2 = _mm256_and_ps(mask,mask2);

            int bitmask = _mm256_movemask_ps(mask2);
            valid += __builtin_popcount(bitmask);
        }

    }
    printf("valid values: %d\n", valid);
    return valid;
}


HEAP_CHILD* compute_squared_distance_matrix_avx(BODY *body, int heap_size,int n) {
    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, dist, mask, mask2,dist2, threshold;

    HEAP_CHILD *children = malloc(sizeof(HEAP_CHILD) * heap_size);
    if (children == NULL) {
        printf("chybicka se vloudila\n");
    }

    int size;
    threshold = _mm256_set1_ps(THRESHOLD);

    int heap_idx = 0;
    for (int i = 0; i < n; i++) {
        xi = _mm256_set1_ps(body->x[i]); //[xi,xi,xi,xi,,,xi] 8 krát pre každý prvok
        yi = _mm256_set1_ps(body->y[i]);

        size = i / 8 + 1;

        for (int j = 0; j < size*8; j += 8) {
            //a = x2-x1, b = y2-y1
            //a*a, b*b, a**2 + b**2
            xj = _mm256_load_ps(&body->x[j]); //načíta 8 čísel z pola, čiže [x1,x2,x3,...x8]
            yj = _mm256_load_ps(&body->y[j]); //načíta 8 čísel z pola, čiže [y1,y2,y3,...y8]

            dx = _mm256_sub_ps(xi, xj); //rozdiel xi-xj
            dy = _mm256_sub_ps(yi, yj); //rozdiel yi-yj

            dx2 = _mm256_mul_ps(dx, dx); //dx**2
            dy2 = _mm256_mul_ps(dy, dy); //dy**2

            dist = _mm256_add_ps(dx2, dy2); //dx**2 + dy**2
            mask = _mm256_cmp_ps(dist, threshold, _CMP_LE_OQ); //maska pre <= 250000

            mask2 = _mm256_cmp_ps(dist,_mm256_set1_ps(0.0f), _CMP_NEQ_OQ);
            mask2 = _mm256_and_ps(mask,mask2);
            dist2 = _mm256_and_ps(dist, mask2); //vynuluje hodnoty, ktoré nesplnaju podmienku

            int bitmask = _mm256_movemask_ps(mask); //vyberie najvyšší bit z každej hodnoty
            int num_valid = __builtin_popcount(bitmask); //počet 1tiek v maske

            if (num_valid) {
                for (int k=0;k<8;k++) {
                    if (dist2[k] != 0) {
                        HEAP_CHILD *to_change = &children[heap_idx++];
                        to_change->dist = dist2[k];
                        to_change->i = i;
                        to_change->j = j + k;
                    }
                }
            }
        }
    }
    printf("end %d\n",heap_idx);
    return children;
}




int main() {
    struct timeval start, end;
    alignas(32) BODY* body = _aligned_malloc(sizeof(BODY),32);
    int n = PRVOTNE_BODY+DRUHOTNE_BODY;
    gettimeofday(&start, NULL);
    //HASHTABLE
    HASHTABLE table; //4B * capacity + 8B = 80 008B ak capacity = 20 000
    table.capacity = (PRVOTNE_BODY < DRUHOTNE_BODY) ? 2*DRUHOTNE_BODY : 2*PRVOTNE_BODY;
    table.free_pointers = (int)(table.capacity*COL_PTR_RATE);
    table.n_of_children = 0;// children sú inicializovaný na náhodné hodnoty
    table.children = (BOD *)malloc(table.capacity * sizeof(BOD));
    table.collision_pointers = malloc(sizeof(BOD*)*table.free_pointers);
    init_table(&table); //inicializuje na 0,0 a assigned=0 a pod.

    BOD *extracted = malloc(sizeof(BOD)*n);



    //prvotne generovanie
    prvotne_body(PRVOTNE_BODY,&table);
    extracted = extract_items_from_hs(&table, extracted, 0); //extrakcia po prvom generovaní
    clear_table(&table); //recyklácia tabuľky


    //druhotne generovanie
    druhotne_body(DRUHOTNE_BODY,&table,extracted); //--0.0016s
    extracted = extract_items_from_hs(&table, extracted, PRVOTNE_BODY); //extrakcia, finalne body --0.002s
    free_table(&table); //dealokuj childov tabuľky --0.0028s, nedá sa nijak viac, veď sa tam nič nerobí pomaly, záleží od col_ptrs


    //vytvorenie heapu





    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time Elapsed: %fs",elapsed);

    //vytvorenie heapu
    //int valid = get_valid_values_n(body,n);
    //HEAP_CHILD *children = compute_squared_distance_matrix_avx(body,valid,n);



}


//OPTIMALIZACIE
/*
 *
 * ADD_HASH -
 * zmenil som hash funkciu, ale viac menej sa neviem dostať pod 50% ale hlavne som spravil, aby bola hashtable veľká ako 2x väčšieho z bodov
 * čiže vždy prinajhoršom máš aspoň 2x miesta pre každé generovanie, namiesto toho spravím iné, spravím pool kolíznych pointerov, ktoré sa budú proste pridelovať
 * bude ich okolo 50%, čo je viac akoby trebalo, keďže všeobecne platí, že máme okolo 35% kolízií, ale nie je to tak veľa pamäte a po generovaní sa uvolní
 *
 * PRVOTNE_BODY - 0.00155s sekcia
 * všetko je optimalizované vrámci add_hash, lebo tá funkcia inak nič nerobí, čiže hashtable má pool pointerov pre kolízie
 * aby sa nemuselo alokovať počas tvorby, zväčšenie tabulky a lepšia hash funkcia
 *
 * -znížil som buffer len na 40% a je to o trošilinku rýchlejšie, čas okolo: 0.0012-0.0013s pre 32000 bodov
 *
 * EXTRACT + CLEAR TABLE:
 * čas je konštantne 0.001549s, čiže tam nie je moc čo zlepšovať
 *
 *
 *
 *
 * DRUHOTNE_BODY -- 0.028s sekcia
 * momentálne čas: 0.0016s pre 32000 bodov, čiže zhruba ako prvotne generovanie, skúsim niečo zlepšiť
 * ani v extract_items ani free_table sa už viac nedá nič robiť
 *
 * TOTAL zatial: 0.0055s cca, viac už to neviem zrýchliť pre 32000+32000 bodov, generovanie
 *
 *
 * VYTVORENIE HEAPU: ten už je dosť rýchly, ale ešte som tam nedal tvorbu heapu, len kopírovanie
 *
 *
 *
 *
 */




