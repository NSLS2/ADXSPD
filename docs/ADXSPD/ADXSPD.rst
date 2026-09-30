======================================
ADXSPD
======================================

:author: Jakub Wlodek (Brookhaven National Laboratory)

.. _ADDriver: https://areadetector.github.io/areaDetector/ADCore/ADDriver.html
.. _areaDetector: https://areadetector.github.io/areaDetector/index.html
.. _EPICS: https://epics-controls.org
.. _X-Spectrum: https://x-spectrum.de
.. _Lambda: https://x-spectrum.de/products/lambda/
.. _ADDriverTemplate: https://github.com/NSLS2/ADDriverTemplate
.. _cpr: https://github.com/libcpr/cpr
.. _nlohmann/json: https://github.com/nlohmann/json
.. _magic_enum: https://github.com/Neargye/magic_enum
.. _ZeroMQ: https://zeromq.org
.. _ADLambda: https://github.com/areaDetector/ADLambda

.. contents:: Contents

.. |br| raw:: html

    <br>

Introduction
------------

This is an `EPICS`_ `areaDetector`_ driver for the `Lambda`_ family of photon counting
pixel detectors from `X-Spectrum`_. The driver communicates with the detector through
XSPD, the X-Spectrum detector remote-control service, which exposes a REST API for control and a
`ZeroMQ`_ publisher (the "data port") for streaming frames.

.. note::

   The `ADLambda`_ driver supports the same detector types, but must run on the control
   computer provided by X-Spectrum, since it uses the C++ `libxsp` API directly.
   Because ADXSPD talks to XSPD over the network (with XSPD handling the lower level `libxsp` interactions),
   the IOC can instead run on any other host. This has several benefits:

   * The IOC can run on a standard system configuration, managed the same way as your
     other IOC hosts, rather than on the specific Debian-based server provided by X-Spectrum.
   * The X-Spectrum control computer can be placed on a locked-down, dedicated detector
     network, with only the IOC host able to reach it. This allows for a less strict policy
     regarding software updates and patches on the host, which may improve stability and reduce maintenance overhead.
   * Since all communication between the driver and XSPD is plain HTTP and ZeroMQ, it is
     much simpler to simulate the detector. X-Spectrum provides a simulator for a
     single-module detector as a container image, and this repository includes a
     multi-module simulator in ``scripts/xspdSimulator.py``.

   ADXSPD also currently supports some features that ADLambda does not, including beam
   energy, flatfield correction, bit depth and ROI readout configuration. Adding these to
   ADLambda is tracked in
   `ADLambda issue #16 <https://github.com/areaDetector/ADLambda/issues/16>`__.

The driver is made up of three layers:

* ``XSPDAPI`` - a standalone C++ wrapper around the XSPD REST API. It uses `cpr`_ for HTTP,
  `nlohmann/json`_ for parsing responses, and `magic_enum`_ to map XSPD string values to
  C++ enumerations. It has no dependency on EPICS.
* ``ADXSPD`` - the main areaDetector driver, which inherits from `ADDriver`_. It controls
  detector-wide settings, runs an acquisition thread that reads frames from the ZeroMQ
  data port into NDArrays, and runs a monitor thread that polls detector status.
* ``ADXSPDModule`` - an ``asynPortDriver`` that is created automatically for each detector
  module, and which exposes per-module information such as temperatures, humidity,
  flatfield state and chip IDs.

The driver requires XSPD version 1.6.0 or later. It was initially generated with
`ADDriverTemplate`_.

Acquisition overview
~~~~~~~~~~~~~~~~~~~~

When ``Acquire`` is set to 1, the driver sends the ``start`` command to XSPD. Each frame
published on the data port is a 3-part ZeroMQ message; the driver copies the final part
(the frame data) into an NDArray and passes it to any connected plugins. Acquisition stops
automatically once ``NumImages`` frames have been received (or ``2 * NumImages`` when
``CounterMode`` is ``Dual``). Setting ``Acquire`` to 0 sends the ``stop`` command.

If XSPD is configured to compress frames, the driver can either pass the compressed data
on as a compressed NDArray (with the ``codec`` field set so that ``NDPluginCodec`` or the
HDF5 file plugin can handle it), or decompress the data itself. See
:ref:`ADXSPD_compression` for details.

Implementation of standard driver parameters
--------------------------------------------

