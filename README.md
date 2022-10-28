# CAB403-parking

## Running anything

Due to Make being smart and reusing object files that already exist, if you only change a header file, just need to
run `make clean` and then `make` to ensure object files are recompiled.

The Makefile should be set up so that any C files in `src` are compiled individually on `make` or `make main`,
and tests are compiled on `make test`. If running on linux need to uncomment `LDFLAGS` in the makefile if not already, should be labelled in there.

To run, binaries are built to `./build/bin/{c_file_no_ext}`. Run from the base folder so plates.txt is loaded.

## libs

&rarr; For any utility functions to be included in the programs

## src

&rarr; For the main programs

- Manager
- Simulator
- Fire Alarm

## test

&rarr; Testing. Hashtable_test.c contains a pretty good (imo) main function to copy for testing (if you want to test anything cause something is breaking, don't think tests are actually required)
