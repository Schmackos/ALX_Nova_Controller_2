# Voice Note: Lower bar for Analog Devices DSP

## Summary
- Investigating whether Analog Devices DSPs (ADAU series) can be driven natively from the ALX Nova platform without requiring SigmaStudio
- Key question: has SigmaStudio's proprietary tooling been reverse-engineered enough to embed basic DSP programming into the platform?
- Goal: remove the need for SigmaStudio for standard users, similar to how ESS Sabre and Cirrus Logic chips are already integrated
- Potential differentiation: basic DSP configuration handled natively by the platform, power users may still need SigmaStudio for advanced use cases
- This would be a strong value proposition for ALX Nova — eliminating proprietary tools and expert knowledge requirements

## Cleaned Transcript

For the DSPs from Analog Devices, I know that you need proprietary tooling in order to use these. It's called SigmaStudio. Is there already data available on how to circumvent this tooling? Is it already reverse-engineered so that we can include a reverse-engineered version of SigmaStudio into the platform, allowing you to drive the Analog Devices DSPs natively from our carrier board platform solution?

This would remove the need for SigmaStudio, at least for standard applications and normal users. There might be a differentiation between the two — for power users, research whether you still need SigmaStudio. But can we do the basic functionality that we also do with the existing manufacturers like ESS Sabre and Cirrus Logic that we have already implemented into the platform?

So my main question, as a summary: do we need SigmaStudio in order to program these chips, or can we create software in our platform to drive these natively, removing the need for SigmaStudio and the complex setup that requires expert knowledge to leverage these? This would make it very interesting to use the ALX Nova platform because it would remove the need for these proprietary tools and the significant knowledge required to set them up properly.
