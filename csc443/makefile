CC = g++
CCFLAGS =  -w -g -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -pedantic
OBJECTS = recordserialize.o pagingservice.o heapservice.o 


TARGETS = write_fixed_len_pages read_fixed_len_page csv2colstore
TARGETS2 = write_fixed_len_pages read_fixed_len_page csv2heapfile scan insert update delete select csv2colstore

PAGINGDEPENDENCIES = recordserialize.o pagingservice.o 



all: $(TARGETS) $(OBJECTS)

read_fixed_len_page: read_fixed_len_page.o $(PAGINGDEPENDENCIES)
	$(CC) -o $@ $(CCFLAGS) $^

write_fixed_len_pages: write_fixed_len_pages.o $(PAGINGDEPENDENCIES)
	$(CC) -o $@ $(CCFLAGS) $^

csv2heapfile: csv2heapfile.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

scan: scan.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

insert: insert.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

update: update.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

delete: delete.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

select: select.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

csv2colstore: csv2colstore.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

%.o: %.cc
	$(CC) -c -o $@ $(CCFLAGS)  $^

clean: clean-test
	rm -rf $(OBJS) $(TARGETS) *.o

