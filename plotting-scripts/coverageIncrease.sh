# USAGE: 1-core-time n1-cores-time [n2-cores-time ...]

#echo 'set title "TODO"'
#echo 'set key left'
echo 'set xlabel "Time Limit in Seconds"'
echo 'set ylabel "Coverage Increase Factor"'
#echo 'set logscale y'
echo 'set grid'
basis=$1
tlimit=300
for i in `seq 1 $tlimit`
do
    orgHist[i]=0
done
cat $basis | cut -d " " -f 2 > $$.tmp

while read f
do
    round=`bc <<< $f'/1'`
    old=${orgHist[round]}
    orgHist[$round]=`expr $old + 1`
done < $$.tmp
rm $$.tmp

subsum=0
for i in `seq 1 $tlimit`
do
    old=${orgHist[i]}
    subsum=`expr $old + $subsum`
    orgHist[$i]=$subsum
    #echo $i $old ${orgHist[i]}
done

shift
first=0
for var in "$@"
do
    for i in `seq 1 $tlimit`
    do
        hist[i]=0
    done
    cat $var | cut -d " " -f 2 > $$.tmp

    while read f
    do
	round=`bc <<< $f'/1'`
        old=${hist[round]}
	hist[$round]=`expr $old + 1`
    done < $$.tmp
    rm $$.tmp

    subsum=0
    for i in `seq 1 $tlimit`
    do
        old=${hist[i]}
        subsum=`expr $old + $subsum`
        hist[$i]=$subsum
	bc <<< 'scale=2;'${hist[i]}'/'${orgHist[i]}
	#echo $i ${hist[i]} ${orgHist[i]}
        #echo $i $old ${orgHist[i]}
    done > $var.$$.tmp

    if [ $first -eq 0 ]
    then
	echo 'plot "'$var.$$.tmp'" w l t "'$var'"'
	first=1
    else
	echo 'replot "'$var.$$.tmp'" w l t "'$var'"'
    fi
done
if [[ -z $filename ]]
then
    echo 'pause 1000'
else
    echo 'set term pdf'
    echo 'set output "'$filename'"'
    echo 'replot'
fi
#rm *.$$.tmp
