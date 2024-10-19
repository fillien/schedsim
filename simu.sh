#!/bin/sh

for x in $(seq 0 4); do
    for y in $(seq 0 9); do
        if [ "$x" -eq 0 ] && [ "$y" -eq 0 ]; then
            continue
        fi
        if [ "$x" -eq 4 ] && [ "$y" -eq 1 ]; then
            echo "${x}_${y}"
            break 2
        fi

        ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_pa/util_${x}_${y}/1.json --cores --index > data_pa_${x}_${y}.csv
        for i in $(seq 2 100)
        do
            ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_pa/util_${x}_${y}/${i}.json --cores >> data_pa_${x}_${y}.csv
        done

        ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_ffa/util_${x}_${y}/1.json --cores --index > data_ffa_${x}_${y}.csv
        for i in $(seq 2 100)
        do
            ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_ffa/util_${x}_${y}/${i}.json --cores >> data_ffa_${x}_${y}.csv
        done

        ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_csf/util_${x}_${y}/1.json --cores --index > data_csf_${x}_${y}.csv
        for i in $(seq 2 100)
        do
            ./build/schedview/schedview --platform ./platforms/exynos5422LITTLE.json data_umax1_logs_csf/util_${x}_${y}/${i}.json --cores >> data_csf_${x}_${y}.csv
        done
        echo "${x}_${y}"
    done
done