The following table describes how the ADXSPD driver implements some of the standard
driver parameters.

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 2
  :widths: 20 20 60

  * - **Parameter Definitions in ADXSPD.cpp and EPICS Record Definitions in ADXSPD.template**
  * - Parameter index variable
    - EPICS record name
    - Description
  * - ADImageMode
    - $(P)$(R)ImageMode
    - Only ``Single`` and ``Multiple`` are supported. ``Continuous`` has been removed from the
      record choices. Selecting ``Single`` sets the XSPD ``n_frames`` variable to 1.
      The image mode is also updated automatically when ``NumImages`` is changed.
  * - ADNumImages
    - $(P)$(R)NumImages
    - Sets the XSPD ``n_frames`` variable. The value must be between 1 and the smallest
      ``MaxFrames_RBV`` reported by any of the detector modules.
  * - ADTriggerMode
    - $(P)$(R)TriggerMode
    - Sets the XSPD ``trigger_mode`` variable. The choices are: |br|
      ``Software`` - acquisition starts immediately. |br|
      ``External Frames`` - each external trigger acquires a single frame. |br|
      ``External Sequence`` - an external trigger starts the full sequence of frames. |br|
      Cannot be changed while acquiring.
  * - ADAcquireTime
    - $(P)$(R)AcquireTime
    - Sets the XSPD ``shutter_time`` variable. The record is in seconds; the driver converts
      to milliseconds for XSPD. Cannot be changed while acquiring.
  * - ADAcquire
    - $(P)$(R)Acquire
    - Sends the ``start`` or ``stop`` command to XSPD.
  * - NDDataType
    - $(P)$(R)DataType
    - Disabled. The data type is set by the driver based on ``BitDepth``: 1 and 6 bit use
      ``UInt8``, 12 bit uses ``UInt16``, and 24 bit uses ``UInt32``.
  * - ADMaxSizeX, ADMaxSizeY, ADSizeX, ADSizeY
    - $(P)$(R)MaxSizeX_RBV, $(P)$(R)MaxSizeY_RBV, $(P)$(R)SizeX_RBV, $(P)$(R)SizeY_RBV
    - Read from the data port ``frame_width`` and ``frame_height`` variables at startup,
      and whenever ``ROIRows`` is changed. The ``SizeX``, ``SizeY``, ``MinX``, ``MinY``,
      ``BinX``, ``BinY``, ``ReverseX``, ``ReverseY`` and ``ColorMode`` output records are
      disabled; use ``ROIRows`` to reduce the readout area.
  * - ADSerialNumber, ADFirmwareVersion, ADSDKVersion, ADModel
    - $(P)$(R)SerialNumber_RBV, $(P)$(R)FirmwareVersion_RBV, $(P)$(R)SDKVersion_RBV, $(P)$(R)Model_RBV
    - Read from XSPD at startup. ``SDKVersion_RBV`` reports the libxsp version.
  * - NDFilePath, NDFileName, NDFullFileName
    - $(P)$(R)FilePath, $(P)$(R)FileName, $(P)$(R)FullFileName_RBV
    - The driver loads ``NDFile.template``, but does not save image files itself. These
      records are used to choose where HTTP request logs are written. See
      :ref:`ADXSPD_request_logging`.

ADXSPD specific parameters
--------------------------

