#!/bin/bash
make
echo "Starting to run the experiment..";

SAMPLE_CSV=sample.csv
OUT_CSV=table.csv
PAGE_FILE=page
ONE_ATTR="abcdefghij"
RECORD=$ONE_ATTR
PAGE_SIZE=50000
WRITE_FIXED_LEN_PAGES="write_fixed_len_pages";
READ_FIXED_LEN_PAGE="read_fixed_len_page";
GRAPH_FILE=graph.txt

rm -f $GRAPH_FILE

for ((i=0; i<99;i++)){
	RECORD="$RECORD,$ONE_ATTR";
}

MULTIPLE_RECORD=$RECORD

if [ ! -f $SAMPLE_CSV ]; then
	touch $SAMPLE_CSV
else
	rm -f $SAMPLE_CSV
	touch $SAMPLE_CSV
fi

for ((i=0; i<5000;i++)){
	echo $MULTIPLE_RECORD >> $SAMPLE_CSV;
}

for j in {0..15..1}
	do
		
		rm -f $OUT_CSV
		touch $OUT_CSV
		PAGE_SIZE=$((($j+1)*5000));

		WRITE_TIME=`./$WRITE_FIXED_LEN_PAGES $SAMPLE_CSV $PAGE_FILE $PAGE_SIZE| grep "RATE" | sed 's/[^0-9|.]//g'` ;
		READ_TIME=`./$READ_FIXED_LEN_PAGE $PAGE_FILE $PAGE_SIZE| grep "RATE" | sed 's/[^0-9|.]//g'` ;
	


		echo "Pagesize : $PAGE_SIZE ; Write time : $WRITE_TIME r/s; Read time : $READ_TIME r/s;";

		echo $PAGE_SIZE $WRITE_TIME $READ_TIME >> $GRAPH_FILE;
	done



