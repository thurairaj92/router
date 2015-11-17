#include <stdio.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <stdlib.h>
#ifndef RECORDSERIALIZE_H
#define RECORDSERIALIZE_H


#define SCHEMA_NUM_ATTR 100
#define SCHEMA_ATTR_LEN 10

typedef const char* V;
typedef std::vector<V> Record;


/**
 * Compute the number of bytes required to serialize record
 */
int fixed_len_sizeof(Record *record);

/**
 * Serialize the record to a byte array to be stored in buf.
 */
void fixed_len_write(Record *record, void *buf);



/**
 * Deserializes `size` bytes from the buffer, `buf`, and
 * stores the record in `record`.
 */
void fixed_len_read(void *buf, int size, Record *record);


#endif