#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

#define D 1

#define PRINT(msg) if(D) printf("[line %d] ", __LINE__); printf(msg)

#define LOCK(mutex) errno = pthread_mutex_lock(mutex);\
    if(errno){ \
        fprintf(stderr, "[%d] ", __LINE__); \
        perror("locking error"); \
        abort(); \
    } \

#define UNLOCK(mutex) errno = pthread_mutex_unlock(mutex); \
    if(errno){ \
        fprintf(stderr, "[%d] ", __LINE__); \
        perror("unlocking error"); \
        abort(); \
    } \

#define INIT_LOCK(mutex) errno = pthread_mutex_init(mutex, NULL); \
    if(errno){ \
        fprintf(stderr, "[%d] ", __LINE__); \
        perror("init lock error"); \
        abort(); \
    } \

#define DESTROY_LOCK(mutex) errno = pthread_mutex_destroy(mutex);\
    if(errno){ \
        fprintf(stderr, "[%d] ", __LINE__); \
        perror("destroy lock error"); \
        abort(); \
    } \

/*
 * How to create a new list:
 * List * myList = NULL;
 * 
 * Initializing a list, no strings will be stored at this point
 * init_list(myList);
 * 
 * inserting a value into a list:
 * insert(&myList, key, value);
 *
 * removing an element from the list:
 * delete(&myList, key, &output_location)
 * 
 * printing the list:
 * printList(myList);
 * 
 * Deallocating a list:
 * destroy_list(&myList);
 */

typedef struct List{
	char* key;
	char* value;
	struct List *next;
    pthread_mutex_t lock;
}List;

void init_list(List** list){
    List* temp;
    temp = malloc(sizeof(List));
    temp->key = NULL;
    temp->value = NULL;
    INIT_LOCK(&temp->lock);
    *list = temp;
}

//threads shouldnt be running when this is called, so not going to mutex it
int destroy_list(List **node){
    List* currentNode = *node;
    List* nextNode;

    while (currentNode != NULL){
        if(currentNode->next != NULL) nextNode = currentNode->next;
        free(currentNode->value);
        free(currentNode->key);
        DESTROY_LOCK(&currentNode->lock);
        free(currentNode);
        currentNode = nextNode;
    }

    *node = NULL;
    return 0;
}

int searchList(List **head_ref, char *key, char **outputLocation){
    List* temp = *head_ref;
    List*head = *head_ref;

    if (temp == NULL || key == NULL || temp->key == NULL){
        return 1;
    }
    
    LOCK(&head->lock);

    while (temp != NULL){

 
        if (strcmp(temp->key, key) == 0){
            *outputLocation = malloc(1 + strlen(temp->value));
            strcpy(*outputLocation, temp->value);
            UNLOCK(&head->lock);
            return 0;
        }

        temp = temp->next;
    }
    
    UNLOCK(&head->lock);
    return 1;
}

void printList(List *list){
    List *temp = list;
    printf("\n\nkey: value\n");
    while (temp != NULL){
        printf("%s: %s\n", temp->key, temp->value);
        temp = temp->next;
    }
}

