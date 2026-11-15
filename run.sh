#!/bin/sh
set -e
cd "$(dirname "$0")"
mkdir -p data
make
./movie_booking
