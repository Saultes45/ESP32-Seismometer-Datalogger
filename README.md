# ESP32-Seismometer-Datalogger
 
TODO: Insert description of the project here


Questions (project manager):
----------

// important

1. Productions: should we spend time ordereing and preparing the w/ the "SMT assembly" or solder by oursleft (probably have already most of the components) SMT assembly.

1. Should we keep track of the bumps during LOG and do a LOG-LOG transition? If yes, do we wait for GPS fix if not prev?
--> LOG LOG and switch of the GPS, use internal timer

1. How much time should we wait for GPS fix? (gps start)
--> don't know

// less important


1. What do we do with the battery voltage?
--> put at the end of the log file