int insert(List **head_ref, char* key, char* value){

    List* temp = *head_ref;
    LOCK(&temp->lock);

    //check to see if the list is empty
    if ((temp->value == NULL) && (temp->key == NULL)){
        //reassign the list to an initialized node
        temp->key = malloc(strlen(key) + 1);
        strcpy(temp->key, key);
        temp->value = malloc(strlen(value) + 1);
        strcpy(temp->value, value);
        temp->next = NULL;
        //lock already set up from init
        UNLOCK(&temp->lock);
        return 0;
    } 

    int searching = 1;
    List *last_node = *head_ref;

	while (searching){

        //to avoid relocking for the first element
        if(last_node != *head_ref){
            LOCK(&last_node->lock);
        }

        if (strcmp(last_node->key, key) == 0){
            //the key alread exists, so were just gonna reassign its value
            char *pointer = realloc(last_node->value, (strlen(value) + 1));
            last_node->value = pointer;
            strcpy(last_node->value, value);
            if(last_node != *head_ref){
                UNLOCK(&last_node->lock);
            }
            UNLOCK(&temp->lock);
            return 0;

        } else if(strcmp(last_node->key, key) < 0) {
            //last_node->key comes before the new key
            //check how the next node compares

            if(last_node->next == NULL){
                //we hit the end, so just insert a new node and add the string pair
                List *new_node = malloc(sizeof(List));
                new_node->key = malloc(strlen(key) + 1);
                strcpy(new_node->key, key);
                new_node->value = malloc(strlen(value) + 1);
                strcpy(new_node->value, value);
                new_node->next = NULL;
                INIT_LOCK(&new_node->lock);
                last_node->next = new_node;
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                UNLOCK(&temp->lock);
                return 0;

            } else if(strcmp(last_node->next->key, key) > 0){
                //the next key comes after the new key
                //so we need to insert a new node between the current and next node

                List *new_node = malloc(sizeof(List));
                new_node->key = malloc(strlen(key) + 1);
                strcpy(new_node->key, key);
                new_node->value = malloc(strlen(value) + 1);
                strcpy(new_node->value, value);
                INIT_LOCK(&new_node->lock);
                new_node->next = last_node->next;
                last_node->next = new_node;
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                UNLOCK(&temp->lock);
                return 0;

            } else {
                //reassign the node
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                last_node = last_node->next;
            }

        } else {

            if(last_node == *head_ref){
                //then insert a node before the head
                List *new_node = malloc(sizeof(List));
                new_node->key = malloc(strlen(key) + 1);
                strcpy(new_node->key, key);
                new_node->value = malloc(strlen(value) + 1);
                strcpy(new_node->value, value);
                INIT_LOCK(&new_node->lock);
                new_node->next = *head_ref;
                *head_ref = new_node;
                UNLOCK(&last_node->lock);
                return 0;

            } else if(strcmp(last_node->next->key, key) > 0){
                //the next key comes after the new key
                //so we need to insert a new node between the current and next node
                List *new_node = malloc(sizeof(List));
                new_node->key = malloc(strlen(key) + 1);
                strcpy(new_node->key, key);
                new_node->value = malloc(strlen(value) + 1);
                strcpy(new_node->value, value);
                INIT_LOCK(&new_node->lock);
                new_node->next = last_node->next;
                last_node->next = new_node;
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                UNLOCK(&temp->lock);
                return 0;

            //see if the next node is null
            } else if(last_node->next == NULL){
                //we hit the end, so just insert a new node and add the string pair
                List *new_node = malloc(sizeof(List));
                new_node->key = malloc(strlen(key) + 1);
                strcpy(new_node->key, key);
                new_node->value = malloc(strlen(value) + 1);
                strcpy(new_node->value, value);
                INIT_LOCK(&new_node->lock);
                new_node->next = NULL;
                last_node->next = new_node;
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                UNLOCK(&temp->lock);
                return 0;
            
            } else {

                //reassign the node
                if(last_node != *head_ref){
                    UNLOCK(&last_node->lock);
                }
                last_node = last_node->next;
                
            }    

        }

    } //end the while loop

    UNLOCK(&temp->lock);
    return 1;

}

/**
 *
 * precondition: output location should be uninitialized
 *
 * return values: 
 * 0 for successful deletion
 * 1 for failure: if the  list is empty/null, or we couldn't find the requested key
 * -1 for unanticipated failure
 */
int delete(List** list, char* key, char** output){

    List* head = *list;
    List* current = *list;

	if (head == NULL || ((head->value == NULL) && (head->key == NULL))){
        //list is empty or uninitialized
        return 1;
    } 

    LOCK(&head->lock);

	//see if the first node is the one were looking for
    if (strcmp(current->key, key) == 0){
        *output = malloc(1 + strlen(current->value));
        strcpy(*output, current->value);
        free(current->key);
        current->key = NULL;
        free(current->value);
        current->value = NULL;
        if(current->next != NULL) *list = current->next;	//reassign the head of the list to the 2nd node
        UNLOCK(&current->lock);
        //DESTROY_LOCK(&current->lock);
        //free(current);
        return 0;
    }
    	
    int searching = 1;

	//check the rest of the list
    while (searching){

        if(current->next == NULL) {
            //found the end of the list before finding the key
            *output = NULL;
            UNLOCK(&head->lock);
            return 1;

        } else if (strcmp(current->next->key, key) == 0){

            //make sure that the pointers dont refer to the same node
            if(current != head) LOCK(&current->lock);
            
            //see if the desired key is between two nodes or is the last node
            if(current->next->next == NULL){
                //desired key is in the last node
                *output = malloc(1 + strlen(current->next->value));
                strcpy(*output, current->next->value);
                free(current->next->value);
                free(current->next->key);
                if(current != head){
                    UNLOCK(&current->lock);
                    DESTROY_LOCK(&current->lock);
                }
                free(current->next);
                current->next = NULL;
                UNLOCK(&head->lock);
                return 0;
                
            } else {
                //key is between two nodes

                //create a node pointer for the node to delete
                List* targetNode = current->next;

                *output = malloc(1 + strlen(current->next->value));
                strcpy(*output, current->next->value);
                free(current->next->value);
                free(current->next->key);

                current->next = current->next->next; //redefine the next node

                UNLOCK(&current->lock);
                DESTROY_LOCK(&current->next->lock);

                free(targetNode);

                UNLOCK(&head->lock);
                return 0;

            }
        
        } else {
            //move onto the next node
            current = current->next;
        }

    }
    
    UNLOCK(&head->lock);
    return -1;

}
