# ESP32-Seismometer-Datalogger
 
TODO: Insert description of the project here


Questions (project manager):
----------

// important

1. Productions: should we spend time ordereing and preparing the w/ the "SMT assembly" or solder by oursleft (probably have already most of the components)

1. Should we keep track of the bumps during LOG and do a LOG-LOG transition? If yes, do we wait for GPS fix if not prev?

1. How much time should we wait for GPS fix?

1. Problem with both RTC + GPS on I2C lines 

1. Can we use only RMC for GPS time  (YES)

1. Do we need the PPS for MS log (NO)

1. Can we turn the datalogger featherwing off? via the 3V pin or Vin or BAT or USB (YES)

// less important

1. Power off the RS1D (warm up): let's see if we can spot the WU profile

1. Do we need the RTC (YES, as a backup)

1. What do we do with the battery voltage?
