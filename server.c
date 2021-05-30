#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>

#include "strbuf.c"
#include "linkedlist.c"

#define BACKLOG 5
#define DEBUG 1
#define PRINT_D(msg) if(DEBUG) { printf("[line %d] ", __LINE__); printf(msg); }

// the argument we will pass to the connection-handler threads
struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    List *myList;
    int fd;
};


/**
 * Post condition: the last char read from the socket will be
 * a newline char, so we need to call getc before proceeding in
 * processKeyLength.
 * 
 * Returns
 * 0 for success
 * 1 for failure due to a malformed message: send BAD to client
 * -1 if we recieved EOF
 */ 
int processRequestType(FILE* incoming, int* mode){

    strbuf_t charStorage;
    strbuf_init(&charStorage, 1);
    char currentChar = getc(incoming);

    int finished = 0;

    while(currentChar != EOF && (!finished)){

        PRINT_D("in the loop\n");

        //If value == "\n", and mode hasn't been set (read first four chars).
        if (currentChar == 10){
            if (charStorage.data[0] == 'G' && charStorage.data[1] == 'E' && charStorage.data[2] == 'T' && charStorage.used == 3){
                *mode = 1;
                printf("got request type: %s\n", charStorage.data);
                strbuf_clear(&charStorage);
                finished = 1;
            } else if (charStorage.data[0] == 'S' && charStorage.data[1] == 'E' && charStorage.data[2] == 'T' && charStorage.used == 3){
                *mode = 2;
                printf("got request type: %s\n", charStorage.data);
                strbuf_clear(&charStorage);
                finished = 1;
            } else if (charStorage.data[0] == 'D' && charStorage.data[1] == 'E' && charStorage.data[2] == 'L' && charStorage.used == 3){
                *mode = 3;
                printf("got request type: %s\n", charStorage.data);
                strbuf_clear(&charStorage);
                finished = 1;
            } else {
                strbuf_clear(&charStorage);
                return 1;
            }
        } else {
            strbuf_append(&charStorage, currentChar);
            currentChar = getc(incoming);
        }

    }

    //clean up
    strbuf_destroy(&charStorage);

    if(finished){
        return 0;
    }

    PRINT_D("found a eof, returning -1");

    //if we reached this point, then the connection was closed
    return -1;

}

/**
 * Precondition: the last char pulled from the socket was a newline char
 * 
 * Returns
 * 0 for success
 * 1 for failure due to the message length being less than 1
 * -1 if we recieved EOF
 */ 
int processKeyLength(FILE* incoming, int *message_len){

    strbuf_t charStorage;
    strbuf_init(&charStorage, 1);
    char currentChar = getc(incoming);

    int finished = 0;

    while ((currentChar != EOF) && (!finished)){ 
        PRINT_D("in the get/del loop\n");

        //If message length is not set, and you reach a "\n"
        if (currentChar == 10){
            PRINT_D("finished reading in message length, adding to strbuf\n");
            *message_len = atoi(charStorage.data);
            if(*message_len < 1) return 1;
            printf("got key length: %s\n", charStorage.data);
            strbuf_clear(&charStorage);
            finished = 1;
        } else {
            //We append message length characters
            strbuf_append(&charStorage, currentChar);
            currentChar = getc(incoming);
        }
    }

    //clean up
    strbuf_destroy(&charStorage);

    if(finished){
        return 0;
    }

    //if we reached this point, then the connection was closed
    return -1;

}





/**
 * note: we can malloc key on thread funciton side to message length
 * 
 * Returns
 * 0 for success
 * 1 if the provided message length does match the length of the actual message
 * -1 if we recieved EOF
 */ 
int processKey(FILE* incoming, int message_len, char **key, int mode){

    strbuf_t charStorage;
    strbuf_init(&charStorage, 1);
    char currentChar = getc(incoming);

    int count = 0;
    int finished = 0;

    while ((currentChar != EOF) && (!finished)){ 
        if (currentChar == 10){

            //Check if message length is valid
            if (mode == 1 || mode == 3){
                if (message_len != count+1){
                    return 1;
                }
            }

            printf("got key: %s\n", charStorage.data);

            //just return the key, process it on the other side

            PRINT_D("\n");
            *key = malloc(charStorage.used + 1);
            PRINT_D("\n");
            strcpy(*key, charStorage.data);
            PRINT_D("\n");
            finished = 1;
            PRINT_D("\n");
            printf("current char = %d\n", currentChar);
            continue;

        } else {
            //Append key characters
            strbuf_append(&charStorage, currentChar);
            count++;
            currentChar = getc(incoming);
        }

    } 

    PRINT_D("\n");

    //clean up
    strbuf_destroy(&charStorage);

    if(finished){
        PRINT_D("finsihed reading in the key");
        return 0;
    }

    //if we reached this point, then the connection was closed
    return -1;   

}

/**
 * Returns
 * 0 for success
 * 1 for failure due to...
 * -1 if we recieved EOF
 */ 
