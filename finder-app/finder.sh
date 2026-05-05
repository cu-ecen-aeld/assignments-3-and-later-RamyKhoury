#!/bin/sh

# Verify arguments
if [ $# -ne 2 ]; then
    echo "Please specify all the parameters:"
    echo "finder.sh [filesdir] [searchstr]" 
    echo "[filesdir]: directory containing files and/or subdirectories"
    echo "[searchstr]: a string which is searched for within the files in filesdir"
    exit 1
elif [ ! -d "$1" ]; then
    echo "The first argument must be a directory path, no directory path found at: $1"
    exit 1
fi

filesdir=$(realpath $1)
searchstr=$2

# Get number of files in directory
numfiles=$(find "$filesdir" -type f | wc -l)

# and count of matches inside these files
nummatches=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $numfiles and the number of matching lines are $nummatches"