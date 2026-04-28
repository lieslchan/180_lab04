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
    size_t rows;     // how many rows this thread handles
    float* matrix;   // pointer to orig matrix
    int client_sock;
	int core;
} submat;

void read_config(char ips[][16], char ports[][6], size_t* userT, char* masterPort, char* masterIP);
void* send_to_slave(void* arg);
void slave(char* userPort, char* masterIp, char* masterPort, char ips[][16], char ports[][6], size_t userN, size_t userT);
void master(char ips[][16], char ports[][6], float* mat, size_t userT, size_t userN, char* masterPort, char* masterIp);
void createThreads(size_t n, size_t t, float* X, int socket_desc);
float* generate_matrix(size_t n);


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
    char ack[4];
	
    size_t n = mat->n;
    size_t original_start = mat->start_idx;
    size_t start_idx = mat->start_idx;
    size_t rows = mat->rows;
    float* X = mat->matrix;  // points to full matrix

    size_t idx_limit = rows*n + start_idx;

    float *submat = (float *)malloc(n*rows*sizeof(float));

    size_t idx = 0;
    for (start_idx = start_idx; start_idx < idx_limit; start_idx++){
        submat[idx++] = X[start_idx];
    }

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(idx % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
	sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);  

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

    // send starting index
    if(send(mat->client_sock, &original_start, sizeof(size_t), 0) < 0){
        printf("Unable to send starting index to slave\n");
        pthread_exit(NULL);
    }

    if(recv(mat->client_sock, ack, 4 * sizeof(char), 0) < 0){
        printf("Couldn't receive\n");
        pthread_exit(NULL);
    } else {
        printf("Received ack.\n\n");
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
    
    int socket_desc;
    struct sockaddr_in server_addr, self_addr;
    float *submat = (float *)malloc(submatSize * sizeof(float));
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
    
    size_t target = submatSize * sizeof(float);
    size_t total = 0;
    clock_gettime(CLOCK_MONOTONIC, &time_before);
    while (total < target) {
        ssize_t r = recv(socket_desc, (char*)submat + total, target - total, 0);
        if (r <= 0){
            printf("Recv error\n");
            return;
        }
        total += r;
    }

    if(recv(socket_desc, &start_idx, sizeof(size_t), 0) < 0){
        printf("Couldn't receive\n");
        pthread_exit(NULL);
    } 

    printf("Received submatrix (%ld bytes).\n", submatSize * sizeof(float));
    printf("Starting index: %ld\n\n", start_idx);
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

    if(send(socket_desc, "ack", 4, 0) < 0){
        printf("Unable to send ack\n");
        return;
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
    size_t rowCount = n/t;
    size_t remainder = n%t;
    struct timespec time_before, time_after;
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN) - 1;

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
        printf("Slave connected: %s: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));\
        
        splits[i].n = n;
        splits[i].matrix = X;          
        splits[i].start_idx = start_idx;
        splits[i].rows = rowCount + (i == 0 ? remainder : 0);
        start_idx += splits[i].rows*n;
        splits[i].client_sock = client_sock;
		splits[i].core = i % num_cores;

        pthread_create(&tid[i], NULL, send_to_slave, &splits[i]);
    }

    for (size_t i = 0; i < t; i++) {
        pthread_join(tid[i], NULL);
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
    
    // size_t userN = atoi(argv[1]);
    // userPort = argv[2];


    // get user n 
    // printf("Enter value of n for nxn matrix: \n");
    // scanf("%ld", &userN);
    // if (userN <= 0) return 0;
    // printf("\n");

    // // get user port 
    // // TODO: input validation!!!
    // printf("Enter port number: \n");
    // scanf("%s", userPort);
    // printf("\n");

    // // get instance status
    // printf("Enter status: \n");
    // scanf("%d", &status);
    // if (status != 0 && status != 1){
    //     return 0;
    // }
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
