#include <stdlib.h>
#include <stdio.h>

//since the null terminator, '\0' doesnt show up when printed, this macro 
//will be used to switch between the null terminator and '0', which I can see when printed.
#define NULL_TERMINATOR '\0'

/*
strbuf.c notes:

- The null terminator will always be located at L->data[L->used]: the position 
	right after the last elements of the "used" portion of the array.
- I am not considering the null byte to be apart of the "used" part of the array

*/

typedef struct {
    size_t length;
    size_t used;
    char *data;
} strbuf_t;

//initialize the strbuf
int strbuf_init(strbuf_t *L, size_t length)
{
	L->data = malloc(sizeof(char) * (length));
	if (!L->data) return 1;

	L->length = length;
	L->used   = 0;

	//make the first and only value of the array the null terminator
	L->data[L->used] = NULL_TERMINATOR;

	return 0;
}

void strbuf_destroy(strbuf_t *L)
{
    free(L->data);
}

int strbuf_append(strbuf_t *L, char item)
{
	//using (used + 1) here to account for the presence of the null terminator
	if ((L->used + 1) == L->length) {
		//double the array length
		size_t size = L->length * 2;
		char *p = realloc(L->data, sizeof(char) * size);
		if (!p) return 1;

		L->data = p;
		L->length = size;

	}

	//append the item to the list
	L->data[L->used] = item;
	
	//tack on the null terminator
	L->data[L->used + 1] = NULL_TERMINATOR;

	//increment used
	++L->used;

	return 0;
}


//remove the last element from the used portion of the list
//move the null terminator accordingly.
int strbuf_remove(strbuf_t *L, char *item)
{
	if (L->used == 0) return 1;

	//assign the last value to the address provided by "item"
	if (item) *item = L->data[L->used - 1];

	//overwrite the last character by moving the null terminator back a postiion
	L->data[L->used - 1] = NULL_TERMINATOR;

	//decrement used, now that the element has been removed
	--L->used;

	return 0;
}

int strbuf_clear(strbuf_t *list){
	//clear the strbuf
	list->used = 0;
	list->data[0] = '\0';
}

int strbuf_insert(strbuf_t *list, int index, char item)
{
	//check to see if the array list needs to be resized.
	//resize if the list isnt big enough or if the position
	//before the null terminator is being used.
	if(index >= (list->length - 1) || ((list->used + 1) == list->length)){
		
		//declare a throwaway pointer
		char *junkPointer;

		//check to see if the size should be doubled
		if(index < (2 * list->length)){
			//double the length
			junkPointer = realloc(list->data, (sizeof(char) * 2 * list->length));
			if (!junkPointer) return 1;

			list->data = junkPointer;
			list->length = (2 * list->length);

		//check to see if the new index should be used rather than doubling
		} else if(index >= (2 * list->length)){
			//resize the array to account for the index and the null byte
			junkPointer = realloc(list->data, (sizeof(char) * (index + 2)));
			if (!junkPointer) return 1;

			list->data = junkPointer;
			//used (index + 2) here because if the index is 5,
			//then index 6 is holds the null byte. indices 0 through
			//6 are apart of the array, therefore the 
			//length (number of array elements) is 7
			list->length = index + 2;

		} 

	}


	//now we can assume that the array is big enough to handle a new element.
	//if needed, step through the array moving elements to the right one position.
	for(int i = (list->used + 1); i > index; i--){
		//printf("Rearranging elements: i=%d > index=%d\n", i, index);
		list->data[i] = list->data[i - 1];
	}

	//now insert the value into the specificed index
	list->data[index] = item;
	
	//update "used" so that it is one greater that the greatest meaningful index
	if(list->used <= index){
		list->used = (index + 1);
	} else {
		list->used++;
	}

	//add in the new null terminator
	list->data[list->used] = NULL_TERMINATOR;

	
	return 0;
}

//step through the provided str, appending chars to the string buffer.
//stop looping when the string's null terminator is found
int sb_concat(strbuf_t *sb, char *str){
	
	int booleanFlag = 1;	//set the flag to true
	int index = 0;		//index for the current character we're looking at

	//loop while the flag is set to true
	while(booleanFlag){

		//check the current character
		if(str[index] == '\0'){

			//we hit the null terminator, so flip the flag
			booleanFlag = 0;	

		} else {

			//append the current character and increment the index
			strbuf_append(sb, str[index]);
			index++;

		}

	}

	return 0;

}
