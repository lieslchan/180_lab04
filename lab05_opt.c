#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

typedef struct minimat {
    size_t n;        // total rows in full matrix 
    size_t start_idx; // which row this thread starts at
    size_t cols;     // how many rows this thread handles
    float* matrix;   // pointer to orig matrix
    int client_sock;
} submat;

// function signatures
void read_config(char ips[][16], char ports[][6], size_t* userT, char* masterPort, char* masterIP);
void* send_to_slave(void* arg);
void slave(char* userPort, char* masterIp, char* masterPort, char ips[][16], char ports[][6], size_t userN, size_t userT);
void master(char ips[][16], char ports[][6], float* mat, size_t userT, size_t userN, char* masterPort, char* masterIp);
void createThreads(size_t n, size_t t, float* X, int socket_desc);
float* generate_matrix(size_t n);
void mmt(float* mat, size_t rows, size_t n);


void mmt(float* mat, size_t cols, size_t n) {
    // prepare minimums and maximums since values are needed for matrix transformation
    float* mins = (float*)malloc(n * sizeof(float));
    float* maxs = (float*)malloc(n * sizeof(float));

    // initialize with first row's values
    for (size_t c = 0; c < n; c++) {
        mins[c] = mat[c];
        maxs[c] = mat[c];
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

    // transform matrix in place 
    size_t col = 0;
    for (size_t idx = 0; idx < cols*n; idx++){
        col = (col >= n) ? 0 : col;
        mat[idx] = (mat[idx] - mins[col]) * maxs[col];
        col++;
    }
}

// READS CONFIG CORRECTLY
void read_config(char ips[][16], char ports[][6], size_t* userT, char* masterPort, char* masterIP){
    FILE *fptr;

    fptr = fopen("config.txt", "r");

    if (fptr != NULL){
        fscanf(fptr, "%s %s", masterIP, masterPort);

        fscanf(fptr, "%ld", userT);
        // printf("t = %ld\n\n", *userT);

        for (int i=0; i<*userT; i++){
            fscanf(fptr, "%s %s", ips[i], ports[i]);
            // printf("IP %d: %s\n", i, ips[i]);
            // printf("PORT %d: %s\n\n", i, ports[i]);
        }

        fclose(fptr);
    } else {
        printf("File is empty.");
    }
}

void* send_to_slave(void* arg){
    submat* mat = (submat*) arg;

    size_t n = mat->n;
    size_t original_start = mat->start_idx;
    size_t start_idx = mat->start_idx;
    size_t rows = mat->rows;
    float* X = mat->matrix;  // points to full matrix
    float* mins = mat->mins;
    float* maxs = mat->maxs;
    size_t info[2] = {n, rows};

    size_t idx_limit = rows*n + start_idx;

    float *submat = (float *)malloc(n*rows*sizeof(float));
    
    // generate new matrix for sending from pointer 
    size_t idx = 0;
    for (start_idx = start_idx; start_idx < idx_limit; start_idx++){
        submat[idx++] = X[start_idx];
    }
    
    // send n, and number of rows
    if(send(mat->client_sock, info, 2 * sizeof(size_t), 0) < 0){
        printf("Unable to send info to slave\n");
        pthread_exit(NULL);
    }

    // send extracted submatrix
    size_t target = rows*n*sizeof(float);
    size_t total = 0;
    while (total < target) {
        ssize_t s = send(mat->client_sock, (char*)submat + total, target - total, 0);
        if (s <= 0){
            printf("Send error\n");
            pthread_exit(NULL);
        }
        total += s;
    }

    // receive transformed submatrix, store in previously malloc-ed submat array
    total = 0;
    while (total < target) {
        ssize_t s = recv(mat->client_sock, (char*)submat + total, target - total, 0);
        if (s <= 0){
            printf("Recv error\n");
            pthread_exit(NULL);
        }
        total += s;
    }

    // update original matrix values with transformed values 
    idx = 0;
    start_idx = original_start;
    for (start_idx = start_idx; start_idx < idx_limit; start_idx++){
        X[start_idx] = submat[idx++];
    }

    free(submat);
    pthread_exit(NULL);

}

void slave(char* userPort, char* masterIp, char* masterPort, char ips[][16], char ports[][6], size_t userN, size_t userT){
    int idx = 0;
    for (idx = idx; idx<userT; idx++){
        if(strcmp(userPort, ports[idx]) == 0){
            break;
        }
    }

    size_t submatSize = idx==0 ? userN * (userN/userT)+(userN % userT) : userN * (userN/userT);
    
    printf("SUPPOSED SUBMAT SIZE: %ld\n", submatSize);
    
    int socket_desc;
    struct sockaddr_in server_addr, self_addr;
    size_t info[2];

    size_t start_idx;
    struct timespec time_before, time_after;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_desc < 0){
        printf("Error creating socket\n");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(masterPort));
    server_addr.sin_addr.s_addr  = inet_addr(masterIp);

	self_addr.sin_family = AF_INET;
	self_addr.sin_port = htons(atoi(userPort));
	self_addr.sin_addr.s_addr = inet_addr(ips[idx]);
	bind(socket_desc, (struct sockaddr*)&self_addr, sizeof(self_addr));
	
    bool connected = false;
    int retries = 0;
    while (!connected) {
        if(retries == 10){
            printf("Could not connect\n");
        }
        if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
            perror("connect"); 
            sleep(1);
            retries++;
        } else {
            connected = true;
        }
    }
    
    printf("Connected to master at %s: %s\n", masterIp, masterPort);
    
    size_t total = 0;
    clock_gettime(CLOCK_MONOTONIC, &time_before);
    
    // receive info
    if(recv(socket_desc, info, 2 * sizeof(size_t), 0) < 0){
        printf("Unable to recv info\n");
        pthread_exit(NULL);
    }

    size_t target = info[0] * info[1] * sizeof(float);
    float *submat = (float *)malloc(submatSize * sizeof(float));
    
    // receive submatrix
    total = 0;
    while (total < target) {
        ssize_t r = recv(socket_desc, (char*)submat + total, target - total, 0);
        if (r <= 0){
            printf("Recv error\n");
            return;
        }
        total += r;
    }

    // // receive starting index (for checking)
    // if(recv(socket_desc, &start_idx, sizeof(size_t), 0) < 0){
    //     printf("Couldn't receive\n");
    //     pthread_exit(NULL);
    // } 

    
    // check if matrix was received properly 
    printf("Received submatrix (%ld bytes).\n", sizeof(float) * info[0] * info[1]);
    // printf("Starting index: %ld\n\n", start_idx);
    size_t dimes = idx==0 ? (userN/userT)+(userN % userT) : (userN/userT);

    // printf("My Matrix: \n");
    //     for (int i=0; i<userN*dimes; i++){
    //         printf("%f", submat[i]);
    //         if ((i+1) % userN == 0) {
    //             printf("\n");
    //         } else {
    //             printf(" ");
    //         }
    //     }

    // transform matrix
    // TODO: COMPUTE FOR MINS AND MAXS WITHIN SLAVE
    

    mmt(submat, info[1], info[0]);
    
    printf("Transformed Matrix: \n");
        for (int i=0; i<userN*dimes; i++){
            printf("%f", submat[i]);
            if ((i+1) % userN == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }

    // send transformed matrix
    total = 0;
    while (total < target) {
        ssize_t r = send(socket_desc, (char*)submat + total, target - total, 0);
        if (r <= 0){
            printf("Send error\n");
            return;
        }
        total += r;
    }

    clock_gettime(CLOCK_MONOTONIC, &time_after);
            double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                                (time_after.tv_nsec - time_before.tv_nsec) / 1e9;

    printf("Execution time: %f\n", time_elapsed);
    
}

