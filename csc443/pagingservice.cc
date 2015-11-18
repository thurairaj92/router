
#include "pagingservice.h"
#include "recordserialize.h"

/**
 * Initializes a page using the given slot size
 */
void init_fixed_len_page(Page *page, int page_size, int slot_size){
	//Intitalize Page Struct, and set data to char[page_sze].

	
	//page->data = new char[page_size];
	page->data = malloc(page_size);
	page->page_size = page_size;
	page->slot_size = slot_size;
	memset(page->data,0,page_size);

}

/**
 * Calculates the maximal number of records that fit in a page
 */
int fixed_len_page_capacity(Page *page){
	//Need to make space for directory along with records.
	int char_size = sizeof(char);
	int num_records = page->page_size / (page->slot_size + char_size);
	return num_records;

}
 
/**
 * Calculate the free space (number of free slots) in the page
 */
int fixed_len_page_freeslots(Page *page){
	//Irerate over the directory on top to 
	int free_slots  = 0;
	int page_capacity = fixed_len_page_capacity(page);
	char *page_dictionary = (char *) page->data;



	for(int z = 0; z < page_capacity; z++){
		int slot_value = 1 - page_dictionary[z];
		// std::cout << "Index z: " << z << " is " << slot_value << '\n' ;
		free_slots += slot_value;
	}

	return free_slots;

}
 
/**
 * Add a record to the page
 * Returns:
 *   record slot offset if successful,
 *   -1 if unsuccessful (page full)
 */
int add_fixed_len_page(Page *page, Record *r){
	int num_free_slots = fixed_len_page_freeslots(page);
	// std::cout << "------------------" << '\n';
	//add the page if there are free slots;

	if(num_free_slots > 0){
		int page_capacity = fixed_len_page_capacity(page);
		char *page_dictionary = (char *) page->data;
		int slot_to_insert = -1;
		for(int z = 0; z < page_capacity; z++){
			if(page_dictionary[z] == 0){
				slot_to_insert = z;
				break;
			}
		}

		if(slot_to_insert != -1){
			write_fixed_len_page(page,slot_to_insert,r);
			return slot_to_insert;
		}
		return -1;


	}
	return -1;


}
 
/**
 * Write a record into a given slot.
 */
void write_fixed_len_page(Page *page, int slot, Record *r){
	char * page_dictionary = (char *) page->data;
	char * slot_entry = page_dictionary + fixed_len_page_capacity(page) + (slot * page->slot_size);
	
	fixed_len_write(r, slot_entry);
	page_dictionary[slot] = 1;
}
 
/**
 * Read a record from the page from a given slot.
 */
bool read_fixed_len_page(Page *page, int slot, Record *r){
	char * page_dictionary = (char *) page->data;
	
	if(page_dictionary[slot] == 1){
		char * slot_entry = page_dictionary + fixed_len_page_capacity(page) + (slot * page->slot_size);
		int size = fixed_len_sizeof(r);
		fixed_len_read(slot_entry, size, r);
		return true;
	}else{
		return false;
	}
}
