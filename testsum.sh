#!/bin/bash

nclient=$(grep -c "client" testout.log)
echo "Numero di client lanciati in tutti i test: $nclient"

for ((i=1; i<=3; i++)) do
  ntestok=$(grep -c "Test $i: superato" testout.log)
  ntestko=$(grep -c "Test $i: fallito" testout.log)

  echo -e "\nBatteria di test numero: $i, numero di test superati: $ntestok"
  echo "Batteria di test numero: $i, numero di test falliti: $ntestko"

  if [ ! $ntestko -eq 0 ]
  then
    # stampo i nomi dei client che hanno fallito e il numero di operazioni fallite
    echo -e "\n\t::::::Lista dei client che hanno fallito::::::\n"
    grep "Test $i: fallito" testout.log | cut -f 1,3 -d ','
  fi
done
