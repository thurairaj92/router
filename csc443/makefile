CC = g++
CCFLAGS = -Wall -g -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -pedantic
OBJECTS = recordserialize.o pagingservice.o heapservice.o 


TARGETS = write_fixed_len_pages read_fixed_len_page csv2heapfile

PAGINGDEPENDENCIES = recordserialize.o pagingservice.o 



all: $(TARGETS) $(OBJECTS)

read_fixed_len_page: read_fixed_len_page.o $(PAGINGDEPENDENCIES)
	$(CC) -o $@ $(CCFLAGS) $^

write_fixed_len_pages: write_fixed_len_pages.o $(PAGINGDEPENDENCIES)
	$(CC) -o $@ $(CCFLAGS) $^

csv2heapfile: csv2heapfile.o $(PAGINGDEPENDENCIES) heapservice.o 
	$(CC) -o $@ $(CCFLAGS) $^

%.o: %.cc
	$(CC) -c -o $@ $(CCFLAGS)  $^

clean: clean-test
	rm -rf $(OBJS) $(TARGETS) *.o

