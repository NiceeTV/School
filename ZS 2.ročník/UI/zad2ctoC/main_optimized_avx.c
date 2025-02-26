#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdalign.h>
#include <immintrin.h>

#define PRVOTNE_BODY 32000
#define DRUHOTNE_BODY 32000 //musí byť dokopy % 8, kvôli AVX, 20024 je % 8
#define TOTAL_BODY (PRVOTNE_BODY+DRUHOTNE_BODY)
#define THRESHOLD 250000.0f
#define INDEX_BUFFER_SIZE 50
#define COL_PTR_RATE 0.4
#define BITS_PER_INT 32
#define BIT_ARR_SIZE ((TOTAL_BODY + BITS_PER_INT - 1)/BITS_PER_INT)


//Unit stride
typedef struct body {
    alignas(32) float x[TOTAL_BODY]; //int je viac podporovaný v AVX ako short
    alignas(32) float y[TOTAL_BODY];
    alignas(32) int next_indices[TOTAL_BODY];
} BODY;

typedef struct min_pair {
    int i;
    int j;
} MIN_PAIR;


typedef struct heap {
    int *i_array;
    int *j_array;
    float *dist_array;
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
    int active_clusters;
    int capacity; //viem, že ich je capacita, ale to je jedno
    PROTO_CLUSTER *clusters; //8+4*n_children to do zmeniť na predalokovanú tabuľku potom ale, zatiaľ to je 48B na cluster
} PROTO_CLUSTER_LIST;









//idem generovať body, tu ide nie o generovanie, ale o počítanie matice vzd
HEAP *clean_heap(HEAP *heap, PROTO_CLUSTER_LIST *clist);


