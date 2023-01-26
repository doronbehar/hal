#!/bin/bash
cd $1
./contrib/setup-cadical.sh
./contrib/setup-btor2tools.sh
./contrib/setup-symfpu.sh
./configure.sh --prefix $2 --shared --no-testing
