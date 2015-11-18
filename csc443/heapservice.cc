#include <cstdio>
#include <stdint.h>

#include "heapservice.h"




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
	return heap_page;
}


Record create_heap_record(){
	Record heap_record(HEAP_NUM_VARS);
	for(int z = 0; z < HEAP_NUM_VARS; z++){
			heap_record.at(z) = new char[HEAP_OFFSET_LEN];
	}
	return heap_record;
}



void search_page(Heapfile *heapfile, PageID pageid, int *page_slot, Page **master_dir){

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
	Record master_record = create_heap_record();
	MasterDirectoryEntry *master_entry;
	bool found  = false;

	//We go through Heap file, and match each page record to find search record.
	while(next_dir_offset != OFFSET_NULL && !found){

		current_dir_offset = next_dir_offset;
		fseeko(file,current_dir_offset,SEEK_SET);
		fread(current_dir->data, page_size, 1, file);
		int slots = fixed_len_page_capacity(current_dir);

		for (int z = 0; z < slots; z++){

			Record cur_page_record = create_heap_record();
			if(read_fixed_len_page(current_dir, z, &cur_page_record)){
				DirectoryEntry cur_page_entry = (DirectoryEntry *) cur_page_record.at(0);
				if(cur_page_entry->page_index == pageid){
					search_slot = z;
					search_offset = cur_page_entry->offset;
					found = true;
					break;

				}
			}
		}

		if(!read_fixed_len_page(current_dir,0,&master_record)){
			printf("Failed to read directory from heap file, during search page.\n");
			exit(1);
		} else{
			master_entry = (MasterDirectoryEntry *) master_record.at(0);

		}

		if((master_entry->offset != current_dir_offset) || (master_entry->identifier != MASTERENTRYID)) {
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
	
	heapfile->lastest_dir = INITIAL_OFFSET;
	heapfile->page_size = page_size;
	heapfile->file_ptr = file;
	heapfile->nextId = 0;


	//Create Main/Master Directory
	Page * directory_main = create_heap_page(Heapfile *heapfile);







	if(!is_read){

		// create a new directory page within Master Directory
		create_master_dir_page(directory_main, INITIAL_OFFSET){


		//Write to File
		fseeko(file,INITIAL_OFFSET,SEEK_SET);
		fwrite(directory_main->data, page_size, 1, file);
		fflush(file);





	} else {

		//We are reading from Heap file
		//TODO




	}
	//Free memory occupied by main dir.
	void destroy(directory_main){




}
/**
 * Allocate another page in the heapfile.  This grows the file by a page.
 */
PageID alloc_page(Heapfile *heapfile){
	

	// Create new page to allocate
	Page *to_allocate  = new Page;
	init_fixed_len_page(to_allocate, heapfile->page_size, HEAP_NUM_VARS * HEAP_OFFSET_LEN);
	int target_slot;
	PageID page_id = heapfile->nextId++;


	//Fetch Details from heap file
	int page_size = heapfile->page_size;
	FILE *file = heapfile->file_ptr;
	
	//get page offset in heapfile
	fseeko(file,0,SEEK_END);
	int64_t page_offset = ftello(file);

	//create Directory Entry
	Record directory_entry = create_heap_record();
	
	DirectoryEntry dir_entry = (DirectoryEntry *) directory_entry.at(0);
	dir_entry->page_index = page_id;
	dir_entry->num_slots = fixed_len_page_freeslots(to_allocate);
	dir_entry->offset = page_offset;

	//Write this new page to the heap file.
	fseeko(file,page_offset,SEEK_SET);
	fwrite(to_allocate->data, page_size, 1, file);
	fflush(file);

	//Now we need to add this new page entry into the directory.
	//create directory
	Page * target_dir = Page *create_heap_page(Heapfile *heapfile);
	MasterDirectoryEntry * master_entry;
	Record master_directory_entry = create_heap_record();

	//Read directory information from file.
	fseeko(file,heapfile->lastest_dir,SEEK_SET);
	fread(target_dir->data,page_size,1,file);
	read_fixed_len_page(target_dir, 0, &master_directory_entry);
	master_entry = (MasterDirectoryEntry *) master_directory_entry.at(0);
	if((master_entry->offset != heapfile->lastest_dir) || (master_entry->identifier != MASTERENTRYID)) {
		printf("Error reading master directory entry from heapfile.\n");
		exit(1);
	}

	//Now we add the directory_entry to master_directory_entry
	target_slot = add_fixed_len_page(target_dir,directory_entry);

	//Now check if target_dir was full.
	if(target_slot == -1){

		//update current target_dir info and write to file.
		//get  latest offset
		fseeko(file, 0, SEEK_END);
		int64_t updated_offset = ftello(file);
		
		//Update latest dir to new dir offset.
		heapfile->latest_dir = updated_offset;
		master_entry->next = updated_offset;
		write_fixed_len_page(target_dir, 0, &master_directory_entry);

		//Write updated dir entry to file.
		fseeko(file,updated_offset,SEEK_SET);
		fwrite(target_dir->data, page_size, 1, file);
		fflush(file);

		//delete current target dir
		destroy(target_dir);


		//Re-initialize target directory, as part of the linked list.
		target_dir = create_heap_page(heapfile);

		create_master_dir_page(target_dir, updated_offset);


		target_slot = add_fixed_len_page(target_dir, directory_entry);
		if(target_slot == -1){
			printf("New allocated Page not added new directory");
			exit(1);
		}
		

	}

	// Finally we write 
	fseeko(file,heapfile->lastest_dir, SEEK_SET);
	fwrite(target_dir->data, page_size, 1, file);
	fflush(file);



	destroy(target_dir);
	destroy(to_allocate);


	return pageId;



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

	search_offset = search_page(heapfile,pid,&master_dir,&search_slot);
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

	search_offset = search_page(heapfile,pid,&master_dir,&search_slot);
	//PageID must exist for write to work.
	if(search_offset != -1){

		Record dir_record = create_heap_record();
		if(read_fixed_len_page(master_dir,search_slot,&dir_record)){

			DirectoryEntry * dir_entry = (DirectoryEntry *) dir_record.at(0);
			dir_entry->num_slots = fixed_len_page_freeslots(page);

			write_fixed_len_page(directory, search_slot, &dir_record);

			if(read_fixed_len_page(master_dir,0,&dir_record)){
				MasterDirectoryEntry * master_entry = (MasterDirectoryEntry *) master_record.at(0);
				fseeko(file,master_entry->offset,SEEK_SET);
				fwrite(master_dir->data, page_size, 1, file);




			}


		}

		fseeko(file,search_offset,SEEK_SET);
		fwrite(page->data, page_size,file);
		fflush(file);

		destroy(master_dir);

		return true;
	}

	return false;





}