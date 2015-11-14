#include <iostream>
#include <string.h>
#include <vector>

typedef const char* V;
typedef std::vector<V> Record;
#define RECORD_SIZE 1000;
/*https://csc.cdf.toronto.edu/mybb/showthread.php?tid=11929*/


int main(){

}

int fixed_len_sizeof(Record *record){
	return record->size() * RECORD_SIZE;
}

void fixed_len_write(Record *record, void *buf){
	buf = malloc(fixed_len_sizeof(record));
	void *head = buf;
	for(int i = 0; i < record->size(); i++){
		memcpy( (char*)head , record->at(i), 1000);
		head += RECORD_SIZE;
	}
}

//I'm not sure buf contains only one record or multiple records.. 
void fixed_len_read(void *buf, int size, Record *record){
	int records_req = (size + RECORD_SIZE - 1) / RECORD_SIZE;
	record->reserve(strlen(records_req));
	char *new;

	for(int i = 0; i < records_req ; i++){
		new = malloc(RECORD_SIZE);
		strcpy_s()
	}


}