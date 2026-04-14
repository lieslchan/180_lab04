#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

// struct for submatrix
typedef struct minimat {
    size_t n;        // total columns in full matrix 
    size_t startCol; // which column this thread starts at
    size_t cols;     // how many columns this thread handles
    float* matrix;   // pointer to orig matrix
    int core;        // which core this submatrix will be run on 
} submat;

// function for solving mmt
void* mmt(void* arg) {
    submat* mat = (submat*) arg;

    // setup core-pinning
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(mat->core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    size_t n = mat->n;
    size_t startCol = mat->startCol;
    size_t cols = mat->cols;
    float* X = mat->matrix;  // points to full matrix

    // allocate for mins and maxs array to store min/max per column 
    float* mins = (float*)malloc(cols * sizeof(float));
    float* maxs = (float*)malloc(cols * sizeof(float));

    // initialize with first row's values
    for (size_t c = 0; c < cols; c++) {
        mins[c] = X[startCol + c];
        maxs[c] = X[startCol + c];
    }

    // gather min/max per column
    for (size_t r = 0; r < n; r++) {
        for (size_t c = 0; c < cols; c++) {
            float val = X[r * n + startCol + c];   // multiply n to row to access values within column 
            if (val < mins[c]) mins[c] = val;
            if (val > maxs[c]) maxs[c] = val;
        }
    }

    // pre-compute 1/(max-min)
    for (size_t c = 0; c < cols; c++) {
        float range = maxs[c] - mins[c];
        if (range != 0){
            maxs[c] = 1.0f/range;
        } else {
            maxs[c] = 0;
        }
        
    }

    // transform original matrix
    for (size_t r = 0; r < n; r++) {
        for (size_t c = 0; c < cols; c++) {
            size_t idx = r * n + startCol + c;
            X[idx] = (X[idx] - mins[c]) * maxs[c];
        }
    }

    free(mins);
    free(maxs);
    return NULL;
}

// function for creating threads to compute for mmt 
void createThreads(size_t n, size_t t, float* X) {
    size_t colCount  = n / t;
    size_t remainder = n % t;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;   // dynamic number of cores depending on machine
    
    submat* splits = (submat*)malloc(sizeof(submat) * t);
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t) * t);
    
    // store pointers to submatrices in array 
    size_t startCol = 0;
    for (size_t i = 0; i < t; i++) {
        splits[i].n = n;
        splits[i].matrix = X;          
        splits[i].startCol = startCol;
        splits[i].cols = colCount + (i == 0 ? remainder : 0);   // add remainder to first split only 
        startCol += splits[i].cols;
        splits[i].core = i % num_cores;     // round robin core assignment
        printf("thread %zu -> core %d\n", i, splits[i].core);   // track whether thread was assigned properly 
    }

    // create threads
    for (size_t i = 0; i < t; i++) {
        pthread_create(&tid[i], NULL, mmt, &splits[i]);
    }

    for (size_t i = 0; i < t; i++) {
        pthread_join(tid[i], NULL);
    }

    free(splits);
    free(tid);
}


// main function
int main(){
    struct timespec time_before, time_after;
    size_t userN;
    size_t userT;

    printf("Enter value of n for nxn matrix: \n");
    scanf("%ld", &userN);
    if (userN <= 0) return 0;
    printf("\n");
    
    printf("Enter number of threads t: \n");
    scanf("%ld", &userT);
    if (userT > userN) return 0;
    if (userN <= 0) return 0;
    printf("\n");
    
    float *mat = (float *)malloc(userN*userN * sizeof(float));

    if (mat != NULL) {
        unsigned int seed = time(0);
        // initialize matrix with random numbers generated with seed
        for (int i=0; i<userN*userN; i++){
            int rd_num = rand_r(&seed) % (100+1);
            mat[i] = rd_num;
        }

        // printf("Initial Matrix: \n");
        // for (int i=0; i<userN*userN; i++){
        //     printf("%f", mat[i]);
        
        //     if ((i+1) % userN == 0) {
        //         printf("\n");
        //     } else {
        //         printf(" ");
        //     }
        // }

        // measure execution time
        clock_gettime(CLOCK_MONOTONIC, &time_before);
        createThreads(userN, userT, mat);
        clock_gettime(CLOCK_MONOTONIC, &time_after);
        double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                            (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

        // printf("Final Matrix: \n");
        // for (int i=0; i<userN*userN; i++){
        //     printf("%f", mat[i]);
        
        //     if ((i+1) % userN == 0) {
        //         printf("\n");
        //     } else {
        //         printf(" ");
        //     }
        // }
            
        free(mat);

        printf("Execution time: %f\n", time_elapsed);
    }
}

// sources:
// https://www.geeksforgeeks.org/c/generating-random-number-range-c/
// https://www.geeksforgeeks.org/c/measure-execution-time-with-high-precision-in-c-c/
// https://www.geeksforgeeks.org/c/multithreading-in-c/
// https://girishjoshi.io/post/glibc-pthread-cpu-affinity-linux/