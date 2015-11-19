#include <stdio.h>
#include <sstream>
#include <fstream>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <sys/timeb.h>
#include "pagingservice.h"
#include "recordserialize.h"

using namespace std;


#define FIXED_SIZE 1000
#define NUM_ARGS 3


int main(int argc , char** argv){
	
	//check valid call to the function.
	if(argc != NUM_ARGS){
		printf("Usage: %s <page_file> <page_size>\n", argv[0]);
		exit(1);
	}

	char *csv_file_path = "table.csv";
	FILE *csv_file = fopen(csv_file_path, "w");

	char *page_file_path = argv[1];
	FILE *pagefile = fopen(page_file_path, "r");

	int page_size = atoi(argv[2]);

	if(!csvfile){
		printf("Cannot open csv_file, please give valid csv_file.\n");
	}

	if(!page_file){
		printf("Cannot open page_file, please give valid page_file.\n");
	}

	Record cur_record;
	string line;
	char *temp;
	Page page;

	int total_pages = 0;
	int record_count = 0;
	struct timeb stimer;
	long begin, end;
	
	init_fixed_len_page(&page, page_size, FIXED_SIZE);
	int record_cap = fixed_len_page_capacity(&page);

	ftime(&stimer);
	begin = stimer.time * 1000 + stimer.millitm;

	while(fread( page.data, 1, page.page_size, pagefile ) > 0){
		
	}


	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	fclose(page_file);

	cout<< "NUMBER OF RECORDS: " << record_count << '\n';
	cout<< "NUMBER OF PAGES: " << total_pages << '\n';
	cout << "TIME :" << end - begin << " MILLISECONDS \n";
}