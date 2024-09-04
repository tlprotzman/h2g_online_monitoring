# H2GROC online monitoring

## Requirements
ROOT 6

## Installation
Compile with `make`.  A standalone version can be compiled with `make exec`, though I haven't particularly tested it in this mode.

## Running
One environment variable must be set.
* `DATA_DIRECTORY`: The path to the folder containing runs of the form `run_xyz.h2g`
Optionally, the port can be specified as well.
* `MONITORING_PORT`:  The port the webserver is hosted on

Monitoring is started with the ROOT macro `RunMonitoring.C`, which takes the run number as an argument.  In the macro, a config file is specified, defining the number of samples and the detector type.  This macro starts the monitoring code which will read the output file as it comes in and starts a webserver to view QA plots.  Upon receiving a stop signal (`ctrl+c`), the process will save all canvases as well as a ROOT file containing all histograms.

To connect to the webserver, open an ssh tunnel to the monitoring computer with `ssh -L 12345:localhost:12345 user@computer`, replacing the port if a different one is used.  From a web browser you can navigate to `localhost:12345` to view the plots.

On the webpage, to allow the plots to update as new data is processed make sure the `Monitoring` checkbox in the top left is checked.