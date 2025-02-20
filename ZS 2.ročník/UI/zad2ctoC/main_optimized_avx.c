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

typedef struct heap {
    HEAP_CHILD *children;
    int heapsize;
    int capacity;
    int reserve;
} HEAP;

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

typedef struct proto_cluster { //16B na cluster * 20000 = 312kB treba to asi alokovat dynamicky, ale to je jedno
    //využijem čo som použil vyššie štruktúru BOD, ktorá má next pointer, ktorý slúžil na tvorbu LL v HASHTABLE
    BOD *head;
    BOD *tail;
} PROTO_CLUSTER;

typedef struct proto_cluster_list {
    PROTO_CLUSTER *clusters; //8+4*n_children to do zmeniť na predalokovanú tabuľku potom ale, zatiaľ to je 48B na cluster
    int active_clusters;
    int capacity; //viem, že ich je capacita, ale to je jedno
} PROTO_CLUSTER_LIST;


//idem generovať body, tu ide nie o generovanie, ale o počítanie matice vzd
HEAP *clean_heap(HEAP *heap, PROTO_CLUSTER_LIST *clist);



int generate_range(int upper, int lower) {
    int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

HEAP *create_heap(int n,int reserve) {

    HEAP *heap = _aligned_malloc(sizeof(HEAP),32);

    heap->children = _aligned_malloc(sizeof(HEAP_CHILD)*n,32);
    heap->heapsize = 0;
    heap->capacity = n;
    heap->reserve = reserve;
    return heap;
}

void heapify_up(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne
    while (idx > 0) {
        int p_idx = (idx-1)/2;

        if (heap->children[p_idx].dist <= heap->children[idx].dist) { //ak je platný a
            break;
        }

        //swap mna a parenta, swap ala C
        HEAP_CHILD tmp = heap->children[p_idx];
        heap->children[p_idx] = heap->children[idx];
        heap->children[idx] = tmp;

        idx = p_idx;
    }
}

void heapify_down(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne

    while (1) {
        int left = 2*idx+1;
        int right = left+1; //2i+1, optimalizácia xd

        //hladame smallest idx
        int smallest = idx;

        if (left < heap->heapsize && heap->children[left].dist < heap->children[smallest].dist) {
            smallest = left;
        }

        if (right < heap->heapsize && heap->children[right].dist < heap->children[smallest].dist) {
            smallest = right;
        }

        //heap down pre smallest idx
        if (smallest != idx) {
            //swap mna a childa
            HEAP_CHILD tmp = heap->children[smallest];
            heap->children[smallest] = heap->children[idx];
            heap->children[idx] = tmp;
            idx = smallest;

        }
        else {
            break; //koniec heapify-down
        }
    }
}



void add_child_buffer(HEAP *heap, HEAP_CHILD *to_add) {
    heap->children[heap->heapsize++] = *to_add; //prekopíruje obsah pointera
    heapify_up(heap,heap->heapsize - 1);
}


HEAP *clean_heap(HEAP *heap, PROTO_CLUSTER_LIST *clist) {

    printf("REBUILDING HEAP, OLD SIZE: %d\n",heap->heapsize); //hybridný prístup, ak sa nepodarí mallocnut nový heap, tak sa bude pokračovať dalej kym nebude miesto na to

    int n = heap->heapsize;
    heap->heapsize = 0;
    int k = 0;

    while (k < n) {
        if (clist->clusters[heap->children[k].i].head == NULL || clist->clusters[heap->children[k].j].head == NULL) {
            heap->children[k] = heap->children[n-1]; //n sa správa ako heapsize
            n--;
            continue;
        }


        //pošle sa do heapu
        add_child_buffer(heap,&heap->children[k]);
        k++;
    }


    if (heap->heapsize < heap->capacity - heap->reserve) {
        heap->reserve = (int)(heap->heapsize*0.2);
        heap->capacity = heap->heapsize + heap->reserve;

        //ATTEMPTING TO REALOC
        /*HEAP_CHILD *new_heap_children = realloc(heap->children,heap->capacity * sizeof(HEAP_CHILD));
        if (new_heap_children) {
            heap->children = new_heap_children;
        }
        printf("realloced to %d\n",heap->capacity);*/
    }



    printf("DONE, new size: %d/%d\n",heap->heapsize, heap->capacity);
    return heap;
}





void add_child(HEAP *heap, const int i, const int j, const float dist,PROTO_CLUSTER_LIST *clist) {
    //temporary uloženie c1 head, aby som mohol vymazať aj jeho prvky z heapu
    /*if (heap->heapsize == heap->capacity) { //REBUILD HEAP - bez alokácií
        BOD *tmp = clist->clusters[i].head;
        clist->clusters[i].head = NULL; //aby clean_heap vymazal aj tieto prvky

        heap = clean_heap(heap, clist);
        clist->clusters[i].head = tmp;
    }*/

    //__builtin_prefetch(&heap->children[heap->heapsize + 1], 1, 3); //toto pomohlo celkom
    int *heapsize = &heap->heapsize;
    HEAP_CHILD tmp = heap->children[*heapsize];
    tmp.i = i;
    tmp.j = j;
    tmp.dist = dist;

    (*heapsize)++;

    //printf("helo");
    //heapify_up(heap,heap->heapsize - 1); //--0.1s
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


HASHTABLE *add_hash(HASHTABLE *table, BOD *dummy, SEARCH_RES *res, BODY *extracted) {
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
        extracted->x[table->n_of_children-1] = new_b->x;
        extracted->y[table->n_of_children-1] = new_b->y;
    }
    return table;
}

BODY *extract_items_from_hs(HASHTABLE *table, BODY *extracted, int last_n) {
    int index = 0;
    for (int i=0;i<table->capacity;i++) {
        BOD *selected = &table->children[i];

        if (selected->assigned == 1) { //ináč nás to nezaujíma
            while (selected != NULL) {
                extracted->x[last_n+index] = selected->x;
                extracted->y[last_n+index] = selected->y;

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


HASHTABLE* druhotne_body(int n, HASHTABLE *table, BODY *extracted) {
    int target = table->n_of_children + n;
    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));

    int x_offset_1, x_offset_2;
    int y_offset_1, y_offset_2;


    while (table->n_of_children != target) {
        //nahodne číslo 0 až n_of_children
        short x = generate_range(table->n_of_children-1,0);
        BOD selected;
        selected.x = (short)(extracted->x[x]); //ušetrené 0.007s? 0.018s->0.011s bez alokácie
        selected.y = (short)(extracted->y[x]);

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


HEAP* create_matica_vzd2(BODY *body, int heap_size,int n, PROTO_CLUSTER_LIST *clist) {
    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, dist, mask, mask2,dist2, threshold;


    int reserve = (int)(heap_size*0.2); //20% rezerva
    HEAP *heap = create_heap(heap_size+reserve,reserve); //20% rezerva

    if (heap == NULL) {
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


            int bitmask = _mm256_movemask_ps(mask2); //vyberie najvyšší bit z každej hodnoty
            if (bitmask == 0) continue;


            //int num_valid = __builtin_popcount(bitmask); //počet 1tiek v maske

            //HEAP_CHILD to_add[8];
            //int indices[8];

            // _mm256_store_ps(distances, dist);

            // Priamo pracujeme s platnými hodnotami
            for (int k = 0; k < 8; k++) {
                if (bitmask & (1 << k)) {
                    add_child(heap,i,j+k,dist2[k],NULL);
                }
            }


        }
    }
    printf("end %d\n",heap_idx);
    return heap;
}



int main() {
    struct timeval start, end;


    int n = PRVOTNE_BODY+DRUHOTNE_BODY;




    //HASHTABLE
    HASHTABLE table; //4B * capacity + 8B = 80 008B ak capacity = 20 000
    table.capacity = (PRVOTNE_BODY < DRUHOTNE_BODY) ? 2*DRUHOTNE_BODY : 2*PRVOTNE_BODY;
    table.free_pointers = (int)(table.capacity*COL_PTR_RATE);
    table.n_of_children = 0;// children sú inicializovaný na náhodné hodnoty
    table.children = (BOD *)malloc(table.capacity * sizeof(BOD));
    table.collision_pointers = malloc(sizeof(BOD*)*table.free_pointers);
    init_table(&table); //inicializuje na 0,0 a assigned=0 a pod.


    BODY* extracted = _aligned_malloc(sizeof(BODY),32);
    HEAP *heap;


    //prvotne generovanie
    prvotne_body(PRVOTNE_BODY,&table);
    extracted = extract_items_from_hs(&table, extracted, 0); //extrakcia po prvom generovaní
    clear_table(&table); //recyklácia tabuľky


    //druhotne generovanie
    druhotne_body(DRUHOTNE_BODY,&table,extracted); //--0.0016s
    extracted = extract_items_from_hs(&table, extracted, PRVOTNE_BODY); //extrakcia, finalne body --0.002s
    free_table(&table); //dealokuj childov tabuľky --0.0025s, nedá sa nijak viac, veď sa tam nič nerobí pomaly, záleží od col_ptrs


    gettimeofday(&start, NULL);
    //vytvorenie heapu
    int valid = get_valid_values_n(extracted,n);
    HEAP *children = create_matica_vzd2(extracted,valid,n, NULL);





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
 * DRUHOTNE_BODY -- 0.025s sekcia
 * momentálne čas: 0.0016s pre 32000 bodov, čiže zhruba ako prvotne generovanie, skúsim niečo zlepšiť
 * ani v extract_items ani free_table sa už viac nedá nič robiť
 *
 * TOTAL zatial: 0.0055s cca, viac už to neviem zrýchliť pre 32000+32000 bodov, generovanie
 *
 *
 * VYTVORENIE HEAPU: ten už je dosť rýchly, ale ešte som tam nedal tvorbu heapu, len kopírovanie
 * vytvorenie heapu trvá 1.25s z pôvodných 7.56s, čo je cool, ale nestačí
 *
 * //0.18s na prechádzanie, čiže nájsť valid a potom znova to vypočítať, zvyšný čas 1.07s je stavanie heapu, alebo pridanie 16 mil. prvkov do heapu
 * bez heapify-up: 0.67s-0.69s
 * teraz celkom 0.74s pre 16mil. prvkov
 *
 * TODO-rovno to dať do extracted v prvotnom generovaní, aby si to nemusel celé iterovať v extract_items
 *
 * IDEA STAVANIA HEAPU:
 * prekopírovať do cez AVX a potom spraviť heapify na to, to ušetrí čas kopírovania
 *
 * IDEA CRAZY 2: prerobiť heap na SoA ako aj body, lebo potom viem hromadne ukladať distances, i, j etc.
 *
 * na porovnanie momentálne: 0.75s pre 32000+32000 prvkov čo je cca 16 mil. prvkov
 *
 */




