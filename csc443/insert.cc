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
#define NUM_ARGS 4


int main(int argc , char** argv){
	
	//check valid call to the function.
	// if(argc != NUM_ARGS){
	// 	printf("Usage: %s <csv_file> <page_file> <page_size>\n", argv[0]);
	// 	exit(1);
	// }

	char *heap_file_path = argv[1];
	FILE *heap_file = fopen(heap_file_path, "r+");
	int page_size = 5000;

	ifstream csvfile;
	csvfile.open(argv[2]);

	if(!csvfile){
		printf("Cannot open csv_file, please give valid csv_file.\n");
	}

	if(!heap_file){
		printf("Cannot open page_file, please give valid page_file.\n");
	}

	Record cur_record;
	Record dummy;
	string line;
	char *temp;
	Page page;
	Heapfile heap;

	bool recordWrite = false;

	int record_count = 0;
	struct timeb stimer;
	long begin, end;
	int64_t offset;
	init_heapfile(&heap, page_size, heap_file,true);
	init_fixed_len_page(&page, page_size, FIXED_SIZE);

	PageID cur_page_id = alloc_page(&heap);

	//Get each line of csv
	while(getline(csvfile, line)){
		//Empty the record for fresh row
		cur_record.clear();

		temp = (char *)malloc(line.length() + 1);
		strcpy(temp, line.c_str());

		char *token = strtok(temp, ",");

		//Get each attr of the table
		while (token != NULL) {
		 	char col[11];
		 	strcpy(col,token);
		 	cur_record.push_back((V)col);
		 	token = strtok(NULL, ",");
    	}

    	MasterDirectoryIterator *master_iterator = new MasterDirectoryIterator(&heap);
    	while(master_iterator->hasNext()){

    		Page *master_dir = master_iterator->next();

    		DirectoryEntryIterator *dir_iterator = new DirectoryEntryIterator(&heap,master_dir);
    		while(dir_iterator->hasNext()){
    			
    			Page *cur_page = dir_iterator->next(&offset);

    			for(int i = 0; i < fixed_len_page_capacity(cur_page); i++){
    				if(!read_fixed_len_page(cur_page, i, &dummy)){
    					write_fixed_len_page(cur_page, i, &cur_record);
    					recordWrite = true;
    					fseeko(heap_file, offset, SEEK_SET);
    					fwrite(cur_page->data, page_size, 1 , heap_file);
    					printf("inserted!\n");
    					break;
    				}
    			}

    			if(recordWrite){
    				break;
    			}

    		} 

    		if(recordWrite){
    			break;
    		}

    	}
    	recordWrite = false;
    	//Try adding record to the current page
    	
	}


	

	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	
	fflush(heap_file);

	fclose(heap_file);
	csvfile.close();

}