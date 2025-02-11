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
    int capacity;
    int in_heap; //pocet prvkov v heape
} CLUSTER;

typedef struct cluster_list {
    int active_clusters;
    int capacity;
    CLUSTER *clusters;
} CLUSTER_LIST;


typedef struct heap_child {
    int i;
    int j;
    int dist;
} HEAP_CHILD;


typedef struct heap {
    int heapsize;
    int capacity;
    HEAP_CHILD *children;
} HEAP;

void heapify_down(HEAP *heap, int idx, CLUSTER_LIST *clist);

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
        if (table->children[i] != NULL && table->children[i]->assigned == 1) {

            //dealokovanie LL okrem head

            BOD *current = table->children[i]->next; //začneme od druhého prvku
            BOD *next_node;

            while (current != NULL) {
                next_node = current->next;
                free(current);
                current = next_node;

            }
            table->children[i]->next = NULL;
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

        while (current != NULL) {
            next_node = current->next;
            free(current);
            current = next_node;
        }
        table->children[i] = NULL;

    }
    free(table->children);
    table->children = NULL;
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
        //printf("added new element at %d with values %d,%d\n",table->n_of_children);
        extracted[table->n_of_children-1] = *new_b;
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

void heapify_up(HEAP *heap, int idx, CLUSTER_LIST *clist) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne


    while (idx > 0) {
        int p_idx = (idx-1)/2;

        if (heap->children[p_idx].dist > heap->children[idx].dist) { //ak je platný a
            //swap mna a parenta, swap ala C

            HEAP_CHILD tmp = heap->children[p_idx];
            heap->children[p_idx] = heap->children[idx];
            heap->children[idx] = tmp;



            if (clist != NULL && clist->clusters[heap->children[idx].i].n_of_children == 0) { //našli sme neplatny prvok
                heap->children[idx].dist = 0; //rezervované pre neplatne prvky
                heapify_down(heap,idx,clist);
                //printf("sending %d,%d to top\n",heap->children[idx].i,heap->children[idx].j);
            }

        }
        idx = p_idx;

    }
}

void heapify_down(HEAP *heap, int idx, CLUSTER_LIST *clist) {
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

            //ak by delete neplatneho prvku znamenal: set dist na 0, heapify up a remove_min
            if (clist->clusters[heap->children[idx].i].n_of_children == 0) { //našli sme neplatny prvok
                heap->children[idx].dist = 0; //rezervované pre neplatne prvky
                heapify_up(heap,idx,clist);
                //printf("sending %d,%d to top\n",heap->children[idx].i,heap->children[idx].j);
            }
        }
        else {
            break; //koniec heapify-down
        }
    }
}

void add_child(HEAP *heap, const int i, const int j, const int dist,CLUSTER_LIST *clist) {
    heap->children[heap->heapsize].i = i;
    heap->children[heap->heapsize].j = j;
    heap->children[heap->heapsize].dist = dist;

    heap->heapsize += 1;

    if (heap->heapsize == heap->capacity) {
        heap->capacity *= 1.5; // *2
        printf("resized to: %d\n",heap->capacity);
        heap->children = realloc(heap->children, heap->capacity*sizeof(HEAP_CHILD));
    }

    heapify_up(heap,heap->heapsize - 1,clist);
}

HEAP_CHILD remove_min(HEAP *heap, CLUSTER_LIST *clist) {
    if (heap->heapsize == 0) {
        printf("prázdny heap\n");
        HEAP_CHILD dummy = {0,0,-1}; //-1 je neplatné dist
        return dummy;
    }


    HEAP_CHILD min = heap->children[0];
    heap->heapsize -= 1;
    heap->children[0] = heap->children[heap->heapsize];

    heapify_down(heap,0,clist);

    return min;
}