int processValue(FILE* incoming, int message_len, char **value, char **key){

    strbuf_t charStorage;
    strbuf_init(&charStorage, 1);
    char currentChar = getc(incoming);

    int count = 0;
    int finished = 0;

    while ((currentChar != EOF) && (!finished)){ 
        if (currentChar == 10){

            //Check if message length is valid
            if (message_len != count + strlen(*key) + 2){
                return 1;
            }

            //just return the key, process it on the other side
            printf("got value: %s\n", charStorage.data);

            *value = malloc(charStorage.used + 1);

            strcpy(*value, charStorage.data);
            finished = 1;

        } else {
            //Append key characters
            strbuf_append(&charStorage, currentChar);
            count++;
            currentChar = getc(incoming);
        }

    } 

    //clean up
    strbuf_destroy(&charStorage);

    if(finished){
        return 0;
    }

    //if we reached this point, then the connection was closed
    return -1; 

}

void* threadTask(void* args){

    PRINT_D("In the thread task!\n");

    char host_name[100], port_num[10];
    struct connection *c = (struct connection *) args;
    List *myList = c->myList;
    int error, bytes_read;

	// retrive the name (stored in host_name) and port (stored in port_num) of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host_name, 100, port_num, 10, NI_NUMERICSERV);

    if (error != 0) {
        fprintf(stderr, "[line %d]: getnameinfo failure: %s", __LINE__, gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    //at this point we should be ready to use I/O functions for the socket

    //create a reading and writing file struct
    FILE *incoming = fdopen(dup(c->fd), "r");  // copy socket fd, then open in read mode


    /**
    * mode = 0 by default: should never see this in use
    * mode = 1 for get
    * mode = 2 for set
    * mode = 3 for del
    */
    int mode = 0;

    /** 
    * this variable will be used to indicate which part of the request where currently handling
    * this approach also helps to avoid any unesscary/redundant work
    *
    * when argNumber = 1, were looking for the request type: GET, SET, or DEL
    * when argNumber = 2, were handling the key's length
    * when argNumber = 3, were handling the key itself
    * when argNumber = 4, were handling the value (only applicable for SET)
    */
    int argNumber = 1;

    //variables to store processing data
    int message_len = 0;
    char *key;
    char *value;

    //set this value to zero if EOF is detected in one of the processing functions
    int connectionOpen = 1;

    PRINT_D("starting the while loop\n");

    while(connectionOpen){

        if(argNumber == 1){
            
            int result = processRequestType(incoming, &mode);

            //if invalid, report the error and start the next iteration of the outer loop
            if(result == 0){
                argNumber++;
            } else if(result == 1){
                //the message is malformed, report this to client
                write(c->fd, "ERR\nBAD\n", 8);
                connectionOpen = 0;
                continue;
            } else if(result == -1){
                PRINT_D("found eof\n");
                connectionOpen = 0;
                continue;
            } else {
                write(c->fd, "ERR\nSRV\n", 8);
                abort();
            }           

        } else if(argNumber == 2){

            int result = processKeyLength(incoming, &message_len);
            
            //if invalid, report the error and start the next iteration of the outer loop
            if(result == 0){
                argNumber++;
            } else if(result == 1){
                //the message len was < 1, report this malformed message to the client
                write(c->fd, "ERR\nBAD\n", 8);
                connectionOpen = 0;
                continue;
            } else if(result == -1){
                connectionOpen = 0;
                continue;
            } else {
                write(c->fd, "ERR\nSRV\n", 8);
                abort();
            }  

        } else if(argNumber == 3){

            int result = processKey(incoming, message_len, &key, mode);

            //if invalid, report the error and start the next iteration of the outer loop
            if(result == 0){

                //check the mode: if its 1 or 3, then handle the request here accordingly
                if(mode == 1) {
                    //GET
                    char *outputLocation;
                    int status = searchList(&myList, key, &outputLocation);
                    if (status == 0){
                        char *temp = malloc(12 + (sizeof(int) * strlen(outputLocation)) + strlen(outputLocation) + 1);
                        sprintf(temp, "OKG\n%ld\n%s\n", strlen(outputLocation) + 1, outputLocation);
                        write(c->fd, temp, strlen(temp));
                        free(temp);
                        free(outputLocation);
                    } else {
                        write(c->fd, "KNF\n", 4);
                    }

                    //reset all carry-over variables
                    message_len = 0;
                    key = NULL;
                    value = NULL;
                    argNumber = 1;
                    PRINT_D("finsihed getting a value, moving onto the next instruction\n");
                    continue;

                } else if(mode == 2) {
                    //SET: increment argNumber
                    PRINT_D("got the key, but still need the value\n");
                    argNumber = 4;

                } else if(mode == 3){
                    //DELETE Function
                    char *outputLocation;
                    int status = delete(&myList, key, &outputLocation); //returns value to 'response' variable.
                    if (status == 0){
                        char *temp = malloc(12 + (sizeof(int) * strlen(outputLocation)) + strlen(outputLocation) + 1);
                        sprintf(temp, "OKD\n%ld\n%s\n", strlen(outputLocation) + 1, outputLocation);
                        write(c->fd, temp, strlen(temp));
                        free(temp);
                        free(outputLocation);
                    } else {
                        write(c->fd, "KNF\n", 4);
                    }

                    //reset all carry-over variables
                    message_len = 0;
                    key = NULL;
                    value = NULL;

                    argNumber = 1;
                    PRINT_D("finsihed deleting a value, moving onto the next instruction\n");
                    continue;
                } else {
                    write(c->fd, "ERR\nSRV\n", 8);
                    abort();

                }

                //dont forget to reset all carry-over variables
                //call getc() here? this way were ready to restart the loop
                //reset argNumber

            } else if(result == 1){
                write(c->fd, "ERR\nLEN\n", 8);
                connectionOpen = 0;
                continue;
            } else if(result == -1){
                connectionOpen = 0;
                continue;
            } else {
                write(c->fd, "ERR\nSRV\n", 8);
                abort();
            }  

        } else if(argNumber == 4){

            int result = processValue(incoming, message_len, &value, &key);
            if (result == 1){
                write(c->fd, "ERR\nLEN\n", 8);
                connectionOpen = 0;
                continue;
            } else if(result == -1){
                connectionOpen = 0;
                continue;
            } 

            int status = insert(&myList, key, value); //returns value to 'response' variable.
            if (status == 0){
                write(c->fd, "OKS\n", 4);
                printList(myList);
            } else {
                write(c->fd, "ERR\nKNF\n", 8);
            }

            //reset all carry-over variables
            message_len = 0;
            key = NULL;
            value = NULL;
            
            //reset argNumber
            argNumber = 1;
            PRINT_D("finsihed setting a value, moving onto the next instruction\n");
            continue;

        } else {
            write(c->fd, "ERR\nSRV\n", 8);
            abort();
        }

    }

    //if we reach this point then the client closed the connection and this thread should be terminated
    printf("client closed the connection; thread terminated\n");
    close(c->fd);
    free(c);
    return NULL;

}

int main(int argc, char** argv){

    /* Check for # of arguments */
    if (argc != 2){
        fprintf(stderr, "invalid number of arguments provided\n");
        return -1;
    }

    //todo: store the port number from cmd line. validate data
    char* port = argv[1];

    /* Set up socket attributes and thread functionality */
    struct addrinfo socketSpecs, *info_list, *info;
    struct connection *con;
    int error, socketFD;
    pthread_t tid;


    /* Initialising socketSpecs (setting up correct IP version, abstract attributes, etc */
    memset(&socketSpecs, 0, sizeof(struct addrinfo));   //fill socketSpecs with 0's
    socketSpecs.ai_family = AF_UNSPEC;     //IPv4 or IPv6
    socketSpecs.ai_socktype = SOCK_STREAM; //TCP Server
    socketSpecs.ai_flags = AI_PASSIVE;     //To allow for listening

    PRINT_D("setup complete. now attempting to get address info\n");

    /* Get socket and address info for listening port */
    /* NULL is localhost - allows for a listening socket */
    /* store results in info_list */
    error = getaddrinfo(NULL, port, &socketSpecs, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    PRINT_D("got the address info. now attempting to create a socket with a for loop\n");


    /* Attempt to create socket (for-loop allows for creating a socket with correct attributes) */
    for (info = info_list; info != NULL; info = info->ai_next) {
        
        //attempt to create a socket
        socketFD = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (socketFD == -1) {
            continue;
        }

        /* If successful socket creation: Try to set it up for incoming connections */
        if ((bind(socketFD, info->ai_addr, info->ai_addrlen) == 0) && (listen(socketFD, BACKLOG) == 0)) {
            break;
        }

        // unable to set the socket up, so try the next entry
        close(socketFD);
    }

    PRINT_D("done with the for loop. now checking if we successfully binded a socket\n");


    /* OUTSIDE FOR-LOOP: We reached the end of result without successfuly binding a socket */
    if (info == NULL) {
        fprintf(stderr, "Could not bind\n");
        freeaddrinfo(info_list);
        return -1;
    }

    //create a list, now that the socket has been bound
    List *myList;
    init_list(&myList);

    PRINT_D("we succesfully binded a socket! freeing info_list and entering the infinite loop\n");


    /* If bind was successful, free the struct - proceed with read & write */
    freeaddrinfo(info_list);


    /* socketFD is bound and listening */
    printf("Waiting for connection....\n");

    while (1) {
        // create argument struct for child thread
        con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);
            // addr_len is a read/write parameter to accept
            // we set the initial value, saying how much space is available
            // after the call to accept, this field will contain the actual address length
        
        //add the linked list to the con struct
        con->myList = myList;

        PRINT_D("in while loop: waiting for an incoming connection\n");

        // wait for an incoming connection
        con->fd = accept(socketFD, (struct sockaddr *) &con->addr, &con->addr_len);

        //at this point the connection struct has had its address, address length, and fd filled with data
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) {
            perror("accept");
            continue;
        }

        printf("got a connection! creating a worker thread\n");
        // spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, threadTask, con);

        // if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

        //detach the thread and wait for the next connection request;
        pthread_detach(tid);
        PRINT_D("thread detached\n");

    } //end while loop

    puts("No longer listening.");
    pthread_detach(pthread_self());
    pthread_exit(NULL);

    // never reach here
    return 0;

}
