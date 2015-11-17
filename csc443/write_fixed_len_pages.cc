#include <stdio.h>
#include <sstream>
#include <fstream>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "pagingservice.h"
#include "recordserialize.h"

using namespace std;


#define FIXED_SIZE 1000
#define NUM_ARGS 4


int main(int argc , char** argv){
	
	//check valid call to the function.
	if(argc != NUM_ARGS){
		printf("Usage: %s <csv_file> <page_file> <page_size>\n", argv[0]);
		exit(1);
	}

	char *page_file_path = argv[2];
	FILE *page_file = fopen(page_file_path, "w");
	int page_size = atoi(argv[3]);

	ifstream csvfile;
	csvfile.open (argv[1]);

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
	
	init_fixed_len_page(&page, page_size, FIXED_SIZE);
	int record_cap = fixed_len_page_capacity(&page);

	//as long as there is a line to read.
	while(getline(csvfile, line)){
		temp = (char *)malloc(line.length() + 1);
		strcpy(temp, line.c_str());

		char *token = strtok(temp, ",");

		while (token != NULL) {
		 	char col[11];
		 	strcpy(col,token);
		 	cur_record.push_back((V)col);
		 	token = strtok(NULL, ",");
    	}


    	if(add_fixed_len_page(&page, &cur_record) < 0){
    		total_pages++;
    		fputs((char *)page.data, page_file);
    		memset(page.data,0,page_size);
    	}
		
		record_count++;	
		free(temp);
	}

	memcpy(page.data, "thur", SCHEMA_ATTR_LEN);

	if(strlen((char *)page.data) > 0){
		total_pages++;
    	fputs((char *)page.data, page_file);
    	memset(page.data,0,page_size);
	}


	
}