/*int remove_any_heap(int idx,HEAP *heap) {
    if (heap->heapsize == 0) { //nemalo by nastať nikdy, ale poistka
        printf("prázdny heap 2\n");
        return -1; //-1 je neplatné dist
    }

    if (idx >= heap->heapsize) {
        printf("Neplatný index: %d\n", idx);
        return -1;
    }

    //nahradiť s posledným prvkom, tento odstrániť, znížiť heapsize a potom heapify up/down podľa situácie
    //swap ala C
    heap->heapsize--;
    heap->children[idx] = heap->children[heap->heapsize]; //posledný platný prvok

    //zistiť aká je situácia
    //heapify-up? platí, že parent idx je menší ako náš idx

    if (idx == 0) return 0; //0 je ok

    int p_idx = (idx-1)/2;
    if (heap->children[p_idx].dist > heap->children[idx].dist) { //treba vykonať heapify-up, lebo nesedí min-heap property
        heapify_up(heap, idx);
        return 0;
    }

    //cek ci netreba spraviť heapify-down
    int left = 2*idx+1;
    int right = left+1;

    int treba = 0; //0-nie 1-ano
    if (left < heap->heapsize && heap->children[left].dist < heap->children[idx].dist) {
        treba = 1;
    }

    if (treba == 0 && right < heap->heapsize && heap->children[right].dist < heap->children[idx].dist) {
        treba = 1;
    }

    if (treba) {
        heapify_down(heap,idx);
    }

    return 0;
}*/






