#include <stdio.h>
#include <sstream>
#include <fstream>
#include <string.h>
#include <iostream>
#include <stdlib.h>
#include <iomanip>
#include <sys/timeb.h>
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
	struct timeb stimer;
	long begin, end;
	
	init_fixed_len_page(&page, page_size, FIXED_SIZE);
	int record_cap = fixed_len_page_capacity(&page);

	ftime(&stimer);
	begin = stimer.time * 1000 + stimer.millitm;

	while(getline(csvfile, line)){
		cur_record.clear();

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
    	 	fwrite (page.data , 1, page.page_size, page_file);
    	 	memset(page.data,0,page_size);
    	 	total_pages++;

    	 	add_fixed_len_page(&page, &cur_record);
    	}
		record_count++;	
	}

	if (fixed_len_page_freeslots(&page) < fixed_len_page_capacity(&page)) {
		fwrite (page.data , 1, page.page_size, page_file);
		total_pages++;
    	memset(page.data,0,page_size);
    	cur_record.clear();
	}

	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	fclose(page_file);
	csvfile.close();


	double second = double(end - begin)/1000.0 ;
	double rate = (double)record_count / second;
	cout<< "NUMBER OF RECORDS: " << record_count << '\n';
	cout<< "NUMBER OF PAGES: " << total_pages << '\n';
	cout << "TIME :" << end - begin << " MILLISECONDS \n";
	cout << "RATE :" << fixed << setprecision( 6 ) << rate;

}