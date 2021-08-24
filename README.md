# RRTGen

The RRTGen directory doesn’t build alone. 

For example, “state”, “event, and “MAX_NAME_LENGTH” are missing when trying 
to build RRTGen by itself.  

These items required by RRTGen are provided elsewhere, namely by the 
test application (e.g., Elevator Verification Test) code.

The Makefile in this directory compiles the RRTGen components:  
    RRandom.o, and
    RRTGen.o

when called from the test application build, with the required header files
containing "state", "event", etc., avaiable for RRTGen's build.

Then RRTGen is incorporated in a test application by including the RRTGen 
components (object files RRandom.o and RRTGen.o) in the test application build.
