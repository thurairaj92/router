#ifndef HEAPSERVICE_H
#define HEAPSERVICE_H


#include "pagingservice.h"
#include <stdint.h>

typedef int PageID;

typedef struct {
    FILE *file_ptr;
    int page_size;
    int64_t latest_dir;
    PageID next_page_id;

} Heapfile;


 
typedef struct {
    int page_id;
    int slot;
} RecordID;



#define INITIAL_OFFSET 1
#define OFFSET_NULL 0x000
#define MASTERENTRYID 0xCAFED00DB16B00B5LL


typedef struct {
	int64_t next;
	int64_t offset;
	uint64_t identifier;
} MasterDirectoryEntry;

typedef struct {

	int64_t page_index;
	int64_t num_slots;
	int64_t offset;

} DirectoryEntry;


/**
 * Initalize a heapfile to use the file and page size given.
 */
void init_heapfile(Heapfile *heapfile, int page_size, FILE *file, bool is_read=false);
/**
 * Allocate another page in the heapfile.  This grows the file by a page.
 */
PageID alloc_page(Heapfile *heapfile);
/**
 * Read a page into memory
 */
bool read_page(Heapfile *heapfile, PageID pid, Page *page);
/**
 * Write a page from memory to disk
 */
bool write_page(Page *page, Heapfile *heapfile, PageID pid); 

Page *create_heap_page(Heapfile *heapfile);

int max_space_for_entry(Page *page);

class MasterDirectoryIterator {
    public:
   	 	MasterDirectoryIterator(Heapfile *heapfile);
    	Page * next();
    	bool hasNext();

	private:
		Heapfile *heap_file;

		Page *latest_dir;

		int64_t next_dir_offset;


};

class DirectoryEntryIterator {
    public:
   	 	DirectoryEntryIterator(Heapfile *heapfile, Page *latest_dir_in);
    	Page * next(int64_t *offset);
    	bool hasNext();

	private:
		Heapfile *heap_file;

		Page *latest_dir;

		Page *latest_page;
		int cur_slot;
		int page_capacity;
		void *buf;
	
};

class RecordIterator {
    public:
   	 	RecordIterator(Heapfile *heapfile);
    	Record *next();
    	bool hasNext();
    	bool nextSlot();
    	bool nextDirEntry();
    	bool nextMasterDir();

	private:
		Heapfile *heap_file;
		DirectoryEntryIterator *dirEntryIterator;
		MasterDirectoryIterator masterDirIterator;
		PageSlotIterator * pageSlotIterator;
		Page *latest_dir;

		int64_t next_dir_offset;
};





#endif