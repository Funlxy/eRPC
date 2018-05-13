#!/usr/bin/env bash
# Run an eRPC app on this machine. This script must be run from eRPC homedir.
source $(dirname $0)/utils.sh

assert_file_exists scripts/autorun_app_file
export autorun_app=`cat scripts/autorun_app_file`

assert_file_exists build/$autorun_app
chmod +x build/$autorun_app # Fix permissions messed up by lsyncd

export MLX4_SINGLE_THREADED=1
export MLX5_SINGLE_THREADED=1
export MLX5_SHUT_UP_BF=0
export MLX_QP_ALLOC_TYPE="HUGE"
export MLX_CQ_ALLOC_TYPE="HUGE"

# Install faster modded driver. This is not a requirement.
# XXX: Auto-choose between libmlx5 and libmlx4
blue "Installing modded driver"
./drivers/libmlx5-41mlnx1/update-driver.sh

# Check arguments
if [ "$#" -gt 3 ] || [ "$#" -lt 2 ]; then
  blue "Illegal args. Usage: do.sh [process_id] [NUMA node] <gdb>"
	exit
fi

epid=$1
numa_node=$2

# Non-GDB mode
if [ "$#" -eq 2 ]; then
  blue "do.sh: Launching process $epid on NUMA node $numa_node"

  sudo -E numactl --cpunodebind=$numa_node --membind=$numa_node \
    ./build/$autorun_app $(cat apps/$autorun_app/config) \
    --process_id $epid --numa_node $numa_node
fi

# GDB mode
if [ "$#" -eq 3 ]; then
  blue "do.sh: Launching process $epid with GDB"
  sudo -E gdb -ex run --args \
    ./build/$autorun_app $(cat apps/$autorun_app/config) \
    --process_id $epid --numa_node $numa_node
fi
