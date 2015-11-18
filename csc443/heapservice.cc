#include <cstdio>
#include <stdint.h>

#include "heapservice.h"

#define INITIAL_OFFSET 1
#define OFFSET_NULL 0x000


void create_master_dir_page(Page *master_dir, int64_t dir_offset){
	Record main_dir_record(HEAP_NUM_VARS);
		for(int z = 0; z < HEAP_NUM_VARS; z++){
			main_dir_record.at(z) = new char[HEAP_OFFSET_LEN];
		}

		MasterDirectoryEntry *entry = (MasterDirectoryEntry *) main_dir_record.at(0);
		entry->next = OFFSET_NULL;
		entry->offset = dir_offset;
		entry->identifier = MASTERENTRYID;
		//Add it to main directory
		int slot  = add_fixed_len_page(master_dir, &main_dir_record);
		if(slot != 0){
			printf("Master Entry not added to first slot of main_directory");
			exit(1);
		}
}

void destroy(Page *to_destroy){

	delete [] (char *) to_destroy->data;
	delete to_destroy;
}


Page *create_heap_page(Heapfile *heapfile){
	Page *heap_page = new Page;
	init_fixed_len_page(heap_page, heapfile->page_size, HEAP_NUM_VARS * HEAP_OFFSET_LEN);
	memset( heap_page->data, '\0', heapfile->page_size);
	return heap_page;
}


void create_heap_record(Record *r){
	
	for(int z = 0; z < HEAP_NUM_VARS; z++){
			r->at(z) = new char[HEAP_OFFSET_LEN];
	}
}

int max_space_for_entry(Page *page){
	int for_entries = page->page_size - sizeof(MasterDirectoryEntry);
	return for_entries/(sizeof(DirectoryEntry));
}

int space_left_for_entry(Page *page){
	int entry;
	void *buf = page->data + sizeof(MasterDirectoryEntry);
	int max = max_space_for_entry(page);

	while(entry < max && ((char *)buf)[0] != '\0'){
		buf += sizeof(DirectoryEntry);
		entry++;
	}

	return max - entry;
}

bool write_entry(Page *page, DirectoryEntry *entry){
	void *buf = page->data + sizeof(MasterDirectoryEntry);
	int max = max_space_for_entry(page);
	int no_entry = 0;

	while(no_entry < max && ((char *)buf)[0] != '\0'){
		buf += sizeof(DirectoryEntry);
		no_entry++;
	}

	std::cout << "Max : " << max << " Current" << no_entry << "\n";

	if (no_entry < max){
		memcpy(buf, entry, sizeof(DirectoryEntry));
		return true;
	}else{
		return false;
	}
}


/*void read_entry_in_slot(Page *page, DirectoryEntry *entry, int slot){
	void *buf = page->data + sizeof(MasterDirectoryEntry);
	buf += (slot * sizeof(DirectoryEntry));
	memcpy(entry, buf, sizeof(DirectoryEntry));
}*/


bool search_page_id(Page *directory , PageID pageid){
	void *buf = directory->data + sizeof(MasterDirectoryEntry);
	int max = max_space_for_entry(directory);
	DirectoryEntry *rec_entry;
	int entry = 0;

	while(entry < max){
		rec_entry = (DirectoryEntry *)(buf);
		if(rec_entry->page_index == pageid){
			return true;
		}
		buf += sizeof(DirectoryEntry);
	}

	return false;

}



