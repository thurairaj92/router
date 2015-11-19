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


int main(){
	
	//check valid call to the function.
	//scan <heapfile> <page_size>
	//<heapfile> <attribute_id> <start> <end> <page_size>
	//
	//
	//
	
	 /*if(argc != NUM_ARGS){
	 	printf("Usage: %s <heapfile> <attribute_id> <start> <end> <page_size>\n", argv[0]);
	 	exit(1);
	}*/

	/*char *heap_file_path = argv[1];
	FILE *heap_file;
	heap_file = fopen(heap_file_path,"r+");*/
	
	/*cout << " " << argv[1] << " " << argv[2] << " " << argv[3] << " " << argv[4] << " " << argv[5] << "\n";

*/
	/*int attribute_id = atoi(argv[2]);
	char *start = argv[3];
	char *end = argv[4];

	const char* cur;

	int page_size = atoi(argv[5]);

	Record *new_rec;
	Heapfile heap;
	
	RecordIterator *recordsIt = new RecordIterator(&heap);

	while(recordsIt->hasNext()){
		new_rec = recordsIt->next();
		for(int j = 0 ; j < new_rec->size() ; j++){
			cur = new_rec->at(j);
			if(strcmp( cur, start ) >= 0 && strcmp( cur, end ) <= 0 ){
				cout << cur << "\n";
			}
		}
	}*/
	/*fclose(heap_file);
*/


		return 1;

}