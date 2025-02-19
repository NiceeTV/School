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

typedef struct chunk_buffer {
    HEAP_CHILD *children;
    struct chunk_buffer *next;
    int n_of_children;
    int size;
} CHUNKBUFFER;




//idem generovať body, tu ide nie o generovanie, ale o počítanie matice vzd


int generate_range(int upper, int lower) {
    int x = rand() % (upper-lower+1) + lower; //0 až upper
    return x;
}

CHUNKBUFFER *create_buffer(int size) {
    CHUNKBUFFER *buffer = malloc(sizeof(CHUNKBUFFER));
    buffer->n_of_children = 0;
    buffer->size = size;

    //buffer->children = malloc(sizeof(HEAP_CHILD)*size);
    buffer->children = _aligned_malloc(sizeof(HEAP_CHILD) * size, alignof(HEAP_CHILD));
    buffer->next = NULL;



    return buffer;
}


//matica vzd
//alignas(32) float distance_matrix[PRVOTNE_BODY + DRUHOTNE_BODY][PRVOTNE_BODY + DRUHOTNE_BODY];

int get_valid_values_n(BODY *body, int n) {
    int valid = 0;

    __m256 xi, yi, xj, yj, dx, dy, dx2, dy2, mask, mask2,dist, dist2,threshold;



    int size;
    threshold = _mm256_set1_ps(THRESHOLD);
    int heap_idx = 0;

    for (int i = 0; i < n; i++) {
        xi = _mm256_set1_ps(body->x[i]); //[xi,xi,xi,xi,,,xi] 8 krát pre každý prvok
        yi = _mm256_set1_ps(body->y[i]);

        size = i / 8 + 1;

        for (int j = 0; j < size * 8; j += 8) {
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




void print_matrix(int n, float** distance_matrix) {
    printf("Distance matrix:\n");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (distance_matrix[i][j] != 0) {
                printf("i: %d, j: %d, dist: %g\n",i,j, distance_matrix[i][j]);
            }

        }

    }
}









int main() {
    struct timeval start, end;
    alignas(32) BODY* body = _aligned_malloc(sizeof(BODY),32);
    int n = PRVOTNE_BODY+DRUHOTNE_BODY;

    for (int i=0;i<n;i++) {
        int x = generate_range(5000, -5000);
        int y = generate_range(5000, -5000);

        body->x[i] = (float)x;
        body->y[i] = (float)y;
    }

    gettimeofday(&start, NULL);

    //compute_squared_distance_matrix_avx(&body,n);
    int valid = get_valid_values_n(body,n);

    HEAP_CHILD *children = compute_squared_distance_matrix_avx(body,valid,n);


    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    printf("Time Elapsed: %fs",elapsed);


}







