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
/*
	char *heap_file_path = argv[2];
	FILE *heap_file = fopen(heap_file_path, "w");*/
	int page_size = 5000;

	ifstream csvfile;
	csvfile.open(argv[1]);

	if(!csvfile){
		printf("Cannot open csv_file, please give valid csv_file.\n");
	}
/*
	if(!heap_file){
		printf("Cannot open page_file, please give valid page_file.\n");
	}*/

	Record cur_record;
	string line;
	char *temp;
	Page page;
	Heapfile heap;

	int record_count = 0;
	struct timeb stimer;
	long begin, end;
	FILE *heap_file;
	heap_file = fopen("new_file","w+");


	init_heapfile(&heap, page_size, heap_file,false);
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


    	
    	//Try adding record to the current page
    	if(add_fixed_len_page(&page, &cur_record) < 0){
    		//Failed to add to record to current page, hence create new pagefile in heap
    	 	write_page(&page , &heap, cur_page_id);
    	 	cur_page_id = alloc_page(&heap);
    	 	memset(page.data,'\0',page_size);
    	 	printf("Page to write %s\n", ((char *)(page.data)));
    	 	add_fixed_len_page(&page, &cur_record);
    	}

		record_count++;
	}


	if (fixed_len_page_freeslots(&page) < fixed_len_page_capacity(&page)) {
		write_page(&page , &heap, cur_page_id);
		printf("Page to write %s\n", ((char *)(page.data)));
    	cur_record.clear();
	}

	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	
	/*Page *dir = create_heap_page(&heap);
	DirectoryEntry *testEntry;
	Page *rec = create_heap_page(&heap);

	fseeko(heap_file,INITIAL_OFFSET ,SEEK_SET);
	fread(dir->data, heap.page_size, 1, heap_file);

	void *testBuf = dir->data;
	testBuf += sizeof(MasterDirectoryEntry);
	testEntry = (DirectoryEntry *)testBuf;

	fseeko(heap_file,testEntry->offset ,SEEK_SET);
	fread(rec->data, heap.page_size, 1, heap_file);

	Record testRec(SCHEMA_NUM_ATTR);
	read_fixed_len_page(rec, 0, &testRec);

	cout << testRec.at(0);*/

	fflush(heap_file);

	fclose(heap_file);

	csvfile.close();


	/*double second = double(end - begin)/1000.0 ;
	double rate = (double)record_count / second;
	cout<< "NUMBER OF RECORDS: " << record_count << '\n';
	cout << "TIME :" << end - begin << " MILLISECONDS \n";
	cout << "RATE :" << fixed << setprecision( 6 ) << rate;*/
}