The ADXSPD driver implements the following parameters in addition to those in
``asynNDArrayDriver.h`` and ``ADDriver.h``. Parameters marked "Cannot be changed while
acquiring" are rejected with an error if ``Acquire`` is 1.

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 2
  :widths: 55 15 20 10

  * - **Parameter Definitions in ADXSPDParamDefs.h and EPICS Record Definitions in ADXSPD.template**
  * - Description
    - drvInfo string
    - EPICS record name
    - EPICS record type
  * - :cspan:`3` **Version and system information**
  * - XSPD REST API version in use.
    - XSPD_API_VERSION
    - $(P)$(R)APIVersion_RBV
    - stringin
  * - XSPD server version.
    - XSPD_VERSION
    - $(P)$(R)XSPDVersion_RBV
    - stringin
  * - Number of detector modules. One ``ADXSPDModule`` port is created for each module.
    - XSPD_NUM_MODULES
    - $(P)$(R)NumModules_RBV
    - ai
  * - Sensor material, read from the detector ``user_data/sensor_material`` variable. Empty,
      with a warning printed at startup, if this has not been set in the XSPD user data.
    - XSPD_SENSOR_MATERIAL
    - $(P)$(R)SensorMaterial_RBV
    - stringin
  * - Sensor thickness, read from the detector ``user_data/sensor_thickness`` variable. 0 if
      this has not been set in the XSPD user data.
    - XSPD_SENSOR_THICKNESS
    - $(P)$(R)SensorThickness_RBV
    - ai
  * - :cspan:`3` **Detector settings**
  * - X-ray beam energy in keV (XSPD ``beam_energy``).
    - XSPD_BEAM_ENERGY
    - $(P)$(R)BeamEnergy, $(P)$(R)BeamEnergy_RBV
    - ao, ai
  * - Low energy threshold in keV. Written to XSPD as the first element of ``thresholds``.
    - XSPD_LOW_THRESHOLD
    - $(P)$(R)LowThreshold, $(P)$(R)LowThreshold_RBV
    - ao, ai
  * - High energy threshold in keV. Written to XSPD as the second element of ``thresholds``.
      The low threshold must be set first.
    - XSPD_HIGH_THRESHOLD
    - $(P)$(R)HighThreshold, $(P)$(R)HighThreshold_RBV
    - ao, ai
  * - Counter bit depth. Choices are ``1 bit``, ``6 bit``, ``12 bit`` and ``24 bit``.
      Changing this updates ``DataType`` and each module's ``MaxFrames_RBV``.
      Cannot be changed while acquiring.
    - XSPD_BIT_DEPTH
    - $(P)$(R)BitDepth, $(P)$(R)BitDepth_RBV
    - mbbo, mbbi
  * - Counter mode. Choices are ``Single`` and ``Dual``. In ``Dual`` mode two frames are
      read out per image. Changing this updates each module's ``MaxFrames_RBV`` and
      flatfield state. Cannot be changed while acquiring.
    - XSPD_COUNTER_MODE
    - $(P)$(R)CounterMode, $(P)$(R)CounterMode_RBV
    - bo, bi
  * - Number of rows to read out from each module (XSPD ``roi_rows``). Choices are powers of
      2 from 1 to 256. Changing this updates the frame size and each module's
      ``MaxFrames_RBV``.
    - XSPD_ROI_ROWS
    - $(P)$(R)ROIRows, $(P)$(R)ROIRows_RBV
    - mbbo, mbbi
  * - Number of frames to sum in the detector (XSPD ``summed_frames``).
    - XSPD_SUMMED_FRAMES
    - $(P)$(R)SummedFrames, $(P)$(R)SummedFrames_RBV
    - ao, ai
  * - Enable or disable gating mode (XSPD ``gating_mode``).
    - XSPD_GATING_MODE
    - $(P)$(R)GatingMode, $(P)$(R)GatingMode_RBV
    - bo, bi
  * - Enable or disable charge summing (XSPD ``charge_summing``).
    - XSPD_CHARGE_SUMMING
    - $(P)$(R)ChargeSumming, $(P)$(R)ChargeSumming_RBV
    - bo, bi
  * - Enable or disable flatfield correction (XSPD ``flatfield_correction``).
    - XSPD_F_F_CORRECTION
    - $(P)$(R)FlatFieldCorrection, $(P)$(R)FlatFieldCorrection_RBV
    - bo, bi
  * - Enable or disable count rate correction (XSPD ``countrate_correction``).
    - XSPD_CR_CORR
    - $(P)$(R)CountrateCorrection, $(P)$(R)CountrateCorrection_RBV
    - bo, bi
  * - Enable or disable the saturation flag (XSPD ``saturation_flag``).
    - XSPD_SATURATION_FLAG
    - $(P)$(R)SaturationFlag, $(P)$(R)SaturationFlag_RBV
    - bo, bi
  * - :cspan:`3` **Compression and data handling**
  * - Compressor used by XSPD for data port frames. Choices are ``none``, ``zlib``,
      ``blosc/blosclz``, ``blosc/lz4``, ``blosc/lz4hc``, ``blosc/snappy``, ``blosc/zlib``
      and ``blosc/zstd``. Read-only; configure the compressor in XSPD.
    - XSPD_COMPRESSOR
    - $(P)$(R)Compressor_RBV
    - mbbi
  * - Compression level (0-9) used by XSPD. Read-only.
    - XSPD_COMPRESS_LEVEL
    - $(P)$(R)CompressLevel_RBV
    - mbbi
  * - Blosc shuffle mode. Choices are ``None``, ``Byte Shuffle``, ``Bit Shuffle`` and
      ``Auto``. Cannot be changed while acquiring.
    - XSPD_SHUFFLE_MODE
    - $(P)$(R)ShuffleMode, $(P)$(R)ShuffleMode_RBV
    - mbbo, mbbi
  * - If enabled, compressed frames are decompressed in the driver. If disabled, compressed
      frames are passed on as compressed NDArrays.
    - XSPD_DECOMPRESS
    - $(P)$(R)Decompress, $(P)$(R)Decompress_RBV
    - bo, bi
  * - Number of threads used for Blosc decompression when ``Decompress`` is enabled.
    - XSPD_BLOSC_NUM_THREADS
    - $(P)$(R)BloscNumThreads, $(P)$(R)BloscNumThreads_RBV
    - ao, ai
  * - Number of frames queued on the data port and not yet sent (XSPD ``frames_queued``).
    - XSPD_FRAMES_QUEUED
    - $(P)$(R)FramesQueued_RBV
    - ai
  * - :cspan:`3` **Status monitoring**
  * - Enable or disable the monitor thread, which polls the detector ``status`` and data port
      ``frames_queued`` variables and updates ``DetectorState_RBV`` and ``FramesQueued_RBV``.
    - XSPD_MONITOR_MODE
    - $(P)$(R)MonitorMode, $(P)$(R)MonitorMode_RBV
    - bo, bi
  * - Interval in seconds between monitor thread polls. The minimum is 0.5 seconds.
    - XSPD_MONITOR_INTERVAL
    - $(P)$(R)MonitorInterval, $(P)$(R)MonitorInterval_RBV
    - ao, ai
  * - :cspan:`3` **Diagnostics**
  * - Enable or disable logging of every HTTP request sent to XSPD. See
      :ref:`ADXSPD_request_logging`.
    - XSPD_LOG_REQUESTS
    - $(P)$(R)LogRequests, $(P)$(R)LogRequests_RBV
    - bo, bi
  * - :cspan:`3` **Reserved (records exist but are not yet implemented by the driver)**
  * - Reset the detector.
    - XSPD_RESET
    - $(P)$(R)Reset
    - bo
  * - Generate a new flatfield.
    - XSPD_GENERATE_FLATFIELD
    - $(P)$(R)GenerateFlatfield
    - bo
  * - Flatfield status message.
    - XSPD_FLATFIELD_STATUS
    - $(P)$(R)FlatfieldStatus_RBV
    - waveform
  * - Average board, FPGA and sensor temperatures across all modules.
    - XSPD_AVG_BOARD_TEMP, XSPD_AVG_FPGA_TEMP, XSPD_AVG_SENSOR_TEMP
    - $(P)$(R)AvgBoardTemp_RBV, $(P)$(R)AvgFPGATemp_RBV, $(P)$(R)AvgSensorTemp_RBV
    - ai

