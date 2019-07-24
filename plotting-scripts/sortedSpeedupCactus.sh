# USAGE: 1-core-time n1-cores-time [n2-cores-time ...]

echo 'set title "TODO"'
echo 'set key left'
echo 'set xlabel "Problems"'
echo 'set ylabel "Time in seconds"'
#echo 'set logscale y'
echo 'set grid'
tlimit=300
first=0
basis=$1
tmpsuf=$$.tmp

cut -d " " -f 2 $basis | sort -n > base.$tmpsuf

shift
for var in "$@"
do
    cut -d " " -f 2 $var | sort -n > $var.$tmpsuf
    paste base.$tmpsuf $var.$tmpsuf -d: | while read pair
    do
	if [[ $pair == :* ]]
	then
	    left=$tlimit
	else
            left=`echo $pair | cut -d: -f 1`
	fi
	right=`echo $pair | cut -d: -f 2`
	speedup=`bc <<< 'scale=2; '$left'/'$right`
	#echo $pair "|" $left $right $speedup
	echo $speedup
    #done
    done | sort -n >> sp.$var.$tmpsuf
    #done >> sp.$var.$tmpsuf


    if [ $first -eq 0 ]
    then
	echo 'plot "'sp.$var.$tmpsuf'" w l t "'$var'"'
	first=1
    else
	echo 'replot "'sp.$var.$tmpsuf'" w l t "'$var'"'
    fi
done
echo 'pause 1000'
echo '#set term pdf'
echo '#set output "TODO"'
echo '#replot'
rm *.$tmpsuf

