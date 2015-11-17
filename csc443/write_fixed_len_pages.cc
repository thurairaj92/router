#include <stdio.h>
#include <sstream>
#include <fstream>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include "pagingservice.h"
#include "recordserialize.h"



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
	size_t page_size = atoi(argv[3]);
	std::ifstream input_csv(argv[1]);

	if(!input_csv){
		printf("Cannot open csv_file, please give valid csv_file.\n");

	}


	if(!page_file){
		printf("Cannot open page_file, please give valid page_file.\n");

	}


	Page page;
	int rows = 0;
	int total_pages = 0;
	int record_count = 0;
	init_fixed_len_page(&page, page_size, FIXED_SIZE);
	int record_cap = fixed_len_page_capacity(&page);

	//as long as there is a line to read.
	while(input_csv){

		// Create a new record.
		Record cur_record (SCHEMA_NUM_ATTR);

		//Initialize Second Dimension
		for(int z = 0; z < SCHEMA_NUM_ATTR; z++){

			cur_record.at(z) = new char[10];


		}

		






		std::string input_buffer;
		input_buffer.reserve(FIXED_SIZE + SCHEMA_NUM_ATTR + 1);

		// if we don't have anything to read then break
		if(!std::getline(input_csv, input_buffer)){
			break;
		}

		std::istringstream input_stream(input_buffer);

		char* buffer = new char[input_buffer.length()+1];
		strcpy(buffer,input_buffer.c_str());

		char *token = std::strtok(buffer, ",");
		printf("%s\n", (char*)cur_record.at(0));
		int record_index = 0;
		 while (token != NULL) {
		 	char col[11];
		 	strcpy(col,token);

		 	strcpy((char*)cur_record.at(record_index++),col);
		 	token = std::strtok(NULL, ",");
        	
        	
        
        	
    	}



    	for (Record::iterator it = cur_record.begin(); it < cur_record.end(); it++) {
	        const char *attr = *it;
	        printf("%s", attr);
    	}	


    	//int slot = add_fixed_len_page(&page, &cur_record);
    	//printf("%d", slot);
		//Use Strtok, to get char array





















	}










}