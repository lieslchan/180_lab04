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

typedef struct socketInfo {
    size_t n;
    size_t t; 
    size_t startCol;
    size_t cols;
    char* port;
    char* ip;
    char* masterPort;
    char* masterIp;
    float* submat;
    size_t id;
    int client_sock;
} socketInfo;

void find_ip(char ips[][16], char ports[][6], char* port, size_t userT, char* ip, size_t* i){
    for (*i=0; *i<userT; *i++){
        if (strcmp(port, ports[*i])==0){
            strcpy(ip, ips[*i]);
            break;
        }
    }
} 

float* generate_matrix(size_t n){
    float *mat = (float *)malloc(n*n * sizeof(float));
    unsigned int seed = time(0);
        // initialize matrix with random numbers generated with seed
        for (int i=0; i<n*n; i++){
            int rd_num = rand_r(&seed) % (100+1);
            mat[i] = rd_num;
        } 
    return mat;
}

float** split_matrix(size_t userN, size_t userT, float* mat) {
    size_t colCount  = userN / userT; 
    size_t remainder = userN % userT;
    float** submatrices = (float**)malloc(sizeof(float*) * userT);
    for (int i=0; i<userT; i++){
        int actualCols = colCount + (i == 0 ? remainder : 0);
        submatrices[i] = (float*)malloc(sizeof(float) * userN * actualCols);
    }

    // store submatrices in array
    size_t startCol = 0;
    for (int i=0; i<userT; i++){
        size_t inner_idx = 0;
        int actualCols = colCount + (i == 0 ? remainder : 0);
        for (int row=0; row<userN; row++){
            for(int col=0; col<actualCols; col++){
                submatrices[i][inner_idx++] = mat[row*userN + (startCol + col)];
            }
            
        }
        startCol += actualCols;
    }

    return submatrices;
}

void read_config(char ips[][16], char ports[][6], size_t* userT, char* masterPort, char* masterIP){
    FILE *fptr;
    // open config file to get ip address
    fptr = fopen("config.txt", "r");
    
    if (fptr != NULL){
        fscanf(fptr, "%s %s", masterIP, masterPort);
        
        fscanf(fptr, "%ld", userT);
        printf("t = %ld\n", *userT);

      for (int i=0; i < *userT; i++){
            fscanf(fptr, "%s %s", ips[i], ports[i]);
      //      printf("IP %d: %s\n", i, ips[i]);
      //      printf("PORT %d: %s\n", i, ports[i]);
        }

        fclose(fptr);
    } else {
        printf("File is empty.");
    }
}

