# Voice Note: Carrier Board Mezzanine Setup online

## Summary
- Mezzanine boards use two auto-detection methods: EEPROM and resistor values
- Need a plan for how both detection mechanisms work at the hardware/firmware level
- Need an online component — a registry/database hosted via the GitHub repository
- The ALX Nova platform should connect to the internet to fetch board data when it detects a new mezzanine
- Need an easy-to-use UI for manufacturers and community members to register their boards
- Key design question: how to bridge local detection → online lookup → device configuration

## Cleaned Transcript
On the carrier board platform, we use two ways for either manufacturers or community members creating mezzanine plug-in boards to identify via auto-detection. The first way is using an EEPROM, and the second way is using resistor values.

Can you create a plan on how this should work and how we should design the online component to this? Because we have our GitHub repository where we can of course manage all the data, but how do we create an easy-to-use user interface and have the ALX Nova platform connect through the internet to grab the required data when it detects a specific board?
