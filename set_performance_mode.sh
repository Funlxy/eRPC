#!/bin/bash

for cpu in /sys/devices/system/cpu/cpu[0-9]*; do
  cpu_num=$(basename $cpu)
  echo "Setting performance mode for $cpu_num"
  sudo cpufreq-set -c ${cpu_num#cpu} -g performance
done