int64_t search_page(Heapfile *heapfile, PageID pageid, int *page_slot, Page **master_dir){

	// Do null checks
	if((page_slot == NULL) || (master_dir == NULL)){
		printf("Search page null pointers given.\n");
		exit(1);
	}

	//Initialization
	int64_t search_offset = -1;
	int64_t search_slot;

	int page_size = heapfile->page_size;
	FILE *file = heapfile->file_ptr;

	Page *current_dir = create_heap_page(heapfile);
	int64_t current_dir_offset = INITIAL_OFFSET;
	int64_t next_dir_offset = INITIAL_OFFSET;

	MasterDirectoryEntry *master_entry;
	bool found  = false;
	void *buf;
	int max;
	DirectoryEntry *rec_entry;
	int entry ;

	//We go through Heap file, and match each page record to find search record.
	while(next_dir_offset != OFFSET_NULL && !found){

		current_dir_offset = next_dir_offset;
	
		fseeko(file,current_dir_offset,SEEK_SET);
		fread(current_dir->data, page_size, 1, file);
		
		buf = current_dir->data + sizeof(MasterDirectoryEntry);
		master_entry = (MasterDirectoryEntry *)(current_dir->data);
		max = max_space_for_entry(current_dir);
		entry = 0;

		while(entry < max){
			rec_entry = (DirectoryEntry *)(buf);
			if(rec_entry->page_index == pageid){
				search_slot = entry;
				search_offset = rec_entry->offset;
				found = true;
				break;
			}
			buf += sizeof(DirectoryEntry);
			entry++;
		}



		if((master_entry->offset != current_dir_offset)) {
			printf("Error reading master directory entry from heapfile. offset or identifer don't match.\n");
			exit(1);
		}

		next_dir_offset = master_entry->next;

	}

	if(search_offset != -1){

		*master_dir = current_dir;
		*page_slot = search_slot;

	} else {
		destroy(current_dir);
	}

	return search_offset;
}



/**
 * Initalize a heapfile to use the file and page size given.
 */
void init_heapfile(Heapfile *heapfile, int page_size, FILE *file, bool is_read){
	
	heapfile->latest_dir = INITIAL_OFFSET;
	heapfile->page_size = page_size;
	heapfile->file_ptr = file;
	heapfile->next_page_id = 0;


	//Create Main/Master Directory
	Page *directory_main = create_heap_page(heapfile);

	if(!is_read){

		// create a new directory page within Master Directory
		//create_master_dir_page(directory_main, INITIAL_OFFSET);
		
		MasterDirectoryEntry *entry = (MasterDirectoryEntry *)(directory_main->data);
		entry->next = OFFSET_NULL;
		entry->offset = INITIAL_OFFSET;
		entry->identifier = MASTERENTRYID;

		//Write to File
	
		fseeko(file,INITIAL_OFFSET,SEEK_SET);
		fwrite(directory_main->data, heapfile->page_size, 1, file);
		fflush(file);


		Page *directory_dupl = create_heap_page(heapfile);

		fseeko(file,INITIAL_OFFSET,SEEK_SET);
		fread(directory_dupl->data, heapfile->page_size, 1, file);

		MasterDirectoryEntry *dup_entry = (MasterDirectoryEntry *)(directory_dupl->data);

	} else {

		//We are reading from Heap file
		//TODO
	}
	//Free memory occupied by main dir.
	destroy(directory_main);
}




/**
 * Allocate another page in the heapfile.  This grows the file by a page.
 */
