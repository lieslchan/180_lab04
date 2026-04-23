#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

typedef struct minimat {
    size_t n;        // total rows in full matrix 
    size_t start_idx; // which row this thread starts at
    size_t rows;     // how many rows this thread handles
    float* matrix;   // pointer to orig matrix
} submat;

// READS CONFIG CORRECTLY
void read_config(char ips[][16], char ports[][6], size_t* userT, char* masterPort, char* masterIP){
    FILE *fptr;

    fptr = fopen("config.txt", "r");

    if (fptr != NULL){
        fscanf(fptr, "%s %s", masterIP, masterPort);

        // fscanf(fptr, "%ld", userT);
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
    size_t start_idx = mat->start_idx;
    size_t rows = mat->rows;
    float* X = mat->matrix;  // points to full matrix

    size_t idx_limit = rows*n + start_idx;

    float *submat = (float *)malloc(n*rows*sizeof(float));

    size_t idx = 0;
    for (start_idx = start_idx; start_idx < idx_limit; start_idx++){
        submat[idx++] = X[start_idx];
    }

    // send extracted submatrix
    if(send())

}

void master(char ips[][16], char ports[][6], float* mat, size_t userT, size_t userN, char* masterPort, char* masterIp){
    int socket_desc;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size;

    socket_desc = socket(AD_INET, SOCK_STREAM, 0);
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
    for (size_t i=0; i<userT; i++){
        client_size = sizeof(client_addr);
        int client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
        if(client_sock < 0){
            printf("Can't accept\n");
            return;
        }
        printf("Slave connected: %s: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        
    }
    

}

void createThreads(size_t n, size_t t, float* X){
    size_t rowCount = n/t;
    size_t remainder = n%t;

    // array of submatrices
    submat* splits = (submat*)malloc(sizeof(submat) * t);
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t) * t);

    size_t start_idx = 0;

    for (size_t i=0; i<t; i++){
        splits[i].n = n;
        splits[i].matrix = X;          
        splits[i].start_idx = start_idx;
        splits[i].rows = rowCount + (i == 0 ? remainder : 0);
        start_idx += splits[i].rows*n;
    }

    for (size_t i=0; i<t, i++){
        pthread_create(&tid[i], NULL, send_to_slave, &splits[i]);
    }

    for (size_t i = 0; i < t; i++) {
        pthread_join(tid[i], NULL);
    }

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

int main(){
    size_t userN;
    int status;
    char userPort[6]; // max of 5 + 1 for null terminator

    // get user n 
    printf("Enter value of n for nxn matrix: \n");
    scanf("%ld", &userN);
    if (userN <= 0) return 0;
    printf("\n");

    // get user port 
    // TODO: input validation!!!
    printf("Enter port number: \n");
    scanf("%s", userPort);
    printf("\n");

    // get instance status
    printf("Enter status: \n");
    scanf("%d", &status);
    if (status != 0 && status != 1){
        return 0;
    }
    printf("\n");

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
            
        }
    }
}