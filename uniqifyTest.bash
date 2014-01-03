#!/bin/bash
##########################################################################################
# 1. Build your uniqify application (using your Makefile).
# 2. Run the uniquify.bash script on each of the input files and produce a 
#    base output file.
# 3. Run your uniqify code on each of the input files, varying the number of 
#    sort sub-processes (the –c # command line option) from 1 to 9, and comparing
#    the output from your code to the output from the uniqify.bash script.
# 4. Clearly identify when the base output file and your code’s output file differs.
# 5. At a minimum, you need to use the 6 sample input files provided to you.
# 6. Except for the simple.txt file, the sample input files were found at Project
#    Gutenberg web site. Some of them had the UTF-8 characters purged from the
#    content before providing them to you.
##########################################################################################

### VARIABLES ###
file1="simple"
file2="words"
file3="websters"
file4="decl-indep"
file5="jargon"
file6="iliad"

dir=$PWD"/"
ext=".txt"
CMD1="./uniquify.bash"
CMD2="./uniqify"

make

# create base output files with uniquify.bash for each file
echo -e "\nCreating base output files using uniqify.bash..."
for f in $file1 $file2 $file3 $file4 $file5 $file6
do
	$CMD1 < $f$ext > $f"_base"$ext
done


for i in {1..9}
do
	echo -e "\nRunning uniqify with $i sort sub-processes..."

	# for each input file
	for f in $file1 $file2 $file3 $file4 $file5 $file63
	do
		$CMD2 -c $i < $f$ext > $f"_test"$ext
		echo $f$ext differences:
		diff --suppress-common-lines -y $f"_base"$ext $f"_test"$ext
	done
	sleep .75
done