int generate_range(int upper, int lower) {
    int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

HEAP *create_heap(int n,int reserve) {

    HEAP *heap = _aligned_malloc(sizeof(HEAP),32);

    heap->i_array = _aligned_malloc(sizeof(int)*n,32);
    heap->j_array = _aligned_malloc(sizeof(int)*n,32);
    heap->dist_array = _aligned_malloc(sizeof(float)*n,32);


    heap->heapsize = 0;
    heap->capacity = n;
    heap->reserve = reserve;
    return heap;
}

void heapify_up(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne
    while (idx > 0) {
        int p_idx = (idx-1)/2;

        if (heap->dist_array[p_idx] <= heap->dist_array[idx]) { //ak je platný
            break;
        }


        //swap mna a parenta, swap ala C
        int temp_i = heap->i_array[p_idx];
        int temp_j = heap->j_array[p_idx];
        float temp_dist = heap->dist_array[p_idx];

        // Výmenná operácia
        heap->i_array[p_idx] = heap->i_array[idx];
        heap->j_array[p_idx] = heap->j_array[idx];
        heap->dist_array[p_idx] = heap->dist_array[idx];

        heap->i_array[idx] = temp_i;
        heap->j_array[idx] = temp_j;
        heap->dist_array[idx] = temp_dist;

        idx = p_idx;
    }
}

void heapify_down(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne
    int n = heap->heapsize;

    int temp_i = heap->i_array[idx];
    int temp_j = heap->j_array[idx];
    float temp_dist = heap->dist_array[idx];

    while (2*idx +1 < n) {
        int left = 2*idx+1;
        int right = left+1; //2i+1, optimalizácia xd
        int child = left;


        if (right < n && heap->dist_array[right] < heap->dist_array[left]) {
            child = right;
        }


        //heap down pre smallest idx
        if (heap->dist_array[child] < temp_dist) {
            heap->i_array[idx] = heap->i_array[child];
            heap->j_array[idx] = heap->j_array[child];
            heap->dist_array[idx] = heap->dist_array[child];
            idx = child;
        } else {
            break; //budem na správnom mieste
        }

        //uložím seba, keď budem dobre, big brain moment
        heap->i_array[idx] = temp_i;
        heap->j_array[idx] = temp_j;
        heap->dist_array[idx] = temp_dist;
    }
}

MIN_PAIR* remove_min(HEAP *heap, MIN_PAIR *min) {
    if (heap->heapsize == 0) {
        printf("prázdny heap\n");
        min->i = -1;
        return min; //-1 je neplatné dist
    }


    //int min = heap->i_array[0];
    min->i = heap->i_array[0];
    min->j = heap->j_array[0];
    heap->heapsize -= 1;

    //printf("dist %f ",heap->dist_array[0]);

    int h_size = heap->heapsize;
    heap->i_array[0] = heap->i_array[h_size];
    heap->j_array[0] = heap->j_array[h_size];
    heap->dist_array[0] = heap->dist_array[h_size];

    heapify_down(heap,0);
    return min;
}




HEAP *clean_heap(HEAP *heap, PROTO_CLUSTER_LIST *clist) {

    printf("REBUILDING HEAP, OLD SIZE: %d\n",heap->heapsize); //hybridný prístup, ak sa nepodarí mallocnut nový heap, tak sa bude pokračovať dalej kym nebude miesto na to

    int n = heap->heapsize;
    heap->heapsize = 0;
    int k = 0;

    while (k < n) {
        if (clist->clusters[heap->i_array[k]].head == NULL || clist->clusters[heap->j_array[k]].head == NULL) {
            heap->i_array[k] = heap->i_array[n-1];  //n sa správa ako heapsize
            heap->j_array[k] = heap->j_array[n-1];
            heap->dist_array[k] = heap->dist_array[n-1];

            n--;
            continue;
        }


        //pošle sa do heapu
        int h_size = heap->heapsize;
        heap->i_array[h_size] = heap->i_array[k];  //n sa správa ako heapsize
        heap->j_array[h_size] = heap->j_array[k];
        heap->dist_array[h_size] = heap->dist_array[k];
        heap->heapsize++;

        heapify_up(heap,heap->heapsize - 1);
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
    if (heap->heapsize == heap->capacity) { //REBUILD HEAP - bez alokácií
        BOD *tmp = clist->clusters[i].head;
        clist->clusters[i].head = NULL; //aby clean_heap vymazal aj tieto prvky

        heap = clean_heap(heap, clist);
        clist->clusters[i].head = tmp;
    }

    //__builtin_prefetch(&heap->children[heap->heapsize + 1], 1, 3); //toto pomohlo celkom
    int heapsize = heap->heapsize;
    heap->i_array[heapsize] = i;
    heap->j_array[heapsize] = j;
    heap->dist_array[heapsize] = dist;
    heap->heapsize++;


    //printf("helo");
    heapify_up(heap,heap->heapsize - 1); //--0.1s
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


HASHTABLE* prvotne_body(int n,HASHTABLE *table, BODY *extracted) {
    //nesmu byť rovnaké tie body

    SEARCH_RES *res = malloc(sizeof(SEARCH_RES));
    BOD *dummy = malloc(sizeof(BOD));
    while (table->n_of_children != n) {
        short x = generate_range(5000,-5000);
        short y = generate_range(5000,-5000);

        dummy->x = x;
        dummy->y = y;
        //cek ci je v zozname
        table = add_hash(table, dummy, res, extracted);
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
    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, mask, mask2,dist,threshold;

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


HEAP* create_matica_vzd2(BODY *body, int heap_size,int n) {
    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, dist, mask, mask2,dist2, threshold;

    struct timeval start, end;
    gettimeofday(&start, NULL);


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
            //dist2 = _mm256_and_ps(dist, mask2); //vynuluje hodnoty, ktoré nesplnaju podmienku


            int bitmask = _mm256_movemask_ps(mask2); //vyberie najvyšší bit z každej hodnoty
            if (bitmask == 0) continue;



            while (bitmask) {
                int k = __builtin_ctz(bitmask); // zistí index najnižšieho nastaveného bitu
                // Vymažeme tento bit zo "bitmask"
                bitmask &= bitmask - 1;

                //printf("storing @%d: %d %d %f\n",stored_idxs,i,j+offset,dist2[offset]);
                heap->dist_array[heap_idx] = dist[k];
                heap->i_array[heap_idx] = i;
                heap->j_array[heap_idx] = j+k;
                heap_idx++;

            }
        }
    }
    heap->heapsize = heap_idx;
    int h_size = heap->heapsize;

    // Prechádzame vnútorné uzly od spodného rodiča smerom hore
    for (int i = (h_size / 2) - 1; i >= 0; i--) { //--0.2s
        heapify_down(heap, i);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Heap created: %fs\n",elapsed);
    return heap;
}

void set_bit(uint32_t* arr, size_t index, int value) {
    size_t arr_index = index / BITS_PER_INT;        // Určujeme index celého čísla
    size_t bit_index = index % BITS_PER_INT;        // Určujeme index bitu v čísle

    if (value == 1) {
        arr[arr_index] |= (1U << bit_index);         // Nastavíme bit na 1
    } else {
        arr[arr_index] &= ~(1U << bit_index);        // Nastavíme bit na 0
    }
}

// Funkcia na získanie hodnoty bitu (0 alebo 1)
int get_bit(const uint32_t* arr, size_t index) {
    size_t arr_index = index / BITS_PER_INT;        // Určujeme index celého čísla
    size_t bit_index = index % BITS_PER_INT;        // Určujeme index bitu v čísle
    return (arr[arr_index] >> bit_index) & 1;        // Vrátime hodnotu bitu (0 alebo 1)
}





PROTO_CLUSTER_LIST* init_cluster_list(BOD *extracted, int capacity) {
    PROTO_CLUSTER_LIST *c_list = malloc(sizeof(PROTO_CLUSTER_LIST));
    c_list->active_clusters = capacity;
    c_list->capacity = capacity;
    c_list->clusters = malloc(sizeof(PROTO_CLUSTER)*capacity);

    //init clusterov
    for (int i=0;i<capacity;i++) {
        BOD *to_add = malloc(sizeof(BOD));
        *to_add = extracted[i];
        to_add->next = NULL;
        c_list->clusters[i].head = to_add;
        c_list->clusters[i].tail = to_add;
    }
    return c_list;
}






void merge_proto_clusters(BODY* extracted, int c1, int c2) {
    //precykli c2 a prekopíruj na tail c1

    int curr_idx = c1;
    while (extracted->next_indices[curr_idx] != -1) {
        curr_idx = extracted->next_indices[curr_idx];
    }
    //sme na konci, tak pripneme c2
    extracted->next_indices[curr_idx] = c2;
}




BODY *aglomeratne_clusterovanie(BODY *extracted, HEAP *heap, int mode) { //mode 0 - centroid, 1 - medoid
    struct timeval start, end;



    MIN_PAIR *min = malloc(sizeof(MIN_PAIR));
    int active_clusters = TOTAL_BODY;
    uint32_t cluster_maska[BIT_ARR_SIZE];

    //inicializujeme pole na 1, pretože sú aktívne
    for (size_t i = 0; i < BIT_ARR_SIZE; i++) {
        cluster_maska[i] = 1;
    }

    for (int j=0;j<TOTAL_BODY;j++) {
        extracted->next_indices[j] = -1; //init, neukazuje na nič
    }




    while (1) {
        gettimeofday(&start, NULL);
        min = remove_min(heap, min);
        //printf("min i:%d j:%d\n",min->i,min->j);

        if (min->i == -1) {
            break; //prázdna halda
        }


        while (get_bit(cluster_maska,min->i) == 0  || get_bit(cluster_maska,min->j) == 0 ) { //ak je jeden z nich "odstránený"
            //printf("ešte znova, removed %d,%d\n",min.i,min.j);
            min = remove_min(heap,min);
            if (min->i == -1) {
                break; //prázdna halda
            }

        }


        //printf("heap: %d\n",heap->heapsize);
        //printf("som tu i:%d, j:%d, dist:%d\n",min.i,min.j,min.dist);
        if (active_clusters == 1 || min->i == -1) { //nemám čo spájať
            break;
        }

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("\n#################\n");
        printf("Search: %fs\n",elapsed);



        gettimeofday(&start, NULL);

        //merge clusters, aby sa to nekopírovalo zbytočne
        merge_proto_clusters(extracted,min->i,min->j);

        //proto delete
        set_bit(cluster_maska,min->j,0);
        active_clusters--;

        //printf("n child %d\n",clusters->clusters[min->i].n_of_children);


        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("Merge: %fs\n",elapsed);
/*



        gettimeofday(&start, NULL);
        //priemerná vzdialenosť
        int podmienka = find_avg_dist(&clusters->clusters[min.i],mode);

        //ci presiahol priem vzdialenost 500 alebo nie
        if (podmienka == 1) {
            printf("Bola prekročená maximálna priemerná vzdialenosť clustera.\n");
            break;
        }
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("Avg dist: %fs\n",elapsed);




        //vymazanie zhlukov - neodstranuju sa LL lebo tie sa len presuvaju, neviem to ani dealokovat lebo to nie su pointery, tak len viem nastaviť head na NULL
        //printf("freed %d\n",clusters->clusters[min.j].n_of_children);
        clusters->clusters[min.j].head = NULL;
        clusters->clusters[min.j].tail = NULL; //nemusím, ale je to lepšie
        clusters->active_clusters -= 1;



        //rebuild heap musí byť v add_child aby sa nepridalo viac ako je kapacita




        gettimeofday(&start, NULL);
        //pocitanie centroidu/medoidu nového clustera
        BOD centroid_medoid;
        if (mode == 0) {
            centroid_medoid = calculate_centroid(&clusters->clusters[min.i]);
            //printf("Centroid: %d,%d\n",centroid_medoid->x,centroid_medoid->y);
        }
        else {
            centroid_medoid = calculate_medoid(&clusters->clusters[min.i]);
            //printf("Medoid: %d,%d\n",centroid_medoid.x,centroid_medoid.y);
        }
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("Pocitanie stredu: %fs\n",elapsed);


        gettimeofday(&start, NULL);
        //vypocet noveho riadka - potrebujem funkciu na centroid/medoid

        BOD bod;
        //int added = 0;
        if (mode == 0) { //centroid
            for (int i=0;i<clusters->capacity;i++) { //vzdialenost všetkých clusterov a nás
                if (clusters->clusters[i].head != NULL) {
                    //printf("pridavam \n");



                    bod = calculate_centroid(&clusters->clusters[i]);
                    //printf("new centroid %d,%d\n",bod->x,bod->y);

                    int dist = calculate_distance(&centroid_medoid,&bod);
                    if (dist != 0) { //vlozenie do heapu nového riadka
                        if (i == min.i) {
                            printf("pozor chyba\n");



                        }
                        add_child(heap,min.i,i,dist,clusters);
                        //add_child_buffer(heap);

                        //added++;
                        //printf("added %d,%d a %d\n",min.i,i, dist);
                    }
                }
            }
        }
        else {
            for (int i=0;i<clusters->capacity;i++) { //vzdialenost všetkých clusterov a nás
                if (clusters->clusters[i].head != NULL) {
                    bod = calculate_medoid(&clusters->clusters[i]);
                    int dist = calculate_distance(&centroid_medoid,&bod);

                    if (dist != 0) { //vlozenie do heapu nového riadka
                        add_child(heap,min.i,i,dist,clusters);
                        //added++;
                        //printf("added %d,%d a %d\n",min.i,i, dist);
                    }
                }
            }
        }
        //clusters->clusters[min.i].in_heap = added;
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("Pridanie noveho riadka: %fs\n",elapsed);
        //printf("Prvkov v heape: %d\n",heap->heapsize);
        //printf("#################\n\n");
        */
    }




    return extracted;
}








int main() {
    struct timeval start, end;

    int n = TOTAL_BODY;


    //HASHTABLE
    HASHTABLE table; //4B * capacity + 8B = 80 008B ak capacity = 20 000
    table.capacity = (TOTAL_BODY) ? 2*DRUHOTNE_BODY : 2*PRVOTNE_BODY;
    table.free_pointers = (int)(table.capacity*COL_PTR_RATE);
    table.n_of_children = 0;// children sú inicializovaný na náhodné hodnoty
    table.children = (BOD *)malloc(table.capacity * sizeof(BOD));
    table.collision_pointers = malloc(sizeof(BOD*)*table.free_pointers);
    init_table(&table); //inicializuje na 0,0 a assigned=0 a pod.

    //init
    BODY* extracted = _aligned_malloc(sizeof(BODY),32);
    HEAP *heap;


    //prvotne generovanie
    prvotne_body(PRVOTNE_BODY,&table,extracted);
    clear_table(&table); //recyklácia tabuľky


    //druhotne generovanie
    druhotne_body(DRUHOTNE_BODY,&table,extracted); //--0.0016s
    free_table(&table); //dealokuj childov tabuľky --0.0020s, nedá sa nijak viac, veď sa tam nič nerobí pomaly, záleží od col_ptrs


    //vytvorenie heapu
    int valid = get_valid_values_n(extracted,n);
    heap = create_matica_vzd2(extracted,valid,n);



    gettimeofday(&start, NULL);
    //INIT CLUSTER LIST - nechám tak či to zmením, teraz to je LL a šetrí to pamäť
    //IDEA: každý cluster má zoznam indexov v spoločnej pamäti prvkov, nič sa nealokuje, nenapája, len sa menia indexy
    //treba najprv index array pre extracted spoločný, ktorý bod kam patrí do akého clusteru

    //čiže extracted sa používa na počítanie distances


    extracted = aglomeratne_clusterovanie(extracted,heap,0);



    /*for (int i=0;i<n;i++) {
        //printf("%d ",clusters->clusters[i].indexes[0]);
        printf("%d-ty cluster: %g,%g na indexe %d\n",i,extracted->x[clusters->clusters[i].indexes[0]],extracted->y[clusters->clusters[i].indexes[0]],clusters->clusters[i].indexes[0]);
    }*/




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
 *
 *
 * STAVANIE HEAPU:
 * prekopírovať do cez AVX a potom spraviť heapify na to, to ušetrí čas kopírovania
 *
 * IDEA CRAZY 2: prerobiť heap na SoA ako aj body, lebo potom viem hromadne ukladať distances, i, j etc.
 *
 * na porovnanie momentálne: 0.75s pre 32000+32000 prvkov čo je cca 16 mil. prvkov
 * nechám to tak, je to celkom decentné
 *
 * Idem fixnut extracted v prvom generovaní, je to malý fix a asi mi nič nedá, ale estetika,
 * ZMENA: odstránil som extract_items_from_hs a je to rovno v tých generovaniach
 * Druhotne generovanie: 0.0025 -> 0.002s
 *
 *
 * TODO problem s clean heapom je, že ak aj realokuješ, tak sa to stále musí prekopírovať - fix: mallocnut novú array a tam to pridať
 * INIT CLUSTER LIST
 * možno riešenie chunkbufferov, že každý bude mať svoj buffer a prekopírujú sa ak bude potrebovať, inač sa uvolní
 * otázka či potrebujem tie cluster_indexes vôbec, ale asi hej aby som vedel čo je active clsuter a čo nie, ale teoreticky nie viem to aj cez index buffer napr.
 *
 * ja čoskoro olutujem toto super komplikovanie, dalo by sa zrušiť index buffer list a všetko si pamätať v clusterovacej funkcii teoreticky
 *
 * 
 */




