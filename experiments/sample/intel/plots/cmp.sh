wls=( "smq" "obim" "pmod" )
for algo in bfs sssp; do
   for gr in usa west web twi; do
      echo "$algo $gr"
      for wl in "${wls[@]}"; do
      	tail -n 10 "${wl}_plots/${algo}_${gr}_${wl}"
     done
     echo ">>>\n\n"
  done
done

for algo in boruvka astar; do
   for gr in usa west; do
      echo "$algo $gr"
      for wl in "${wls[@]}"; do
        tail -n 10 "${wl}_plots/${algo}_${gr}_${wl}"
     done
     echo ">>>\n\n"
  done
done
