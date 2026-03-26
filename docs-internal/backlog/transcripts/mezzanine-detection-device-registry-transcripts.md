# Transcripts: mezzanine-detection-device-registry

Extracted from [concept-mezzanine-detection-device-registry.md](../concept-mezzanine-detection-device-registry.md)

## Original Transcripts

<details>
<summary>Source: Mezzanine Detection Setup Methods.m4a</summary>

> We have two ways to auto detect add-on boards that are inserted into the mezzanine slots. The first is the EEPROM approach. The second one is using specific resistor values, a combination of the resistor values. When there is no EEPROM present. The third way is to just set it up manually but that requires more expert users in order to set all the values in the right way for the add-on to properly work. The auto detect is very important to make the set up basically a breeze. Now the drivers that are created either by the ALX platform team or the community. I believe they need to be integrated into the firmware otherwise these cannot be loaded and identified properly. So every add-on board that will ever be created in the future needs their unique identification number in order for the system to properly work. How should we design this in the right way and how can we link this to an online website in this case we could use maybe the docuSaurus that we already have to display the devices that are available with all their specifics components and electrical diagrams etc etc so that it's a full service platform that has everything in place. The other question that I have is is there a need to connect online to get this information or does it always needs to be compiled into the firmware when new devices become available.

</details>

<details>
<summary>Source: Carrier Board Mezzanine Setup online.m4a</summary>

> On the carrier board platform we use two ways for either manufacturers and community members when creating mezzanine plug-in boards to identify auto detection. The first way is using an EEPROM and EEPROM and the second way is using resistor values. Can you create a plan on how this should work and how should we design the online component to this? Because we have our get up repository where we can of course manage all the data but how do we create an easy to use user interface and have the ALX Nova platform connect through the internet to grab the required data when it detects a specific board.

</details>
