# Voice Note: Analog devices adc and dac proprietary tooling needed

## Summary
- Analog Devices is an unrepresented chipset manufacturer in the ALX Nova platform
- Analog Devices DSPs require their proprietary Sigma Studio tooling for programming
- Key question: Do Analog Devices ADCs and DACs also require Sigma Studio, or can they be controlled via native I2C commands?
- Cirrus Logic and ESS Sabre devices are referenced as examples of chips that work with standard I2C control

## Cleaned Transcript

One manufacturer with chipsets that we are currently missing in the platform is Analog Devices. I know that Analog Devices uses their proprietary tooling called Sigma Studio that you need to be able to program at least the DSPs. Is Sigma Studio and its proprietary package also needed to use their ADCs and DACs specifically? Or can we run these through native I2C commands like we do with Cirrus Logic and with ESS Sabre?