ADXSPDModule parameters
-----------------------

One ``ADXSPDModule`` asyn port is created for each detector module, named
``<PORT>_MOD1``, ``<PORT>_MOD2``, etc. Load ``ADXSPDModule.template`` once for each module.
Values are read from XSPD when the IOC starts. Module values are not currently polled by
the monitor thread, because reading module status takes too long.

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 2
  :widths: 55 15 20 10

  * - **Parameter Definitions in ADXSPDModuleParamDefs.h and EPICS Record Definitions in ADXSPDModule.template**
  * - Description
    - drvInfo string
    - EPICS record name
    - EPICS record type
  * - Board, FPGA and humidity sensor temperatures in degrees C.
    - XSPD_BOARD_TEMP, XSPD_FPGA_TEMP, XSPD_HUM_TEMP
    - $(P)$(R)BoardTemp_RBV, $(P)$(R)FPGATemp_RBV, $(P)$(R)HumiditySensorTemp_RBV
    - ai
  * - Relative humidity in %.
    - XSPD_HUM
    - $(P)$(R)Humidity_RBV
    - ai
  * - Sensor high voltage in V. Only the readback is currently implemented.
    - XSPD_VOLTAGE
    - $(P)$(R)Voltage, $(P)$(R)Voltage_RBV
    - ao, ai
  * - Sensor current in uA.
    - XSPD_SENS_CURR
    - $(P)$(R)SensorCurrent_RBV
    - ai
  * - Pixel saturation threshold.
    - XSPD_SAT_THRESH
    - $(P)$(R)SaturationThresh_RBV
    - ai
  * - Module rotation (yaw, pitch, roll) in degrees.
    - XSPD_ROT_YAW, XSPD_ROT_PITCH, XSPD_ROT_ROLL
    - $(P)$(R)RotationYaw_RBV, $(P)$(R)RotationPitch_RBV, $(P)$(R)RotationRoll_RBV
    - ai
  * - Module position (X, Y, Z).
    - XSPD_POS_X, XSPD_POS_Y, XSPD_POS_Z
    - $(P)$(R)PositionX_RBV, $(P)$(R)PositionY_RBV, $(P)$(R)PositionZ_RBV
    - ai
  * - Maximum number of frames the module can buffer with the current settings. This limits
      ``NumImages``, and is refreshed when ``BitDepth``, ``ROIRows`` or ``CounterMode``
      change.
    - XSPD_MAX_FRAMES
    - $(P)$(R)MaxFrames_RBV
    - ai
  * - Number of sub-frames.
    - XSPD_NUM_SUBFRAMES
    - $(P)$(R)NumSubFrames_RBV
    - ai
  * - Number of connectors.
    - XSPD_NUM_CONS
    - $(P)$(R)NumConnectors_RBV
    - ai
  * - Whether pixel interpolation is enabled.
    - XSPD_INTERP_MODE
    - $(P)$(R)InterpolationMode_RBV
    - bi
  * - Bitmask of features supported by the module: |br|
      bit 0 - ``FEAT_HV`` |br|
      bit 1 - ``FEAT_1_6_BIT`` |br|
      bit 2 - ``FEAT_MEDIPIX_DAC_IO`` |br|
      bit 3 - ``FEAT_EXTENDED_GATING`` |br|
      bit 4 - ``FEAT_ROI``
    - XSPD_FEAT_BITMASK
    - $(P)$(R)Features_RBV
    - mbbiDirect
  * - Module compressor and compression level.
    - XSPD_COMPRESSOR, XSPD_COMPRESS_LEVEL
    - $(P)$(R)Compressor_RBV, $(P)$(R)CompressLevel_RBV
    - mbbi
  * - Whether RAM has been allocated on the module.
    - XSPD_RAM_ALLOCATED
    - $(P)$(R)RamAllocated_RBV
    - bi
  * - Whether flatfield correction is enabled on the module.
    - XSPD_FF_ENABLED
    - $(P)$(R)FlatfieldEnabled_RBV
    - bi
  * - Flatfield status message.
    - XSPD_FF_STATUS
    - $(P)$(R)FlatfieldStatus_RBV
    - waveform
  * - Author and date of the low and high threshold flatfields.
    - XSPD_LOW_THRESH_FF_AUTHOR, XSPD_HIGH_THRESH_FF_AUTHOR, XSPD_LOW_THRESH_FF_DATE,
      XSPD_HIGH_THRESH_FF_DATE
    - $(P)$(R)LowThreshFfAuthor_RBV, $(P)$(R)HighThreshFfAuthor_RBV,
      $(P)$(R)LowThreshFfDate_RBV, $(P)$(R)HighThreshFfDate_RBV
    - stringin
  * - Number of frames queued on the module.
    - XSPD_FRAMES_QUEUED
    - $(P)$(R)FramesQueued_RBV
    - ai
  * - Whether the pixel mask is applied. Only the readback is currently implemented.
    - XSPD_PIXEL_MASK
    - $(P)$(R)PixelMask, $(P)$(R)PixelMask_RBV
    - bo, bi
  * - Module shuffle mode. Not currently implemented.
    - XSPD_SHUFFLE_MODE
    - $(P)$(R)ShuffleMode, $(P)$(R)ShuffleMode_RBV
    - mbbo, mbbi
  * - Number of chips on the module, and the ID of each chip (up to 12).
    - XSPD_NUM_CHIPS, XSPD_CHIP1_ID ... XSPD_CHIP12_ID
    - $(P)$(R)NumChips_RBV, $(P)$(R)Chip1ID_RBV ... $(P)$(R)Chip12ID_RBV
    - ai, stringin

