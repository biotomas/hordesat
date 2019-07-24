# USAGE: 1-core-time n1-cores-time [n2-cores-time ...]

#echo 'set title "TODO"'
echo 'set key left'
echo 'set xlabel "Problems"'
echo 'set ylabel "Minimal Speedup"'
echo 'set logscale y'
echo 'set grid'
first=0
basis=$1
tmpsuf=$$.tmp
rm -f *.tmp
shift
for var in "$@"
do
    join -e '300.00' -a1 -a2 -o '0,1.2,2.2' $basis $var | while read pair
    do
        left=`echo $pair | cut -d " " -f 2`
	right=`echo $pair | cut -d " " -f 3`
	speedup=`bc <<< 'scale=2; '$left'/'$right`
	#echo $left $right $speedup
	echo $speedup
    #done
    done | sort -n >> sp.$var.$tmpsuf

    if [ $first -eq 0 ]
    then
	echo 'plot "'sp.$var.$tmpsuf'" w l t "'$var'"'
	first=1
    else
	echo 'replot "'sp.$var.$tmpsuf'" w l t "'$var'"'
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
