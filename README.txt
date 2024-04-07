Cambridge MRes Project titled "Assessing high-performance lightweight compression formats for Geospatial computation".

Main programs:
* test_comp.cpp - test codecs
* bench_comp.cpp - benchmark codecs
* bench_pipeline.cpp - benchmark geospatial pipelines

All codecs are implemented in the `codecs` directory.

Additional:
* test_remappings.cpp - verifies Morton remappings
* util.h, transformations.h, remappings.h - CPP utilities
* py/* - Python utilities

Setup:
1. obtain the submodules in `external` and build them
2. `make`
