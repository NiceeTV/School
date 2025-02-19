#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <immintrin.h>


typedef struct bod  {
    short x;
    short y;
    unsigned char assigned;
    struct bod *next;
} BOD;

typedef struct hashtable {
    int capacity;
    int n_of_children;
    BOD *children;
} HASHTABLE;

typedef struct seach_res {
    int found;
    BOD* tail;
} SEARCH_RES;

typedef struct bod_simple {
    short x;
    short y;
} BOD_SIMPLE;

typedef struct heap_child {
    int i;
    int j;
    int dist;
} HEAP_CHILD;

typedef struct heap {
    int heapsize;
    int capacity;
    int reserve;
    HEAP_CHILD *children;
} HEAP;

typedef struct chunk_buffer {
    int n_of_children;
    int size;
    HEAP_CHILD *children;
    struct chunk_buffer *next;
} CHUNKBUFFER;


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




//TEST KOD



void heapify_down(HEAP *heap, int idx);
HEAP *clean_heap(HEAP *heap, PROTO_CLUSTER_LIST *clist);

PROTO_CLUSTER *merge_proto_clusters(PROTO_CLUSTER *c1, PROTO_CLUSTER *c2) {
    //precykli c2 a prekopíruj na tail c1
    BOD *current = c2->head;
    BOD *c1_tail = c1->tail;

    c1_tail->next = current;
    c1->tail = c2->tail;


    //treba dealokovať LEN proto cluster, nie jeho prvky, lebo tie sa len presuvaju
    return c1;
}

CHUNKBUFFER *create_buffer(int size) {
    CHUNKBUFFER *buffer = malloc(sizeof(CHUNKBUFFER));
    buffer->n_of_children = 0;
    buffer->size = size;

    buffer->children = malloc(sizeof(HEAP_CHILD)*size);
    buffer->next = NULL;
    return buffer;
}

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
}

HASHTABLE *clear_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    for (int i=0;i<table->capacity;i++) {
        if (table->children[i].assigned == 1) {

            //dealokovanie LL okrem head

            BOD *current = table->children[i].next; //začneme od druhého prvku
            BOD *next_node;

            while (current != NULL) {
                next_node = current->next;
                free(current);
                current = next_node;

            }
            table->children[i].next = NULL;
            table->children[i].assigned = 0;
        }
    }
    return table;
}

void free_table(HASHTABLE *table) {
    //musíme dealokovať childov, aby sme mohli priradiť nových, môžme to spraviť cez to extracted, aby sme sa vyhli dealokácii LL
    for (int i=0;i<table->capacity;i++) {
        //dealokovanie LL okrem head
        BOD *current = table->children[i].next; //začneme od druhého prvku
        BOD *next_node;

        while (current != NULL) {
            next_node = current->next;
            free(current);
            current = next_node;
        }
    }
    free(table->children);
    table->children = NULL;
}

