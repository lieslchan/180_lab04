#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>

// struct for submatrix
typedef struct minimat {
    size_t n;        // total columns in full matrix 
    size_t start_idx; // which column this thread starts at
    size_t rows;     // how many columns this thread handles
    float* mins;
    float* maxs;
    float* matrix;   // pointer to orig matrix
} submat;

// function for solving mmt
void* mmt(void* arg) {
    submat* mat = (submat*) arg;
    size_t n = mat->n;
    size_t start_idx = mat->start_idx;
    size_t rows = mat->rows;
    float* mins = mat->mins;
    float* maxs = mat->maxs;
    float* X = mat->matrix;  // points to full matrix

    size_t idx_limit = rows*n + start_idx; 
    
    // transform original matrix
    size_t col = 0;
    for (start_idx = start_idx; start_idx < idx_limit; start_idx++){
        col = (col >= n) ? 0 : col;
        X[start_idx] = (X[start_idx] - mins[col]) * maxs[col];
        col++;
    }
    
    start_idx++;

    return NULL;
}

// function for creating threads to compute for mmt 
void createThreads(size_t n, size_t t, float* X) {
    size_t rowCount  = n / t;
    size_t remainder = n % t;

    submat* splits = (submat*)malloc(sizeof(submat) * t);
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t) * t);

    size_t start_idx = 0;

    // allocate for mins and maxs array to store min/max per column 
    float* mins = (float*)malloc(n * sizeof(float));
    float* maxs = (float*)malloc(n * sizeof(float));

    // initialize with first row's values
    for (size_t c = 0; c < n; c++) {
        mins[c] = X[c];
        maxs[c] = X[c];
    }

    // gather min/max per column
    size_t col = 0;
    for (size_t idx = 0; idx < n*n; idx++){
        col = (col >= n) ? 0 : col;
        float val = X[idx];
        if (val < mins[col]) mins[col] = val;
        if (val > maxs[col]) maxs[col] = val;
        col++;
    }

    // pre-compute 1/(max-min)
    for (size_t c = 0; c < n; c++) {
        float range = maxs[c] - mins[c];
        if (range != 0){
            maxs[c] = 1.0f/range;
        } else {
            maxs[c] = 0;
        }
    }

    // store pointers to submatrices in array
    for (size_t i = 0; i < t; i++) {
        splits[i].n = n;
        splits[i].matrix = X;          
        splits[i].start_idx = start_idx;
        splits[i].rows = rowCount + (i == 0 ? remainder : 0);   // add remainder to first split only 
        splits[i].mins = mins;
        splits[i].maxs = maxs;
        start_idx += splits[i].rows*n ;
    }

    // create threads
    for (size_t i = 0; i < t; i++) {
        pthread_create(&tid[i], NULL, mmt, &splits[i]);
    }

    for (size_t i = 0; i < t; i++) {
        pthread_join(tid[i], NULL);
    }

    free(mins);
    free(maxs);
    free(splits);
    free(tid);
}


// main function
int main(){
    struct timespec time_before, time_after;
    int userN;
    int userT;

    printf("Enter value of n for nxn matrix: \n");
    scanf("%d", &userN);
    if (userN <= 0) return 0;
    printf("\n");
    
    printf("Enter number of threads t: \n");
    scanf("%d", &userT);
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

        printf("Initial Matrix: \n");
        for (int i=0; i<userN*userN; i++){
            printf("%f", mat[i]);
        
            if ((i+1) % userN == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }

        // measure execution time
        clock_gettime(CLOCK_MONOTONIC, &time_before);
        createThreads(userN, userT, mat);
        clock_gettime(CLOCK_MONOTONIC, &time_after);
        double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                            (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

        printf("Final Matrix: \n");
        for (int i=0; i<userN*userN; i++){
            printf("%f", mat[i]);
        
            if ((i+1) % userN == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
            
        free(mat);

        printf("Execution time: %f\n", time_elapsed);
    }
}

// sources:
// https://www.geeksforgeeks.org/c/generating-random-number-range-c/
// https://www.geeksforgeeks.org/c/measure-execution-time-with-high-precision-in-c-c/
// https://www.geeksforgeeks.org/c/multithreading-in-c/


