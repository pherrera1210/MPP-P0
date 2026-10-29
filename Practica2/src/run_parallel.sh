#!/bin/bash

make parallel

in_file="../input/igen/in.txt"
in_file_distances="../input/input"

# Lista de hilos
LISTA_HILOS=(1 2 3 4 5 6 7 8)

while read line ; do 

    if [[ $line != \#* ]] && [[ -n $line ]]
    then
        n=$(echo $line | tr -s ' ' | cut -f1 -d ' ')
        m=$(echo $line | tr -s ' ' | cut -f2 -d ' ')
        gen=$(echo $line | tr -s ' ' | cut -f3 -d ' ')
        tam=$(echo $line | tr -s ' ' | cut -f4 -d ' ')
        rate=$(echo $line | tr -s ' ' | cut -f5 -d ' ')
    
        echo "=================================================="
        echo "Configuración: N=$n, M=$m, N_GEN=$gen, TAM_POB=$tam, RATE=$rate"
        echo "=================================================="

        for hilos in "${LISTA_HILOS[@]}"; do
            export OMP_NUM_THREADS=$hilos
            echo -n "--> Ejecutando con Hilos = $hilos: "
            
            # Pasamos HILOS=$hilos al Makefile
            make test_parallel N=$n M=$m N_GEN=$gen T_POB=$tam M_RATE=$rate HILOS=$hilos < "${in_file_distances}_${n}_${m}_${gen}_${tam}.txt"
            
            echo " [Guardado]"
        done
    fi
done < $in_file