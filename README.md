# SSSIM

This is SSSim 0.5  
    added improved logging and Kalman filter calculation for location
    estimates.

The first public release of the Sea Swarm Simulator,
originally developed by Eemeli Aro at Aalto University.

The simulator is released subject to the terms of the Mozilla Public
License, v. 2.0. A copy of the MPL should be included as a LICENSE
file, or you can obtain one at http://mozilla.org/MPL/2.0/.

For more information and the latest version, please visit:

    http://autsys.tkk.fi/sssim


WHY?
====
SSSim has been released primarily to help others developing multi-robot
simulators for research purposes; especially for developing underwater
systems. It is a tool I developed to enable my own research on an
autonomous group of robotic floats, and that shows in its implementation
details. I hope it may prove useful to others, as it is extensible to handle 
different types of robots and different types of 2D and 3D environments.


INSTALLATION
============
Development work has been done using Ubuntu Linux, so exact library & other
requirements on other platforms is unknown.

Under Ubuntu (currently 18.04 in use), the following command should install 
a minimal set of required libraries:

```bash
sudo apt-get install build-essential 
sudo apt-get install libnetcdf-dev 
sudo apt-get install libnetcdf-cxx-legacy-dev
sudo apt-get install freeglut3-dev 
sudo apt-get install libgd-dev 
sudo apt-get install libconfig++-dev
```

Additionally, the Simple Vector Library is required, and as it is not
available via APT it is included with the simulator. To install:

```bash
cd lib-src/svl-1.5/
make
sudo make install
```


USAGE
=====
Once the necessary libraries are installed, running `make` should generate
five executable files in the bin/ directory:
@
    env - the environment server
    float - sample underwater robotic float
    base - sample base station
    cli - command line controller for the environment
    gui - a combined visualizer and controller for the system

To run the simulator, a number of the above programs need to be run at the
same time. The shell script "run" is provided, which uses gnome-terminal to
start the environment, a base station, six floats, and the GUI. The sample
floats will operate on a two-hour cycle, meeting at the sound channel depth to
communicate before picking a random depth to float at. The sample base station
will act as a GPS position data relay; the GUI will display these last known
locations for the active float.

