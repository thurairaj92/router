#include "recordserialize.h"




/**
 * Compute the number of bytes required to serialize record
 */
int fixed_len_sizeof(Record *record){

	return SCHEMA_NUM_ATTR * SCHEMA_ATTR_LEN;


}

/**
 * Serialize the record to a byte array to be stored in buf.
 */
void fixed_len_write(Record *record, void *buf){

	//Iterate over the records and copy them to the buffer.
	for(Record::iterator z = record->begin(); z < record->end(); z++){
		memcpy(buf, *z, SCHEMA_ATTR_LEN);
		buf = ((char *) buf) + SCHEMA_ATTR_LEN;
	}


}



/**
 * Deserializes `size` bytes from the buffer, `buf`, and
 * stores the record in `record`.
 */
void fixed_len_read(void *buf, int size, Record *record){
	//If size matches record size, then only do this.
	if(size == (fixed_len_sizeof(record))){
		//loop over attr times and populate the record.
		for(int z = 0; z < SCHEMA_NUM_ATTR; z++){
			char *curattr = (char *) record->at(z);
			int offset = SCHEMA_ATTR_LEN * z;
			((char *) buf) + offset;
			memcpy(curattr, ((char *) buf) + offset, SCHEMA_ATTR_LEN);

		}
	}


}