short int generate_range(int upper, int lower) {
    short int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

int calculate_distance(BOD *bod1, BOD *bod2) {
    //dáme rovno štvorcovú vzdialenosť
    const int diff1 = bod2->x-bod1->x;
    const int diff2 = bod2->y-bod1->y;


    const int distance = diff1*diff1 + diff2*diff2;
    return (distance > 250000) ? 0 : distance;
}

int calculate_distance_simple(BOD_SIMPLE *bod1, BOD_SIMPLE *bod2) {
    const int diff1 = bod2->x-bod1->x;
    const int diff2 = bod2->y-bod1->y;


    const int distance = diff1*diff1 + diff2*diff2;
    return distance;
}


HEAP *create_matica_vzd(int capacity, BOD *extracted, int *pocet_prvkov) {
    //napadla ma optimalizácia: ak vypočítam, že vzdialenosť je viac ako 500, tak to ani nedám do matice, ďalšie ušetrené miesto
    uint64_t n = capacity;
    //uint64_t size = (n*n-n)/2; //počet prvkov dolneho trojuholníku mínus diagonála


    //postupné zvyšovanie veľkosti lebo neviem koľko toho je, tak dajme na začiatok 50 000
    int initial_size = capacity*40;
    HEAP *heap = create_heap(initial_size);

    //rovno spravím dolny trojuholnik, bez diagonaly
    for (int i=0;i<n;i++) {
        BOD *i1 = &extracted[i];
        for (int j=0;j<i;j++) {
            int distance = calculate_distance(i1,&extracted[j]);

            if (distance != 0) {
                if (heap->heapsize == initial_size) {
                    initial_size = initial_size << 1;
                    heap->capacity = initial_size;

                    HEAP_CHILD *temp = realloc(heap->children, initial_size * sizeof(HEAP_CHILD));
                    if (!temp) {
                        printf("Realloc failed!\n");
                        free(heap->children);  // Uvoľníme starú pamäť
                        exit(1);
                    }
                    heap->children = temp;
                }
                add_child(heap,i,j,distance,NULL);
                pocet_prvkov[i]++;
            }
        }
        //printf("bod %d,%d má %d prvkov\n",extracted[i].x,extracted[i].y,pocet_prvkov[i]);
    }

    printf("je: %d/%d\n",heap->heapsize,heap->capacity);
    return heap;
}

CLUSTER create_cluster(BOD *bod, int elements_in_heap) {
    CLUSTER new;
    new.children = malloc(sizeof(BOD_SIMPLE)*10);

    BOD_SIMPLE new_b;
    new_b.x = (short)bod->x;
    new_b.y = (short)bod->y;

    new.children[0] = new_b;
    new.n_of_children = 1;
    new.capacity = 10;
    new.in_heap = elements_in_heap;
    return new;
}

CLUSTER_LIST* init_cluster_list(BOD *extracted, int capacity, int *pocet_prvkov) {

    CLUSTER_LIST *c_list = malloc(sizeof(CLUSTER_LIST));
    c_list->active_clusters = capacity;
    c_list->capacity = capacity;

    c_list->clusters = malloc(sizeof(CLUSTER)*capacity);

    for (int i=0;i<capacity;i++) {
        c_list->clusters[i] = create_cluster(&extracted[i],pocet_prvkov[i]);
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

        //printf("selected: %d,%d na x: %d\n",selected.x,selected.y,x);

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

CLUSTER* merge_clusters(CLUSTER *c1, CLUSTER *c2) { //predpokladá sa, že chceme spojiť c2 do c1 a vrátiť c1
    //cek ci sa zmestí
    if (c1->n_of_children + c2->n_of_children > c1->capacity) {
        //zvačši c1 buffer
        c1->capacity *= 20;
        //printf("zvyšujem na %d\n",c1->capacity);
        BOD_SIMPLE *temp = realloc(c1->children, c1->capacity * sizeof(BOD_SIMPLE));
        if (!temp) {
            perror("Not enough memory\n");
            return NULL;  // Alebo spraviť chybu inak
        }
        c1->children = temp;
    }

    //prekopírovanie
    int c1_n = c1->n_of_children;
    for (int i=0;i<c2->n_of_children;i++) {
        c1->children[c1_n+i] = c2->children[i];
    }

    c1->n_of_children += c2->n_of_children;
    return c1;
}

BOD_SIMPLE calculate_centroid(CLUSTER *cluster) {
    if (cluster->n_of_children == 1) { //sám sebe centroidom
        return cluster->children[0];
    }

    //výpočet centroidu
    //1. spočítať x a y
    //2. centroid = [priem. x, priem.y]

    //treba sum_x, sum_y, n_of_children
    int sum_x = 0, sum_y = 0;
    for (int i=0;i<cluster->n_of_children;i++) {
        sum_x += cluster->children[i].x;
        sum_y += cluster->children[i].y;
    }

    BOD_SIMPLE res;
    res.x = (short)(sum_x/cluster->n_of_children);
    res.y = (short)(sum_y/cluster->n_of_children);

    return res;
}

BOD_SIMPLE calculate_medoid(CLUSTER *cluster) {
    if (cluster->n_of_children == 1) { //sám sebe centroidom
        return cluster->children[0];
    }


    //každý bod s každým
    int smallest_idx = 0, smallest_sum = 200000; //max možná vzdialenosť skoro
    int sum_col;
    for (int i=0;i<cluster->n_of_children;i++) {
        sum_col = 0;
        for (int j=0;j<cluster->n_of_children;j++) {
            if (i == j) { //keď tak odstran ak prevýši zisk
                continue;
            }
            int dist = calculate_distance_simple(&cluster->children[i],&cluster->children[j]);
            sum_col += dist;
        }
        if (sum_col < smallest_sum) {
            smallest_sum = sum_col;
            smallest_idx = i;
        }
    }

    BOD_SIMPLE res = cluster->children[smallest_idx];
    return res;

}

int find_avg_dist(CLUSTER *cluster, int mode) {
    BOD_SIMPLE bod;
    if (mode == 0) {
        bod = calculate_centroid(cluster);
    }
    else {
        bod = calculate_medoid(cluster);
    }

    //vzdialenost všetkých bodov clustera od centroidu/medoidu
    int sum_avg = 0;
    for (int i=0;i<cluster->n_of_children;i++) {

        int dist = calculate_distance_simple(&cluster->children[i],&bod);
        sum_avg += dist;
    }

    int avg_dist = sum_avg/cluster->n_of_children;
    //printf("avg dist: %d\n",avg_dist);
    return (avg_dist <= 250000) ? 0 : 1; //500**2
}




CLUSTER_LIST *aglomeratne_clusterovanie(CLUSTER_LIST *clusters, HEAP *heap, int mode) { //mode 0 - centroid, 1 - medoid
    struct timeval start, end;
    while (1) {
        gettimeofday(&start, NULL);
        HEAP_CHILD min = remove_min(heap,clusters);
        if (min.dist == -1) {
            break; //prázdna halda
        }
        //clusters->clusters[min.i].in_heap--;
        while (clusters->clusters[min.i].n_of_children == 0 || clusters->clusters[min.j].n_of_children == 0) { //ak je jeden z nich "odstránený"
            //printf("ešte znova, removed %d,%d\n",min.i,min.j);
            min = remove_min(heap,clusters);
            //clusters->clusters[min.i].in_heap--;
            if (min.dist == -1) {
                break; //prázdna halda
            }

        }


        //printf("som tu i:%d, j:%d, dist:%d\n",min.i,min.j,min.dist);
        if (clusters->active_clusters == 1 || min.dist == -1) { //nemám čo spájať
            break;
        }

        gettimeofday(&end, NULL);
        double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("\n#################\n");
        //printf("Search: %fs\n",elapsed);


        gettimeofday(&start, NULL);
        //merge clusters, aby sa to nekopírovalo zbytočne
        clusters->clusters[min.i] = *merge_clusters(&clusters->clusters[min.i],&clusters->clusters[min.j]);

        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        //printf("Merge: %fs\n",elapsed);


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


        //vymazať z heapu, je celkom rozumné uvažovať, že treba vymazať okolo n_clusters + 1 prvkov, možno tam bude aj trochu takých čo netreba, ale nie veľa
        //tiež môžeme dať trochu lepšiu optimalizáciu na začiatok, že ak je počet childov == 1, tak sa vypočíta koľko presne má prvkov v heape a nemusí sa celý pejsť
        //stále to je akože O(n) ale uvidíme ako to ovplyvní čas, tá optimalizácia platí pre počet všetkých clusterov na začiatku, čiže kapacita, potom bude
        //safe uvažovať, že môžeme brať n_clusters od každého čiže čím viac toho bude, tak tým rýchlejšie to bude

        //má to 2 časti, vymazať ten vymazaný cluster a 2. časť je nahradiť c1 staré s novými hodnotami, všetky
        //1.časť:
        //clusters->clusters[min.i].in_heap--;


        /*gettimeofday(&start, NULL);
        int to_delete = clusters->clusters[min.i].in_heap + clusters->clusters[min.j].in_heap;
        int deleted = 0; //spoločný delete

        int n = heap->heapsize;
        int indexes_to_delete[to_delete] = {};
        int it = 0;

        printf("heapsize %d, min.i %d min.j %d\n\n",heap->heapsize,min.i,min.j);
        for (int i=0;i<heap->heapsize;i++) {
            //printf("i: %d heap i: %d j: %d\n",i,heap->children[i].i,heap->children[i].j);
            if (heap->children[i].i == min.i || heap->children[i].i == min.j) { //vymaže c1 aj c2 v jednom cykle
                //indexes_to_delete[it++] = i;
                deleted++;
                remove_any_heap(i,heap);
                //printf("deleted %d/%d\n",deleted,to_delete);

            }
            //printf("i: %d\n",heap->children[i].i);
            if (deleted == to_delete) {
                break;
            }
        }
        gettimeofday(&end, NULL);
        elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
        printf("Delete in heap: %fs\n",elapsed);*/

        //presunieme odstranovanie do heapify operácii

        //CHYBA: tým, že sa opravujú pri delete prvky, tak sa vymienajú s inými, ktoré chcem ja nahradiť a odstranuju sa potom špatné indexy

        //remove_many_heap(indexes_to_delete,heap);

        //remove_any_heap(i,heap);


        //vymazanie zhlukov
        //printf("freed %d\n",clusters->clusters[min.j].n_of_children);
        free(clusters->clusters[min.j].children); //ak to nepojde, tak sa dá free len na childov to pojde určite
        clusters->clusters[min.j].children = NULL;
        clusters->clusters[min.j].n_of_children = 0;
        clusters->active_clusters -= 1;







        //for (int i=0;i<clusters->clusters[min.i].n_of_children;i++) {
        //    printf("BOD %d: %d,%d\n",i,clusters->clusters[min.i].children[i].x,clusters->clusters[min.i].children[i].y);
        //}
        gettimeofday(&start, NULL);
        //pocitanie centroidu/medoidu nového clustera
        BOD_SIMPLE centroid_medoid;
        if (mode == 0) {
            centroid_medoid = calculate_centroid(&clusters->clusters[min.i]);
            //printf("Centroid: %d,%d\n",centroid_medoid.x,centroid_medoid.y);
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
        BOD_SIMPLE bod; //možno dynamicky alokovať? todo
        //int added = 0;
        if (mode == 0) { //centroid
            for (int i=0;i<clusters->capacity;i++) { //vzdialenost všetkých clusterov a nás
                if (clusters->clusters[i].n_of_children != 0) {
                    bod = calculate_centroid(&clusters->clusters[i]);
                    int dist = calculate_distance_simple(&centroid_medoid,&bod);

                    if (dist != 0 && dist <= 250000) { //vlozenie do heapu nového riadka
                        add_child(heap,min.i,i,dist,clusters);
//0.0005-0.0008

                        //added++;
                        //printf("added %d,%d a %d\n",min.i,i, dist);
                    }
                }
            }
        }
        else {
            for (int i=0;i<clusters->capacity;i++) { //vzdialenost všetkých clusterov a nás
                if (clusters->clusters[i].n_of_children != 0) {
                    bod = calculate_medoid(&clusters->clusters[i]);
                    int dist = calculate_distance_simple(&centroid_medoid,&bod);

                    if (dist != 0 && dist <= 250000) { //vlozenie do heapu nového riadka
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
    }

    return clusters;
}











void print_arr(HASHTABLE *table) {
    printf("printing table:\n");
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
    printf("end of the table\n");
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

    int *pocet_prvkov = calloc(capacity,sizeof(int)); //inicializuje na 0
    HEAP *heap = create_matica_vzd(capacity,extracted,pocet_prvkov); //vytvor maticu vzdialeností

    //for (int i=0;i<heap->heapsize;i++) {
        //printf("bod %d: i:%d, j:%d, dist:%d\n",i,heap->children[i].i,heap->children[i].j,heap->children[i].dist);
    //}



    CLUSTER_LIST *clusters = init_cluster_list(extracted,capacity,pocet_prvkov);


    gettimeofday(&start, NULL); // Začiatok merania

    clusters = aglomeratne_clusterovanie(clusters,heap,0);



    

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
 *
 *
 * PROBLEM: tým, že neodstranujem ale len pridávam, tak sa mi strašne rýchlo zväčšuje pole a je to hodne pamäte a to je špatne
 *
 */

//MEDOID
/*
 * 1. buď vypočítaš všetko 2 krát ale nemusíš ukladať do matice, čiže nulové pamäťové nároky alebo
 * 2. budeš ukladať do matice, čiže pamäť, ale budeš robiť len prístupy do vedlajších polí a brať zvyšok čo potrebuješ
 * tá 2jka je interesantná, ale problematickejšia a zase pamäť nechcem, uvidím ako pôjde pre veľa bodov ten 1. prístup a keď tak to zmeníme
 */

//AGLOMERATNE CLUSTEROVANIE:
/*
 * stats: toto je nesprávne, bola tam chyba X
 * 20+20000: 5.5s cca len clusterovanie čo je crazy, s výpismi, 4.4s-5s bez veľkých výpisov len resizy
 * 20000+20000: 18.75s s výpismi, je to menší heap lebo je to rozhodené po mape, ale zase viac resizov clusterov, 15.4s bez výpisu
 *
 * PROBLEM: pamäť dochádza lebo nemám efektívne čistenie heapu, proste sa spolieham na to, že sa to samé vymaže, ale to nie je dobre, moc veľký heap
 * +PROBLEM: ostavaju asi aj staré hodnoty pred mergom, čiže sa môžu dať a to nie je dobré, buď nahradiť alebo vymazať
 * TODO: keď toto fixnem, tak dealokovať
 *
 * FIXED: mal som zle pridávanie nového riadka som potreboval cykliť všetko nie len n_clusters, pridal som atribút active_clusters a n_clusters->capacity
 * + pridal som pri vytváraní heapu sa zapíše koľko má ten prvok prvkov v heape, s tým sa potom pri vymazávaní pracuje
 *
 * PROBLEM: mizne mi v heape to čo nemá, treba popozerať ako presne sa hýbe heap, aby to bolo dobre
 *
 * Niečo ako nastav mu dist na 0 napr. čo nemá nikto iný a daj mmu ehapify up a potom daj remove-min toto som dal do heapify-down, nevymaže to všetko, ale je to lepšie
 * ako nič, čas:
 * 20+20000: 20.6s ale niekedy pamäť padne xd
 * 20000+20000: cca 20s
 *
 *
 * HEAPIFY-DOWN/UP: zmena na iteratívny cyklus namiesto rekurzie
 * 20+20000: 12.5s-12.9s ale strašne veľa memory errorov, čiže dosť nestabilné
 * 20000+20000: 16.5s-16.9s
 *
 * bez Clion:
 * 20+20000: 12s cca
 *
 *
 * OPTIMALIZACIA v realokáciach clusterovania, čo sa dá spraviť namiesto toho, je že by sme predalokovali capacity prvkov a potom len mali indexy, že kde začína
 * v tom poli ten cluster konkretny
 *
 */

