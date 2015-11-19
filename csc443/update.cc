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
	// 
	// <heapfile> <record_id> <attribute_id> <new_value> <page_size>

	char *heap_file_path = argv[1];
	FILE *heap_file = fopen(heap_file_path, "r+");

	char *record_id = argv[2];
	string record_page_id_str(record_id);
	cout << record_id << "\n";
	int comma_index = record_page_id_str.find(",");

	string pid_str = record_page_id_str.substr(0,comma_index);
	string record_id_str = record_page_id_str.substr(comma_index+1, record_page_id_str.length() - comma_index);
	
	int pageID = atoi(pid_str.c_str());
	int recordId = atoi(record_id_str.c_str());
	pageID++;
	int attr_id = atoi(argv[3]);
	char *new_attr = argv[4];

	int page_size = atoi(argv[5]);;

	cout << pageID << "," << recordId << attr_id << ","  << new_attr << "\n";

	if(!heap_file){
		printf("Cannot open page_file, please give valid page_file.\n");
	}

	Heapfile heap;
	Page selected;
	init_heapfile(&heap, page_size, heap_file,true);
	init_fixed_len_page(&selected, page_size, FIXED_SIZE);

	Record cur_record(SCHEMA_NUM_ATTR);
	
	for(int z = 0; z < SCHEMA_NUM_ATTR; z++){
		cur_record.at(z) =  new char[SCHEMA_ATTR_LEN];
	}

	read_page(&heap, pageID, &selected);

	if(recordId > fixed_len_page_capacity(&selected)){
		printf("no record to update\n");
		exit(1);
	}

	if(read_fixed_len_page(&selected, recordId, &cur_record)){
		cur_record.at(attr_id) =  new_attr;
		write_fixed_len_page(&selected, recordId, &cur_record);
		write_page(&selected, &heap, pageID);
	}else{
		printf("no record to update\n");
	}

	
	
	fflush(heap_file);

	fclose(heap_file);

}