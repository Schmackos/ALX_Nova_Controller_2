# Transcripts: hardware-dsp-integration

Extracted from [concept-hardware-dsp-integration.md](../concept-hardware-dsp-integration.md)

## Original Transcripts

<details>
<summary>Source: Hardware DSP.m4a</summary>

> In the platform right now we have ADCs like analog to digital converters and docs digital to analog converters integrated but we are still missing is the DSP component. We have our native DSP software DSP that we created in order to work with these ADCs and docs. Now as a future expansion of the platform it would be great if we can integrate DSP chips and use these as the main DSP processing units. This would introduce a selection criteria within the platform that either you use the software implementation that we have or you use the native implementation from a proprietary chip from for example analog devices the Adal 1467 or a like. Next to this what benefits would this bring by introducing a hardware capability for DSP and would it be beneficial to our end users in the community or our professional users in the enterprise to leverage a feature like this.

</details>

<details>
<summary>Source: Analog devices adc and dac proprietary tooling needed.m4a</summary>

> One manufacturer with chipsets that we are missing currently in the platform is analog devices. I know that analog devices uses their proprietary tooling called Sigma Studio that I think you need to be able to program at least the DSPs. Is this Studio and proprietary package also needed to use their ADCs and docs specifically? Or can we run these through native I2C commands like we do with Serious Logic and with ESS Saber?

</details>

<details>
<summary>Source: Lower bar for Analog Devices DSP.m4a</summary>

> For the DSPs from analog devices, I know that you need proprietary tooling in order to use these. It's called Sign Studio. Is there already data available in how to circumvent these tooling? Is it already reverse engineered so that we can basically include a reverse engineered version of Sign Studio into the platform so that you can use the platform to drive the analog devices DSP natively from our carrier board platform solution? This would remove the need for a Sign Studio at least for standard application and normal users. So there might be a differentiation between the two. So for power users research if you then still need the studio, but can we do the basic bit that we also do with the existing manufacturers like E is a Saber and Serious Logic that we have already implemented into the platform? So my main question as a summary, do we need Sign Studio in order to program these chips? Or can we create software in our platform to drive these natively, removing the need for Sign Studio and complex setup that require expert knowledge in order to leverage these? This would make it very interesting to use the ALX Nova platform because it would remove the need for these proprietary tools and the significant knowledge to set them up properly.

</details>