HASHTABLE *add_hash(HASHTABLE *table, BOD *dummy, SEARCH_RES *res, BOD *extracted) {
    short x = dummy->x;
    short y = dummy->y;
    short success = 0;
    BOD *new_b;
    uint64_t hash = (x * 73856093) ^ (y * 19349663);
    size_t index = hash % table->capacity;


    if (table->children[index].assigned == 1) { //kolízia
        res = ll_search(&table->children[index],dummy, res);
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
        new_b = &table->children[index];
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
        BOD *selected = &table->children[i];

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
HEAP *create_heap(int n,int reserve) {
    HEAP *heap = malloc(sizeof(HEAP));
    heap->children = malloc(sizeof(HEAP_CHILD)*n);
    heap->heapsize = 0;
    heap->capacity = n;
    heap->reserve = reserve;
    return heap;
}

void heapify_up(HEAP *heap, int idx) {
    //pri add sa to dá na najbližšie prázdne miesto a potom sa posúva hore kým nie je správne
    while (idx > 0) {
        int p_idx = (idx-1)/2;

        if (heap->children[p_idx].dist > heap->children[idx].dist) { //ak je platný a
            //swap mna a parenta, swap ala C

            HEAP_CHILD tmp = heap->children[p_idx];
            heap->children[p_idx] = heap->children[idx];
            heap->children[idx] = tmp;

        }
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




void add_child(HEAP *heap, const int i, const int j, const int dist,PROTO_CLUSTER_LIST *clist) {
    //temporary uloženie c1 head, aby som mohol vymazať aj jeho prvky z heapu
    if (heap->heapsize == heap->capacity) { //REBUILD HEAP - bez alokácií
        BOD *tmp = clist->clusters[i].head;
        clist->clusters[i].head = NULL; //aby clean_heap vymazal aj tieto prvky

        heap = clean_heap(heap, clist);
        clist->clusters[i].head = tmp;
    }



    heap->children[heap->heapsize].i = i;
    heap->children[heap->heapsize].j = j;
    heap->children[heap->heapsize].dist = dist;
    heap->heapsize += 1;


    heapify_up(heap,heap->heapsize - 1);
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







HEAP_CHILD remove_min(HEAP *heap) {
    if (heap->heapsize == 0) {
        printf("prázdny heap\n");
        HEAP_CHILD dummy = {0,0,-1}; //-1 je neplatné dist
        return dummy;
    }


    HEAP_CHILD min = heap->children[0];
    heap->heapsize -= 1;
    heap->children[0] = heap->children[heap->heapsize];

    heapify_down(heap,0);

    return min;
}

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

//CHUNKBUFFER - HEAP ALLOCATION
HEAP *create_matica_vzd2(int capacity, BOD *extracted) {
    struct timeval start, end;
    gettimeofday(&start, NULL);

    uint64_t n = capacity;
    int initial_size = capacity*40;
    CHUNKBUFFER *buffer = create_buffer(initial_size);
    CHUNKBUFFER *current_buffer = buffer;

    int count = 0;
    //rovno spravím dolny trojuholnik, bez diagonaly
    int buffer_idx = 0;
    for (int i=0;i<n;i++) {
        BOD *i1 = &extracted[i];
        for (int j=0;j<i;j++) {
            int distance = calculate_distance(i1,&extracted[j]);

            if (distance != 0) {
                if (current_buffer->n_of_children == current_buffer->size) { //je plný
                    initial_size = initial_size << 1; //*2
                    buffer_idx = 0;

                    CHUNKBUFFER *new = create_buffer(initial_size);
                    //printf("new chunk of size: %d\n",initial_size);

                    current_buffer->next = new; //začne sa plniť nový buffer
                    current_buffer = new;
                }
                //pridať do aktualneho buffera
                //add_child(heap,i,j,distance,NULL);
                HEAP_CHILD *to_change = &current_buffer->children[buffer_idx++];
                to_change->i = i;
                to_change->j = j;
                to_change->dist = distance;
                current_buffer->n_of_children++;
                count++;
            }
        }
    }

    //printf("count %d\n",count);
    //vložíme to do jedného heapu
    int reserve = count*0.2; //20% rezerva
    HEAP *heap = create_heap(count+reserve,reserve); //20% rezerva
    int i=0;
    while (buffer != NULL) {
        for (int j = 0; j < buffer->n_of_children; j++) {
            add_child_buffer(heap,&buffer->children[j]);
        }
        i++;
        CHUNKBUFFER *tmp = buffer;
        buffer = buffer->next; //prechod na dalsi chunkbuffer
        free(tmp);
    }

    //printf("heapsize voci rezerva %d/%d\n",count,heap->capacity);
    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    printf("Heap allocated: %fs\n",elapsed);
    return heap;
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

        //printf("selected: %d,%d na x: %d\n",selected.x,selected.y,x);

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


BOD calculate_centroid(PROTO_CLUSTER *cluster) {
    if (cluster->head == cluster->tail) { //sám sebe centroidom
        return *cluster->head;
    }
    BOD centroid;
    //výpočet centroidu
    //1. spočítať x a y
    //2. centroid = [priem. x, priem.y]

    //treba sum_x, sum_y, n_of_children
    int sum_x = 0, sum_y = 0;
    int count = 0;
    BOD *act = cluster->head;
    while (act != NULL) {
        sum_x += act->x;
        sum_y += act->y;
        act = act->next;
        count++;
    }

    centroid.x = (short)(sum_x/count);
    centroid.y = (short)(sum_y/count);
    return centroid;
}

BOD calculate_medoid(PROTO_CLUSTER *cluster) {
    if (cluster->head == cluster->tail) { //sám sebe centroidom
        return *cluster->head;
    }


    //každý bod s každým
    int smallest_sum = INT_MAX; //max možná vzdialenosť skoro
    int sum_col;

    BOD *act = cluster->head;
    BOD *smallest = cluster->head;
    BOD *second_head;
    while (act != NULL) {
        sum_col = 0;
        second_head = cluster->head;
        while (second_head != NULL) {
            if (act != second_head) {
                int dist = calculate_distance(act,second_head);
                sum_col += dist;
            }

            second_head = second_head->next;
        }
        if (sum_col < smallest_sum) {
            smallest_sum = sum_col;
            smallest = act;
        }

        act = act->next;
    }


    return *smallest;

}

int find_avg_dist(PROTO_CLUSTER *cluster, int mode) {
    BOD bod;
    if (mode == 0) {
        bod = calculate_centroid(cluster);
    }
    else {
        bod = calculate_medoid(cluster); //lebo je to existujúci bod, tak mi ho proste vráti
    }

    //vzdialenost všetkých bodov clustera od centroidu/medoidu
    int sum_avg = 0;
    int count = 0;
    BOD *act = cluster->head;
    while (act != NULL) {
        int dist = calculate_distance(act,&bod);
        sum_avg += dist;
        act = act->next;
        count++;
    }


    int avg_dist = sum_avg/count;
    //printf("avg dist: %d\n",avg_dist);
    return (avg_dist <= 250000) ? 0 : 1; //500**2
}




PROTO_CLUSTER_LIST *aglomeratne_clusterovanie(PROTO_CLUSTER_LIST *clusters, HEAP *heap, int mode) { //mode 0 - centroid, 1 - medoid
    struct timeval start, end;





    while (1) {
        gettimeofday(&start, NULL);
        HEAP_CHILD min = remove_min(heap);

        if (min.dist == -1) {
            break; //prázdna halda
        }

        while (clusters->clusters[min.i].head == NULL || clusters->clusters[min.j].head == NULL) { //ak je jeden z nich "odstránený"
            //printf("ešte znova, removed %d,%d\n",min.i,min.j);
            min = remove_min(heap);
            if (min.dist == -1) {
                break; //prázdna halda
            }

        }


        //printf("heap: %d\n",heap->heapsize);
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
        clusters->clusters[min.i] = *merge_proto_clusters(&clusters->clusters[min.i],&clusters->clusters[min.j]);

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

    }




    return clusters;
}











void print_arr(HASHTABLE *table) {
    printf("printing table:\n");
    for (int i=0;i<table->n_of_children;i++) {
        if (table->children[i].assigned == 1) {

            if (table->children[i].next == NULL) {
                printf("BOD %d: (%d,%d)\n",i, table->children[i].x,table->children[i].y);
            }
            else {
                printf("Printing LL:\n");
                BOD *act = &table->children[i];
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

    int prvotne = 32000;
    int druhotne = 32000;


    int capacity = prvotne + druhotne;

    PROTO_CLUSTER_LIST *clusters;
    HEAP *heap;
    BOD *extracted = malloc(sizeof(BOD)*capacity);

    //HASHTABLE
    HASHTABLE table; //4B * capacity + 8B = 80 008B ak capacity = 20 000
    table.capacity = capacity;
    table.n_of_children = 0;// children sú inicializovaný na náhodné hodnoty
    table.children = (BOD *)malloc(table.capacity * sizeof(BOD));
    init_table(&table); //inicializuje na 0,0 a assigned=0 a pod.


    //EXTRACTED
    //extracted[capacity]; //4B * capacity = 80 000B ak capacity = 20 000


    //PRVOTNE BODY
    prvotne_body(prvotne, &table); //prvotne body
    extracted = extract_items_from_hs(&table, extracted, 0); //extrakcia po prvom generovaní
    clear_table(&table); //recyklácia tabuľky


    //DRUHOTNE BODY
    druhotne_body(druhotne, &table, extracted); //druhotne generovanie
    extracted = extract_items_from_hs(&table, extracted, prvotne); //extrakcia, finalne body
    free_table(&table); //dealokuj childov tabuľky

    //---- 0.001659s potialto ---//


    //HEAP + CLUSTERS
    heap = create_matica_vzd2(capacity,extracted);
    clusters = init_cluster_list(extracted, capacity); //toto je len 8B môže byť

    free(extracted);

    /*for (int i=0;i<clusters->active_clusters;i++) {
        BOD *act = clusters->clusters[i].head;
        while (act != NULL) {
            printf("bod %d: %d,%d\n",i,act->x,act->y);
            act = act->next;
        }
    }*/





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
 *
 */


//Heap allocation:
/*
 * 20+20000: 2.5s
 * 20000+20000: 3s
 *
 * Nová CHUNKBUFFER alokácia: fess rýchle, ale čas je z cyklenia, ale asi lepšie to už nebude
 * 20+20000: 1.19s
 * 20000+20000: 2.6s-2.8s
 * --neplatí, je to bez heapify, som sprostý--
 * ale aspoň pamäťovo je to banger
 *
 *
 * RECAP: nie úplne moj nápad, ale moja implementácia
 * + žiadne relokácie, akože je tam malloc, ale lepšie ako realloc
 * + ušetrení čas an prekopírovavaní prvkov pri realoku
 * + bezpečnejšie pamäťovo, rýchlejšie
 * + celkom rozumné :D
 * //idea je taká, že sa bude pridávať do blokových polí tie prvky heapu a namiesto realokácie
 * sa vytvorí nový blok a spojí sa so starým
 * nakoniec sa spoja dokopy
 * Je to LL bufferov, nie su vsetky rovnakej velksoti
 * proste ked sa ma realokovat tak sa namiesto toho vytvori vacsi buffer/buffer rovnakej velksoti ako ten posledny
 * a to je akože zvačšenie bez toho, aby sme museli kopírovať prvky pri realokácii
 */

//DELETE V HEAPE:
/* prechádzam experimentálne na metódu, kde ak sa naplní heap so svojou rezervou 20%, tak sa prestavia heap, odstranene prvky sa nahradia poslednym prvkom
 * potom pri dosiahnuti hranice sa všetky "odstránia", testy akú veľkú rezervu je dobré mať a či ju škálovať so zmenšujúcou sa veľkosťou heapu
 * nevýhoda: pamäť navyše lebo 20% je hodne pri veľkom heape, ale malo by sa to zlepšovať rýchlostne lebo bude menej prvkov, možno domyslieť
 * dáko dealokáciu voľného miesta, ale to mi príde veľmi extra, že môže sa alokovať nový heap a dealokovať starý a bude tam len koľko treba
 * vždy bude menší a menší, jedine problém s pamäťou, lebo sa bude často realokovať tým, že 20% heapu je málo, pri 20000 bodoch to je okolo 200 cluster operácii
 * ak je v heape 20 000 000 prvkov + 4 mil. rezerva
 *
 * testy s rebuildom: (bez realokácie)
 * 20+20000: 15.4s-15.6s
 * 20000+20000:
 */

//FIXED, problem bol v recyklovaní premenných Centroid_medoid a Bod, lebo niekde sa nastavil na prvok v clsuteri a potom sa niekde prepísal nechtiac a
/* bol iný centroid potom, to je fixed, momentálne časy:
 *
 * //REBUILD BEZ REALOKACIE + VYPISY REBUILDU + NEMENI SA REZERVA - to je len clusterovanie
 * 20+20000: 22.47s
 * 20000+20000: 39.8s
 * 16000+16000: 15.98s
 * 20+32000: 72.08s hodne špatné zhoršenie
 * 32000+32000: 70.01s-71.3s wat, toto je zas lepšie
 *
 *
 * //REBUILD BEZ REALOKACIE + VYPISY REBUILDU + MENI SA REZERVA - to je len clusterovanie, 3/5 best times
 * 20+20000: 13.837s - best time
 * 20000+20000: 29.46s
 * 16000+16000: 15.28s
 * 20+32000: 37.97s slušné - best time
 * 32000+32000: 66.33s ?? - best time
 *
 *
 *
 * //REBUILD S REALOKACIOU + VYPISY REBUILDU + NEMENI SA REZERVA - to je len clusterovanie
 * 20+20000: 21.81s (zdá sa, že bottleneck je veľkosť heapu pre operácie typu pridanie riadku a search, čiže zmenšiť heap čo najrýchlejšie je efektívne)
 * 20000+20000: 26.36s (rebuild s realokáciou, výpisy rebuildu, nemení sa rezerva, čiže pomáha realokácia)
 * 16000+16000: 15.8s len mierne zhoršenie
 * 20+32000: 66.15s? waaat, čiže častejší rebuild je lepší?
 * 32000+32000: 80.23s (asi by som povedal, že častejší rebuild urýchloval bottleneck, čiže operácie clusterov, rýchlejší výber, pridávanie riadku etc.)
 *
 *
 *
 * //REBUILD S REALOKACIOU + VYPISY REBUILDU + MENI SA REZERVA - to je len clusterovanie, 2/5 best time
 * 20+20000: 14.19s (celkom časté rebuildy)
 * 20000+20000: 25.085s (čiže častejší rebuild, pomalšie lebo sa rebuilduje rýchlejšie a prevyšuje to zisk) - best time
 * 16000+16000: 15.02s akože ?? - best time
 * 20+32000: 38.15s (len to zabralo 1.5GB heap, ale rýchlo to zmizlo aspoň)
 * 32000+32000: 71.21s (fess vela bodov, ale malý heap, čiže to je zdržiavané na tých cluster operáciách)
 *
 *
 * //CONCLUSION: keď sa mení rezerva, tak sú dobré časy, to je 1. vec, hoci sú časy podobné až na to posledné, tak sa stráca čas realokáciou, lebo treba veľa pamäte
 * realoknúť
 */



