CC = g++
CCFLAGS = -Wall -g -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -pedantic
OBJECTS = recordserialize.o pagingservice.o 


TARGETS = write_fixed_len_pages  

PAGINGDEPENDENCIES = recordserialize.o pagingservice.o 



all: $(TARGETS) $(OBJECTS)

write_fixed_len_pages: write_fixed_len_pages.o $(PAGINGDEPENDENCIES)
	$(CC) -o $@ $(CCFLAGS) $^



%.o: %.cc
	$(CC) -c -o $@ $(CCFLAGS)  $^

clean: clean-test
	rm -rf $(OBJS) $(TARGETS) *.o