void master(char ips[][16], char ports[][6], float* mat, size_t userT, size_t userN, char* masterPort, char* masterIp){
    int socket_desc;
    struct sockaddr_in server_addr;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc < 0){
        printf("Error creating socket \n"); return;
    }

    // reuse ports
    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(masterPort));
    server_addr.sin_addr.s_addr = inet_addr(masterIp);

    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind\n");
        return;
    }

    if(listen(socket_desc, userT) < 0){
        printf("Error listening \n");
        return;
    }

    printf("Master listening on %s: %s\n", masterIp, masterPort);
    
    createThreads(userN, userT, mat, socket_desc);
}

void createThreads(size_t n, size_t t, float* X, int socket_desc){
    struct sockaddr_in client_addr;
    socklen_t client_size;
    size_t colCount = n/t;
    size_t remainder = n%t;
    struct timespec time_before, time_after;

    // array of submatrices
    submat* splits = (submat*)malloc(sizeof(submat) * t);
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t) * t);

    size_t start_idx = 0;

    clock_gettime(CLOCK_MONOTONIC, &time_before);
    for (size_t i=0; i<t; i++){
        client_size = sizeof(client_addr);
        int client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
        if(client_sock < 0){
            printf("Can't accept\n");
            return;
        }
        printf("Slave connected: %s: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        splits[i].n = n;
        splits[i].matrix = X;          
        splits[i].start_idx = start_idx;
        splits[i].cols = colCount + (i == 0 ? remainder : 0);
        start_idx += splits[i].cols*n;
        splits[i].client_sock = client_sock;
        
        size_t submatSize = i==0 ? n * (n/t)+(n % t) : n * (n/t);
    
    	printf("SUPPOSED SUBMAT %ld SIZE: %ld\n", i, submatSize);

        pthread_create(&tid[i], NULL, send_to_slave, &splits[i]);
    }

    for (size_t i = 0; i < t; i++) {
        pthread_join(tid[i], NULL);
    }

    printf("Transformed Matrix: \n");
        for (int i=0; i<n*n; i++){
            printf("%f", X[i]);
            if ((i+1) % n == 0) {
                printf("\n");
            } else {
                printf(" ");
            }
        }

    clock_gettime(CLOCK_MONOTONIC, &time_after);
            double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                                (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
    printf("Execution time: %f\n", time_elapsed);

    free(splits);
    free(tid);
}

// GENERATES MATRIX PROPERLY
float* generate_matrix(size_t n){
    float *mat = (float *)malloc(n*n*sizeof(float));
    unsigned int seed = time(0);
        for (int i=0; i<n*n; i++){
            int rd_num = rand_r(&seed) % (100+1);
            mat[i] = rd_num;
        }
    return mat;
}

int main(int argc, char *argv[]){
    if (argc != 4){
        printf("Usage: %s <userN> <userPort> <status>\n", argv[0]);
        return 1;
    }

    size_t userN = (size_t) atoi(argv[1]);
    int status = atoi(argv[3]);

    if (userN <= 0){
        printf("Error: userN must be > 0\n");
        return 1;
    }

    if (status != 0 && status != 1){
        printf("Error: status must be 0 (master) or 1 (slave)\n");
        return 1;
    }

    char userPort[6]; // max 5 digits + null terminator
    strncpy(userPort, argv[2], sizeof(userPort) - 1);
    userPort[5] = '\0'; // ensure null termination

    // printf("\n");

    // max of 16 ip addresses; 15 chars max for ip address + 1 for null terminator
    char ips[16][16]; 
    char ports[16][6];
    size_t userT;
    char masterIP[16];
    char masterPort[6];

    read_config(ips, ports, &userT, masterPort, masterIP);
	printf("DEBUG masterIP: '%s', masterPort: '%s'\n", masterIP, masterPort);
    
    // master instance
    if (status == 0){
        float *mat = generate_matrix(userN);
        // printf("Initial Matrix: \n");
        // for (int i=0; i<userN*userN; i++){
        //     printf("%f", mat[i]);
        
        //     if ((i+1) % userN == 0) {
        //         printf("\n");
        //     } else {
        //         printf(" ");
        //     }
        // }
        if (mat != NULL){
            master(ips, ports, mat, userT, userN, masterPort, masterIP);
        }
    } else if (status == 1){
        slave(userPort, masterIP, masterPort, ips, ports, userN, userT);
    }
}
