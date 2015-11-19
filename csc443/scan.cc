#include <stdio.h>
#include <sstream>
#include <fstream>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <iomanip>
#include <sys/timeb.h>
#include "heapservice.h"

using namespace std;


#define FIXED_SIZE 1000
#define NUM_ARGS 2


int main(int argc , char** argv){
	
	//check valid call to the function.
	//scan <heapfile> <page_size>
	//  if(argc != NUM_ARGS){
	//  	printf("Usage: %s <csv_file> <page_file> <page_size>\n", argv[0]);
	//  	exit(1);
	// }

	char *heap_file_path = argv[1];
	FILE *heap_file;
	heap_file = fopen(heap_file_path,"r");
	int page_size = atoi(argv[2]);

	Heapfile heap;
	Record *new_rec;

	init_heapfile(&heap, page_size, heap_file,true);
	
	RecordIterator *recordsIt = new RecordIterator(&heap);

	while(recordsIt->hasNext()){
		new_rec = recordsIt->next();
		for(int j = 0 ; j < new_rec->size() ; j++){
			
			cout << new_rec->at(j) << " " ;

		}
		printf("\n");
	}
}