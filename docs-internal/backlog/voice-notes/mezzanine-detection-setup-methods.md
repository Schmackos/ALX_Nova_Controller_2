# Voice Note: Mezzanine Detection Setup Methods

## Summary
- Three tiers of add-on board detection exist: EEPROM auto-detect, resistor-value identification (fallback), and manual configuration (expert users)
- Auto-detection is critical for user-friendly setup experience
- Community and first-party drivers must be compiled into firmware — runtime loading isn't supported
- Every add-on board needs a unique identification number in a central registry
- Proposal to use the existing Docusaurus site as a device catalog with specs, components, and electrical diagrams
- Open question: can device definitions be fetched online at runtime, or must they always be compiled into firmware?

## Cleaned Transcript

We have two ways to auto-detect add-on boards that are inserted into the mezzanine slots. The first is the EEPROM approach. The second one is using specific resistor values — a combination of resistor values when there is no EEPROM present. The third way is to just set it up manually, but that requires more expert users in order to set all the values in the right way for the add-on to properly work. Auto-detect is very important to make the setup a breeze.

Now, the drivers that are created — either by the ALX platform team or the community — I believe they need to be integrated into the firmware, otherwise these cannot be loaded and identified properly. So every add-on board that will ever be created in the future needs a unique identification number in order for the system to properly work.

How should we design this in the right way? And how can we link this to an online website? In this case, we could maybe use the Docusaurus site that we already have to display the devices that are available, with all their specific components, electrical diagrams, et cetera — so that it's a full-service platform that has everything in place.

The other question I have is: is there a need to connect online to get this information, or does it always need to be compiled into the firmware when new devices become available?
