#include <stdio.h>
#include <string.h>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
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

	char *page_file_path = argv[1];
	FILE *page_file = fopen(page_file_path, "r");

	int page_size = atoi(argv[2]);

	if(!page_file){
		printf("Cannot open page_file, please give valid page_file.\n");
	}
	
	string line;
	string buf;
	char *temp;
	const char *attr;

	Page page;

	int total_pages = 0;
	int record_count = 0;
	struct timeb stimer;
	long begin, end;
	
	init_fixed_len_page(&page, page_size, FIXED_SIZE);
	int record_cap = fixed_len_page_capacity(&page);


	//initialize record
	Record cur_record (SCHEMA_NUM_ATTR);
	for(int z = 0; z < SCHEMA_NUM_ATTR; z++){
		cur_record.at(z) = new char[11];
	}

	ftime(&stimer);
	begin = stimer.time * 1000 + stimer.millitm;

	size_t read = fread( page.data, 1, page.page_size, page_file );
	while(read > 0){
		for(int i = 0; i < fixed_len_page_capacity(&page); i++){
			if(read_fixed_len_page(&page, i, &cur_record)){
				for(int z = 0; z < SCHEMA_NUM_ATTR-1; z++) {
					attr = cur_record.at(z);
					string str(attr);
					buf += str + ",";
				}

				attr = cur_record.at(SCHEMA_NUM_ATTR-1);
				string str(attr);
				buf += str;
				//cout << buf << "\n";
				buf.clear();
				str.clear();

				record_count++;
			}
		}
		read = fread( page.data, 1, page.page_size, page_file );
	}


	ftime(&stimer);
	end = stimer.time * 1000 + stimer.millitm;
	fclose(page_file);

	double second = double(end - begin)/1000.0 ;
	double rate = (double)record_count / second;
	
	cout << "RECORDS :" << record_count << "\n";
	cout << "TIME :" << end - begin << " MILLISECONDS \n";
	cout << "RATE :" << fixed << setprecision( 6 ) << rate << "r/s \n";
}



