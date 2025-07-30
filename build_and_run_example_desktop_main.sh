#!/bin/bash

(
  cd ./examples/cpp/desktop-testbed || exit 1
  ./1_build_desktop.sh && ./2_run_desktop.sh
)
