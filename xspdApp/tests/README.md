# ADXSPD Unit Tests

This directory contains unit tests written to cover both the general inteface to the xspd API classes (i.e. those in the `XSPD` namespace), as well as the `ADXSPD` areaDetector driver. The tests are written with [GoogleTest](https://github.com/google/googletest). To build the tests, add `BUILD_TESTS=YES` to the `CONFIG_SITE` or `CONFIG_SITE.local` file in the top level `configure` directory, and build the driver. The tests will be installed in `bin/$ARCH/TestADXSPD`.

The tests rely on example resonse data that is fed through a Mocked API interface class. The example data is produced by running the included `generate_sample_response_json` script while the X-Spectrum provided simulated detector is running (along with the xspd service).