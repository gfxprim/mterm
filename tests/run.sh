#!/bin/sh

failed=0
passed=0

for i in *.in; do
	../mterm-test $i > tmp 2> /dev/null
	if [ $? -ne 0 ]; then
		echo "************** $i **************"
		../mterm-test -v $i > tmp
		echo "Test finished with error!!!";
		echo "************************************"
		failed=$((failed+1))
		continue;
	fi
	OUT=$(echo $i | sed s/\.in$/\.out/)
	if ! [ -e "$OUT" ]; then
		echo "**** File $OUT does not exits"
		cat tmp
		echo -n "**** Accept y/n "
		read res
		if [ $res = 'y' ]; then
			cp tmp "$OUT"
		fi
	fi
	if ! diff tmp $OUT &> /dev/null; then
		echo "************** $i **************"
		diff -u $OUT tmp
		echo "************************************"
		failed=$((failed+1))
	else
		passed=$((passed+1))
	fi
done

rm tmp

echo "passed: $passed failed: $failed"

[ $failed -eq 0 ]
