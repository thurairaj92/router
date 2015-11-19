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
#define SCHEMA_NUM_ATTR 1
#define SCHEMA_ATTR_LEN 10


int main(int argc , char** argv){
	
	cout << SCHEMA_NUM_ATTR;

	//check valid call to the function.
	// if(argc != NUM_ARGS){
	// 	printf("Usage: %s <csv_file> <page_file> <page_size>\n", argv[0]);
	// 	exit(1);
	// }

	/*int page_size = 5000;

	ifstream csvfile;
	csvfile.open(argv[1]);

	if(!csvfile){
		printf("Cannot open csv_file, please give valid csv_file.\n");
	}

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

	FILE *heapfiles[100];
	Page page[100];
	Heap heap[100];

	string filename;

	PageID cur_page_id;
	
	for (int i = 0; i < 100; i++){
		filename = "heaps/" + to_string(i);
	    heapfiles[i] = fopen(filename.c_str(), "w");
		init_heapfile((&heap)[i], page_size, heapfiles[i],false);
		init_fixed_len_page((&page)[i], page_size, FIXED_SIZE);
		cur_page_id = alloc_page((&heap)[i]);
		filename.clear()
	}


	//Get each line of csv
	while(getline(csvfile, line)){
		//Empty the record for fresh row
		cur_record.clear();

		temp = (char *)malloc(line.length() + 1);
		strcpy(temp, line.c_str());

		char *token = strtok(temp, ",");

		int i = 0;
		//Get each attr of the table
		while (token != NULL) {
	    	cur_record.clear();
		 	
		 	char col[11];
		 	strcpy(col,token);
		 	cur_record.push_back((V)col);
		 	token = strtok(NULL, ",");

		 	if(add_fixed_len_page(&(page[i]), &cur_record) < 0){
	    		//Failed to add to record to current page, hence create new pagefile in heap
	    	 	write_page(&(page[i]) , &(heap[i]), cur_page_id);
	    	 	cur_page_id = alloc_page(&(heap[i]));
	    	 	memset(page[i].data,'\0',page_size);
	    	 	add_fixed_len_page(&(page[i]), &cur_record);
	    	}

	    	i++
    	}

		record_count++;
	}

	for (int i = 0; i < 100; ++i)
	{
		if (fixed_len_page_freeslots(&(page[i])) < fixed_len_page_capacity(&(page[i]))) {
			write_page(&(page[i]) ,&(heap[i]), cur_page_id);
			printf("Page to write %s\n", ((char *)(page[i].data)));
		}
	}

	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	

	for (int i = 0; i < 100; i++){
		fflush(heapfiles[i]);
		fclose(heapfiles[i]);
	}


	csvfile.close();

*/
	/*double second = double(end - begin)/1000.0 ;
	double rate = (double)record_count / second;
	cout<< "NUMBER OF RECORDS: " << record_count << '\n';
	cout << "TIME :" << end - begin << " MILLISECONDS \n";
	cout << "RATE :" << fixed << setprecision( 6 ) << rate;*/
}