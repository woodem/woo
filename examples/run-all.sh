#!/bin/bash
LOG=/tmp/woo-examples-run-all.log 
echo > $LOG # clear log
for script in *.py; do
	echo '=========================================================' >> $LOG
	echo '======================' $script '========================' >> $LOG
	woo -x $script >> $LOG 2>&1;
	echo 'Exit status' $? >> $LOG
done

woo -x -c'import woo.utils; woo.utils.runAllPreprocessors()' >> $LOG
