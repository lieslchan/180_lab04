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

void find_ip(char ips[][16], char ports[][6], char* port, size_t userT, char* ip, size_t* i){
    for (*i=1; *i<userT; *i++){
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

void read_config(char ips[][16], char ports[][6], size_t* userT){
    FILE *fptr;
    // open config file to get ip address
    fptr = fopen("config.txt", "r");
    if (fptr != NULL){
        fscanf(fptr, "%ld", userT);

        for (int i=0; i < *userT; i++){
            fscanf(fptr, "%s %s", ips[i], ports[i]);
        }

        fclose(fptr);
    } else {
        printf("File is empty.");
    }
}

void slave(char* ip, char* port, size_t submatSize){
    int socket_desc, client_sock, client_size;
    struct sockaddr_in server_addr, client_addr;
    float *submat = (float *)malloc(submatSize * sizeof(float));

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    
    if(socket_desc < 0){
        printf("Error while creating socket\n");
        return;
    }
    printf("Socket created successfully\n");
    
    // Initialize the server address by the port and IP:
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = inet_addr(ip);
    
    // Bind the socket descriptor to the server address (the port and IP):
    if(bind(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr))<0){
        printf("Couldn't bind to the port\n");
        return;
    }
    printf("Done with binding\n");

    // Turn on the socket to listen for incoming connections:
    if(listen(socket_desc, 1) < 0){
        printf("Error while listening\n");
        return;
    }
    printf("\nListening for incoming connections.....\n");
    
    /* Store the client’s address and socket descriptor by accepting an
    incoming connection. The server-side code stops and waits at accept()
    until a client calls connect().
    */
    client_size = sizeof(client_addr);
    client_sock = accept(socket_desc, (struct sockaddr*)&client_addr, (socklen_t*)&client_size);
    
    if (client_sock < 0){
        printf("Can't accept\n");
        return;
    }
    printf("Client connected at IP: %s and port: %i\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // Receive client's message:
    if (recv(client_sock, submat, submatSize*sizeof(float), 0) < 0){
        printf("Couldn't receive\n");
        return;
    } else {
        printf("Received Matrix: \n");
        for (int i=0; i<submatSize; i++){
            printf("%f", submat[i]);
            if (i == 3) {
                printf("\n");
            } else {
                printf(" ");
            }
        }
    }

    if(send(client_sock, "ack", 4 * sizeof(char), 0) < 0){
        printf("Unable to send message\n");
        return;
    }

    close(client_sock);
    close(socket_desc);
}

void master(char ips[][16], char ports[][6], float** submatrices, size_t userT, size_t userN){
    int socket_desc;
    struct sockaddr_in server_addr;
    char ack[4];
    
    for (int i=0; i<userT; i++){
        // Create socket:
        socket_desc = socket(AF_INET, SOCK_STREAM, 0);
        
        if(socket_desc < 0){
            printf("Unable to create socket\n");
            return;
        }
        printf("Socket created successfully\n");
        
        // Set port and IP the same as server-side:
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(ports[i+1]));
        server_addr.sin_addr.s_addr = inet_addr(ips[i+1]);
        
        bool connected = false;
        int retries = 0;
        while (connected == false){
            if (retries == 10){
                return;
            }
            if(connect(socket_desc, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
                printf("Unable to connect. Trying again...\n");
                sleep(10);
                continue;
                retries++;
            } else {
                connected = true;
            }
        }

        printf("Connected with server successfully\n");

        size_t submatSize;
        submatSize = i==0 ? userN * (userN/userT)+(userN % userT) : userN * (userN/userT);
    
        // Send the message to server:
        if(send(socket_desc, submatrices[i], submatSize * sizeof(float), 0) < 0){
            printf("Unable to send message\n");
            return;
        } else {
            printf("Sent Matrix: \n");
                for (int j=0; j<submatSize; j++){
                    printf("%f", submatrices[i][j]);
                    if (i == 3) {
                        printf("\n");
                    } else {
                        printf(" ");
                    }
                }
        }

        // Receive the server's response:
        if(recv(socket_desc, ack, 4*sizeof(char), 0) < 0){
            printf("Error while receiving server's msg\n");
            return;
        }

        if (strcmp(ack, "ack") == 0){
            printf("Slave %d acknowledged\n", i);
        }

        // Close the socket:
        close(socket_desc);
    }
}

// main function
int main(){
    struct timespec time_before, time_after;
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
    printf("Enter status");
    scanf("%d", &status);
    if (status != 0 && status != 1){
        return 0;
    }
    printf("\n");

    char ips[17][16];
    char ports[17][6];
    size_t userT;

    read_config(ips, ports, &userT);

    if (status == 0){
        float *mat = generate_matrix(userN);
        if (mat != NULL) {
            float** submatrices = split_matrix(userN, userT, mat);
            
            // measure execution time
            clock_gettime(CLOCK_MONOTONIC, &time_before);
            master(ips, ports, submatrices, userT, userN);
            clock_gettime(CLOCK_MONOTONIC, &time_after);
            double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                                (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
                
            free(mat);
            for (int i=0; i<userT; i++){
                free(submatrices[i]);
            }

            free(submatrices);

            printf("Execution time: %f\n", time_elapsed);
        } 
    } else if (status == 1) {
        char ip[16];
        size_t i;
        find_ip(ips, ports, userPort, userT, ip, &i);
        size_t submatSize;
        submatSize = i==1 ? userN * (userN/userT)+(userN % userT) : userN * (userN/userT);
        
        clock_gettime(CLOCK_MONOTONIC, &time_before);
        slave(ip, userPort, submatSize);
        clock_gettime(CLOCK_MONOTONIC, &time_after);
        double time_elapsed = (time_after.tv_sec - time_before.tv_sec) + 
                            (time_after.tv_nsec - time_before.tv_nsec) / 1e9;
            
        printf("Execution time: %f\n", time_elapsed);
    }
}

// sources:
// https://www.geeksforgeeks.org/c/generating-random-number-range-c/
// https://www.geeksforgeeks.org/c/measure-execution-time-with-high-precision-in-c-c/
// https://www.geeksforgeeks.org/c/multithreading-in-c/
// https://girishjoshi.io/post/glibc-pthread-cpu-affinity-linux/s
