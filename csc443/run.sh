#!/bin/bash

echo "Starting to run the experiment..";

SAMPLE_CSV="sample1.csv"; 

ONE_ATTR = "abcdefghij";
RECORD = ONE_ATTR;

for ((i=0; i<99;i++)){
	RECORD += "," + $ONE_ATTR;
}


