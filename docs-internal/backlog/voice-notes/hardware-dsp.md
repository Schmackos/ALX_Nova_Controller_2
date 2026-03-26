# Voice Note: Hardware DSP

## Summary
- The platform currently integrates ADCs and DACs but is missing hardware DSP support
- A software DSP implementation already exists and works with the ADC/DAC pipeline
- Future expansion should allow integrating dedicated DSP chips (e.g., Analog Devices ADAU1467) as the main processing units
- This introduces a selection criteria: software DSP vs. hardware DSP from a proprietary chip
- Need to evaluate the benefits of hardware DSP capability for both community/end users and enterprise/professional users

## Cleaned Transcript

In the platform right now we have ADCs (analog-to-digital converters) and DACs (digital-to-analog converters) integrated, but what we are still missing is the DSP component. We have our native software DSP that we created in order to work with these ADCs and DACs.

Now, as a future expansion of the platform, it would be great if we can integrate DSP chips and use these as the main DSP processing units. This would introduce a selection criteria within the platform — either you use the software implementation that we have, or you use the native implementation from a proprietary chip, for example the Analog Devices ADAU1467 or similar.

Next to this: what benefits would this bring by introducing a hardware capability for DSP, and would it be beneficial to our end users in the community or our professional users in the enterprise to leverage a feature like this?
