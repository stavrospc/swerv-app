#!/bin/bash

# USE IN DOCKER

cd /home/swerv-app/
make -s
echo "--- RTL ------------------------------------------------------------------------"
echo "  Executing RTL Simulation:"
./obj_dir/Vtb_top
make -s whisper
echo "--- Whisper --------------------------------------------------------------------"
echo "  Executing Whisper Simulation:"
whisper --configfile snapshots/swerv-app/whisper.json --newlib --target out/mc_smc.exe
echo "--------------------------------------------------------------------------------"