void slave(char* userPort, char* masterIp, char* masterPort, size_t submatSize){
    struct timespec time_before, time_after;
    
    int socket_desc;
    struct sockaddr_in server_addr;
    float *submat = (float *)malloc(submatSize * sizeof(float));
    size_t length; 

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socket_desc < 0){
        printf("Error creating socket\n");
        return;
    }

    // Initialize the server address by the port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(masterPort));
    server_addr.sin_addr.s_addr = inet_addr(masterIp);
    
    bool connected = false;
    int retries = 0;
    while(!connected){
        if(retries == 10){
            printf("Could not connect\n");
            return;
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
    // measure execution time
    clock_gettime(CLOCK_MONOTONIC, &time_before);
    send(socket_desc, userPort, 6, 0);

    if(recv(socket_desc, submat, submatSize * sizeof(float), 0) < 0){
        printf("Couldn't receive\n");
        return; 
    }

    if(recv(socket_desc, length, sizeof(size_t), 0) < 0){
        printf("Couldn't receive\n");
        return; 
    }

    char* colRange[length];

    if(recv(socket_desc, colRange, length, 0) < 0){
        printf("Couldn't receive\n");
        return; 
    }
    
    printf("Received submatrix (%ld bytes).\n", submatSize * sizeof(float));
    printf("Received columns: %s\n", colRange);

    if(send(socket_desc, "ack", 4, 0) < 0){
        printf("Unable to send ack\n");
        return;
    }
	clock_gettime(CLOCK_MONOTONIC, &time_after);
	double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
		            (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
	    
	printf("Execution time: %f\n", time_elapsed);
    close(socket_desc);
    free(submat);
}

void* send_to_slave(void* arg){
    socketInfo* sock = (socketInfo*) arg;
    char ack[4];

    size_t submatSize;
    submatSize = sock->id==0 
        ? sock->n * (sock->n/sock->t)+(sock->n % sock->t) 
        : sock->n * (sock->n/sock->t);

    // send submat 
    if(send(sock->client_sock, sock->submat, submatSize * sizeof(float), 0) < 0){
        printf("Unable to send to slave %zu\n", sock->id);
        return NULL;
    }

    int needed = snprintf(NULL, 0, "%ld - %ld",
                      sock->startCol,
                      sock->startCol + sock->cols);

    char *colRange = malloc(needed + 1); // +1 for null terminator

    snprintf(colRange, needed + 1, "%ld - %ld",
            sock->startCol,
            sock->startCol + sock->cols);
    
    // send colRange
    size_t len = sizeof(colRange) + 1
    if(send(sock->client_sock, &len, sizeof(len), 0) < 0){
        printf("Unable to send to slave %zu\n", sock->id);
        return NULL;
    }
    if(send(sock->client_sock, colRange, len, 0) < 0){
        printf("Unable to send to slave %zu\n", sock->id);
        return NULL;
    }

    printf("Sent submatrix to slave %zu (%ld bytes sent)\n", sock->id, submatSize * sizeof(float));

    if(recv(sock->client_sock, ack, 4, 0) < 0){
        printf("Error receiving ack from slave %zu\n", sock->id);
        return NULL;
    }

    if(strcmp(ack, "ack") == 0) printf("Slave %zu acknowledged\n", sock->id);

    close(sock->client_sock);
    return NULL;
}

void master(char ips[][16], char ports[][6], float** submatrices, size_t userT, size_t userN, char* masterPort, char* masterIp){
    struct timespec time_before, time_after;
    
    int socket_desc;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_size;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_desc < 0){
        printf("Error creating socket\n"); return; 
    }

    setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(masterPort));
    server_addr.sin_addr.s_addr = inet_addr(masterIp);
    
    printf("MasterIP: %s\n", masterIp);

    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
        printf("Couldn't bind\n"); 
        return;
    }
	
	
    if(listen(socket_desc, userT) < 0){
        printf("Error listening\n");
        return;
    }
    

    printf("Master listening on %s: %s\n", masterIp, masterPort);
    
    socketInfo* splits = (socketInfo*)malloc(sizeof (socketInfo) * userT);
    pthread_t* tid = (pthread_t*)malloc(sizeof(pthread_t) * userT);
    
    clock_gettime(CLOCK_MONOTONIC, &time_before);
    size_t colCount  = userN / userT;
    size_t startCol = 0;
    for (size_t i=0; i<userT; i++){
        client_size = sizeof(client_addr);
        int client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, &client_size);
        if(client_sock < 0){
            printf("Can't accept\n"); 
            return; 
        }
        printf("Slave connected: %s: %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	
	
        char slavePort[6];
        recv(client_sock, slavePort, 6, 0);
        
	
        size_t idx = 0;
        for(size_t j=0; j < userT; j++){
            if(strcmp(slavePort, ports[j]) == 0){
                idx = j;
                break;
            }
        }
        
        splits[idx].n = userN;
        splits[idx].t = userT;
        splits[idx].submat = submatrices[idx];
        splits[idx].ip = ips[idx];
        splits[idx].port = ports[idx];
        splits[idx].masterIp = masterIp;
        splits[idx].masterPort = masterPort;
        splits[i].cols = colCount + (i == 0 ? remainder : 0);   // add remainder to first split only 
        startCol += splits[i].cols;
        splits[idx].id = idx;
        splits[idx].client_sock = client_sock;

        pthread_create(&tid[idx], NULL, send_to_slave, &splits[idx]);
    }

    for (size_t i = 0; i < userT; i++) {
        pthread_join(tid[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &time_after);
            double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                                (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
    
    printf("Execution time: %f\n", time_elapsed);
    close(socket_desc);
    free(splits);
    free(tid);
}


// main function
int main(){

    size_t userN;
    int status;
    //size_t userT;
    char userPort[6];

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

    char ips[17][16];
    char ports[17][6];
    size_t userT;
    char masterIP[16];
    char masterPort[6];

    read_config(ips, ports, &userT, masterPort, masterIP);
	// printf("DEBUG masterIP: '%s', masterPort: '%s'\n", masterIP, masterPort);

    if (status == 0){
        float *mat = generate_matrix(userN);
        if (mat != NULL) {
            float** submatrices = split_matrix(userN, userT, mat);
            
            master(ips, ports, submatrices, userT, userN, masterPort, masterIP);
                
            free(mat);
            for (int i=0; i<userT; i++){
                free(submatrices[i]);
            }

            free(submatrices);
        } 
    } else if (status == 1) {
        char ip[16];
        size_t i;
        find_ip(ips, ports, userPort, userT, ip, &i);
        size_t submatSize;
        submatSize = i==0 ? userN * (userN/userT)+(userN % userT) : userN * (userN/userT);
        
        // printf("submat size: %ld\n", submatSize);
        // printf("userN: %ld\n", userN);
        // printf("userT: %ld\n", userT);
        // printf("i: %ld\n", i);
        
        slave(userPort, masterIP, masterPort, submatSize);
    }
}

// sources:
// https://www.geeksforgeeks.org/c/generating-random-number-range-c/
// https://www.geeksforgeeks.org/c/measure-execution-time-with-high-precision-in-c-c/
// https://www.geeksforgeeks.org/c/multithreading-in-c/
// https://girishjoshi.io/post/glibc-pthread-cpu-affinity-linux/s