PageID alloc_page(Heapfile *heapfile){
	// Create new page to allocate
	Page *to_allocate  = new Page;
	init_fixed_len_page(to_allocate, heapfile->page_size, HEAP_NUM_VARS * HEAP_OFFSET_LEN);
	int target_slot;
	PageID page_id = heapfile->next_page_id++;


	//Fetch Details from heap file
	int page_size = heapfile->page_size;
	FILE *file = heapfile->file_ptr;
	
	//get page offset in heapfile
	fseek(file,0,SEEK_END);
	int64_t page_offset = ftello(file);


	DirectoryEntry *dir_entry = (DirectoryEntry *) malloc(sizeof(DirectoryEntry));
	dir_entry->page_index = page_id;
	dir_entry->num_slots = fixed_len_page_freeslots(to_allocate);
	dir_entry->offset = page_offset;

	//Write this new page to the heap file.
	fseeko(file,page_offset,SEEK_SET);
	fwrite(to_allocate->data, page_size, 1, file);
	fflush(file);

	//Now we need to add this new page entry into the directory.
	//create directory
	Page * target_dir = create_heap_page(heapfile);
	MasterDirectoryEntry * master_entry;

	fseeko(file,heapfile->latest_dir,SEEK_SET);
	fread(target_dir->data, target_dir->page_size, 1, file);;
	master_entry = (MasterDirectoryEntry *)(target_dir->data);


	bool wrote = write_entry(target_dir, dir_entry);

	if(!wrote){
		printf("\n\ndirectory is FULL!!\n\n");
		//update current target_dir info and write to file.
		//get  latest offset
		fseeko(file, 0, SEEK_END);
		int64_t updated_offset = ftello(file);
		
		//Update latest dir to new dir offset.
		master_entry->next = updated_offset;

		//Write updated dir entry to file.
		fseeko(file,heapfile->latest_dir,SEEK_SET);
		fwrite(target_dir->data, page_size, 1, file);
		fflush(file);

		heapfile->latest_dir = updated_offset;
		//delete current target dir
		destroy(target_dir);


		//Re-initialize target directory, as part of the linked list.
		target_dir = create_heap_page(heapfile);

		MasterDirectoryEntry *entry = (MasterDirectoryEntry *) (target_dir->data);
		entry->next = OFFSET_NULL;
		entry->offset = updated_offset;
		entry->identifier = MASTERENTRYID;



		wrote = write_entry(target_dir, dir_entry);
		if(!wrote){
			printf("New allocated Page not added new directory");
			exit(1);
		}
		fseeko(file,heapfile->latest_dir, SEEK_SET);
		fwrite(target_dir->data, page_size, 1, file);
		fflush(file);
		return page_id;

	}
	
	// Finally we write 
	fseeko(file,heapfile->latest_dir, SEEK_SET);
	fwrite(target_dir->data, page_size, 1, file);
	fflush(file);



	destroy(target_dir);
	destroy(to_allocate);


	return page_id;
}
/**
 * Read a page into memory
 */
bool read_page(Heapfile *heapfile, PageID pid, Page *page){
	Page *master_dir;
	int64_t search_offset;
	int search_slot;
	FILE *file = heapfile->file_ptr;
	int page_size = heapfile->page_size;

	search_offset = search_page(heapfile,pid,&search_slot,&master_dir);
	std::cout << "search offset read " << search_offset << "\n";
	if(search_offset != -1){

		fseeko(file,search_offset,SEEK_SET);
		fread(page->data, page_size, 1, file);

		destroy(master_dir);
		return true;
	}

	return false;
}

/**
 * Write a page from memory to disk
 */
bool write_page(Page *page, Heapfile *heapfile, PageID pid){
	Page *master_dir;
	int64_t search_offset;
	int search_slot;
	FILE *file = heapfile->file_ptr;
	int page_size = heapfile->page_size;

	search_offset = search_page(heapfile,pid,&search_slot, &master_dir);
	std::cout << "search offset write " << search_offset << "\n";
	//PageID must exist for write to work.
	if(search_offset != -1){
		DirectoryEntry *dir_record;

		void *buf = master_dir->data + sizeof(MasterDirectoryEntry);
		buf += (search_slot * sizeof(DirectoryEntry));
		dir_record = (DirectoryEntry *)(buf);
		dir_record->num_slots = fixed_len_page_freeslots(master_dir);

		MasterDirectoryEntry *master_entry = (MasterDirectoryEntry *)(master_dir->data);
		fseeko(file,master_entry->offset,SEEK_SET);
		fwrite(master_dir->data, page_size, 1, file);

		fseeko(file,search_offset,SEEK_SET);
		fwrite(page->data, page_size,1,file);
		std::cout << "WRITE" << (char *)page->data << "\n";
		fflush(file);

		destroy(master_dir);

		Page * newPage = create_heap_page(heapfile);
		read_page(heapfile,pid,newPage);

		std::cout << "READ" << (char *)newPage->data << "\n";
		return true;

	}

	return false;
}
	



class RecordIterator {
	RecordID *record;
	Heapfile *heap;
	Page *cur_page;

    public:
    RecordIterator(Heapfile *heapfile);
    Record next();
    bool hasNext();
};

RecordIterator::RecordIterator(Heapfile *heapfile){
	record = (RecordID *)malloc(sizeof(record));
	record->page_id = 0;
	record->slot = 0;
	heap = heapfile;
	read_page(heap, record->page_id, cur_page);

}

bool RecordIterator::hasNext(){
	while(heap->page_id)
}