.. _ADXSPD_compression:

Compressed data
---------------

XSPD can compress frames before publishing them on the data port. The compressor and
level are shown in ``Compressor_RBV`` and ``CompressLevel_RBV``. The ``Decompress``
record controls how the driver handles compressed frames:

* ``Decompress = Enabled``: the driver decompresses each frame with zlib or Blosc before
  passing it to plugins. Blosc decompression uses ``BloscNumThreads`` threads.
* ``Decompress = Disabled``: the compressed bytes are copied into the NDArray as-is, and the
  NDArray ``codec`` field is set to ``zlib`` or ``blosc`` (with the Blosc sub-compressor,
  level and ``ShuffleMode``). Plugins such as ``NDPluginCodec`` or ``NDFileHDF5`` can then
  decompress or write the data directly. zlib-compressed NDArrays require ADCore R3-15 or
  later.

.. _ADXSPD_request_logging:

HTTP request logging
--------------------

For debugging communication with XSPD, the driver can log every HTTP request it sends.
Each log entry contains a timestamp, the HTTP method, the full URI, the HTTP status code
(0 if no response was received) and the time taken in milliseconds, for example::

    2026-09-30 12:00:00.123 | GET localhost:8008/api/v1/devices/lambda01/variables?path=lambda/status | status=200 | elapsed=1.842 ms

