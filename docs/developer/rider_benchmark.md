# Rider import benchmark

The `rider_import_benchmark` test exercises the rider importer against a large
synthetic rider file to monitor execution time and memory use. It can be run
manually or through CTest and is intended to provide a reproducible measurement
before and after rider-related optimizations.

## Files involved

- `tests/data/rider_large.txt` – 1,100+ fixture entries distributed across
  multiple hangs and a floor section.
- `tests/rider_import_benchmark.cpp` – small console benchmark that measures
  import duration and process memory usage while reporting the resulting fixture
  and truss counts.

## Building and running

1. Configure the test project (requires the same dependencies as the other
   C++ tests, including wxWidgets and tinyxml2):
   ```bash
   cmake -S tests -B build/tests
   ```
2. Build the benchmark target:
   ```bash
   cmake --build build/tests --target rider_import_benchmark
   ```
3. Run the benchmark with the default large rider file (the second argument is
   the iteration count):
   ```bash
   ./build/tests/rider_import_benchmark tests/data/rider_large.txt 3
   ```
   Or execute it through CTest:
   ```bash
   cd build/tests
   ctest -R RiderImportBenchmark
   ```

## Comparing results

Record the source revision, compiler and build type, host hardware, iteration
count, elapsed time, peak RSS, and imported object counts with each result.
Compare before and after runs on the same provisioned host; this document does
not define a portable timing or memory threshold.
