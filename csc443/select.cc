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
#define NUM_ARGS 6


int main(int argc , char** argv){
	
	//check valid call to the function.
	//scan <heapfile> <page_size>
	//<heapfile> <attribute_id> <start> <end> <page_size>
	//
	//
	//
	 if(argc != NUM_ARGS){
	 	printf("Usage: %s <heapfile> <attribute_id> <start> <end> <page_size>\n", argv[0]);
	 	exit(1);
	}

	char *heap_file_path = argv[1];
	FILE *heap_file;
	heap_file = fopen(heap_file_path,"r+");
	
	int attribute_id = atoi(argv[2]);
	char *start = argv[3];
	char *end = argv[4];

	const char* cur;

	int page_size = atoi(argv[5]);

	Record *new_rec;
	Heapfile heap;
	init_heapfile(&heap, page_size, heap_file,true);
	RecordIterator *recordsIt = new RecordIterator(&heap);

	while(recordsIt->hasNext()){
		new_rec = recordsIt->next();
		
			cur = new_rec->at(attribute_id);
			/*cout << "strcmp( cur, start )  " << strcmp( cur, start ) << "\n";
			cout << "strcmp( cur, end ) " << strcmp( cur, start ) << "\n";*/

		if(strcmp( cur, start ) >= 0 && strcmp( cur, end ) <= 0 ){
			cout << cur << "\n";
		}
	}
	


	/*while(recordsIt->hasNext()){
		new_rec = recordsIt->next();
		for(int j = 0 ; j < new_rec->size() ; j++){
			printf("%s ", new_rec->at(j));
		}
		printf("\n");
	}*/
	fclose(heap_file);



		return 1;

}