Set ``LogRequests`` to ``Enabled`` to start logging. Where logs are written depends on the
``NDFile.template`` records:

* If ``FilePath`` is empty, logs are written to stdout.
* Otherwise, logs are appended to ``FilePath`` + ``FileName``. If ``FileName`` is empty,
  ``xspd_requests.log`` is used. ``FilePath`` must already exist; enabling logging fails
  if it does not.
* The full path of the log file is shown in ``FullFileName_RBV``.
* Changing ``FilePath`` or ``FileName`` while logging is enabled switches to the new file.
  If the new file cannot be opened, logging is disabled and an error is reported.

Request logging can also be used directly from the ``XSPDAPI`` library with
``XSPD::API::EnableRequestLogging(path)`` and ``XSPD::API::DisableRequestLogging()``.

Configuration
-------------

The ADXSPD driver is created with the ``ADXSPDConfig`` command, either from C/C++ or from
the EPICS IOC shell.

::

    int ADXSPDConfig(const char *portName, const char *ip, int portNum, const char *deviceId)

.. cssclass:: table-bordered table-striped table-hover
.. flat-table::
  :header-rows: 1
  :widths: 20 80

  * - Argument
    - Description
  * - portName
    - Name of the asyn port to create. Module ports are named ``<portName>_MOD1``,
      ``<portName>_MOD2``, etc.
  * - ip
    - Hostname or IP address of the machine running XSPD.
  * - portNum
    - Port number of the XSPD REST API. If 0, the default of 8008 is used.
  * - deviceId
    - Optional. The XSPD device ID to connect to (e.g. ``lambda01``), or a single-digit index
      into the list of devices. If omitted, the first device is used.

Example st.cmd startup file
---------------------------

The following startup script is provided with ADXSPD.

.. literalinclude:: ../../../ADXSPD/iocs/xspdIOC/iocBoot/iocXSPD/st_base.cmd

Screens
-------

Phoebus screens are provided in ``xspdApp/op/bob``:

* ``ADXSPD.bob`` - the main screen for controlling the detector.
* ``ADXSPD_Module.bob`` - per-module status and information.

Auto-generated screens covering every record in the database templates are in
``xspdApp/op/bob/autogenerated``.

Building
--------

ADXSPD depends on the following, in addition to EPICS base, asyn and ADCore:

* `cpr`_, `nlohmann/json`_ and `magic_enum`_, which are included in ``xspdSupport``.
  ``cpr`` requires libcurl; set ``CURL_EXTERNAL=YES`` in ``configure/CONFIG_SITE`` to use
  the system libcurl.
* `ZeroMQ`_ (``libzmq``). Set ``ZMQ_LIB`` to use a non-system build.
* zlib and Blosc, which can be provided by ADSupport or the system.

A C++17 compiler is required.

Testing
-------

Unit tests for the API are in ``xspdApp/tests`` and use GoogleTest and GoogleMock. Set
``BUILD_TESTS=YES`` in ``configure/CONFIG_SITE`` to build them, then run::

    ./xspdApp/tests/O.linux-x86_64/TestADXSPD

A simulator of the XSPD REST API and data port is provided in
``scripts/xspdSimulator.py``, and can be started with::

    pixi run simulator
