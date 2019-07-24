#!/bin/bash

echo "USAGE: [filename=<name.pdf>] [logscale=yes] [title=<String>] ./scatter.sh inp1 inp2 | gnuplot" >&2

# Set the plot title if specified
if [[ -n $title ]]
then
	echo 'set title "'$title'"'
fi

# Set the key position and labels
echo 'set key left'
echo 'set xlabel "'$1'"'
echo 'set ylabel "'$2'"'
if [[ -n $logscale ]]
then
    echo 'set logscale'
fi

# plot the central line
echo 'set xrange [1 2]'
echo 'set yrange [1 2]'
echo 'plot x w l t ""'

# prepare the data
tmpfile=joined$1$2.tmp
join $1 $2 > $tmpfile

# Plot the data
echo 'replot "'$tmpfile'" using 2:3 w p t ""'

# If no filename specified just show the plot
if [[ -z $filename ]]
then
    echo 'pause 1000'
else
    echo 'set term pdf'
    echo 'set output "'$filename'"'
    echo 'replot'
fi
