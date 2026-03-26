# Concept: Documentation & Device Catalog

| Field | Value |
|---|---|
| Workflow | `draft` |
| Priority | `---` |
| Effort | `---` |
| Sources | Docs and devices overview.m4a, Docs and oitnof box devices.m4a |
| Last updated | 2026-03-26 |

## Problem / Opportunity

Users currently have no single-page reference for supported chipsets within the ALX Nova documentation, forcing them to search externally for datasheets, pinouts, and integration details. By consolidating chip information, datasheet links, and project-relevant details into dedicated documentation pages — and prominently showcasing supported hardware on the landing page — we create a key differentiator that reduces friction for users evaluating or building with the platform.

## Sub-topics

### Per-chipset documentation pages with datasheets

#### What we know
- Each supported chipset (ADCs, DACs, etc.) should have its own dedicated documentation page
- Pages must include a visual image/link of the chip itself
- Each page must link directly to the manufacturer's official datasheet
- Goal is a single-page reference per chip so users don't need to search externally
- All project-relevant details (chip info, datasheet, integration notes) consolidated together

#### What needs research
- Which chipsets currently have sufficient documentation to populate pages (ESS ES8311, ES9219, ES9023, ES9028PRO, ES9038PRO, Cirrus CS43131, CS43198, etc.)
- What project-specific integration details should each page include (supported sample rates, DSD capability, HAL driver pattern used, mezzanine compatibility)
- Whether to host datasheet PDFs locally or link to manufacturer pages (licensing/copyright considerations)
- Page layout and template design — what fields and sections each chipset page should contain
- How to source and license chip product images for each page

### Supported hardware showcase on landing page

#### What we know
- Supported chipsets and manufacturers should be a prominent differentiator on the Docusaurus landing page
- Must clearly list out-of-the-box supported hardware (ESS, Cirrus Logic, etc.)
- Each manufacturer/chipset listing should link to the relevant documentation page

#### What needs research
- How to visually present the manufacturer/chipset grid on the landing page (logo grid, card layout, table)
- Whether to organize by manufacturer, device type (ADC/DAC/codec), or both
- How to keep the landing page showcase in sync as new chipsets are added
- Whether manufacturer logos require permission or licensing for use on the docs site

## Action Items

- [ ] Create a chipset documentation page template in `docs-site/docs/developer/hal/devices/` with standardized sections: chip name, manufacturer, type (ADC/DAC/codec), product image, datasheet link, supported sample rates and bit depths, DSD support, HAL driver pattern used, I2C address, mezzanine slot compatibility, and any known limitations. Use the existing device DB in `src/hal/hal_device_db.cpp` as the source of truth for supported devices. Write the template to `docs-site/docs/developer/hal/devices/_template.md`.
- [ ] Generate individual chipset documentation pages for all 26 expansion devices currently in the HAL device database. Extract chip details from `src/hal/hal_device_db.cpp` and the four generic driver patterns (A/B/C/D) in `src/hal/`. Create one `.md` file per chipset family in `docs-site/docs/developer/hal/devices/` (e.g., `es9038pro.md`, `cs43198.md`). Include datasheet links from manufacturer websites (ESS Technology, Cirrus Logic). Add pages to the `devSidebar` in `docs-site/sidebars.js`.
- [ ] Add a "Supported Hardware" showcase section to the Docusaurus landing page (`docs-site/src/pages/index.js` or equivalent). Display supported manufacturers (ESS Technology, Cirrus Logic) and chipset families in a visually prominent grid or card layout. Each entry should link to the corresponding chipset documentation page. Use the existing design token pipeline for consistent styling.
- [ ] Research manufacturer logo and product image licensing for ESS Technology and Cirrus Logic. Determine whether product images and logos can be used on the documentation site under fair use or if explicit permission is needed. Document findings in `docs-internal/backlog/research/chipset-image-licensing.md`.

## Original Transcripts

<details>
<summary>Source: Docs and devices overview.m4a</summary>

> In our documentation section I want to make sure that we have all the documentation for the supported chipsets. So for the docs, for the ADCs and everything else. So there needs to be a visual link to the chip itself and also the data sheet that is provided by the manufacturer for easy reference. And this will always make sure that we keep keep it keep everything together so that when a user looks it up they basically have everything in one page and they don't need to scroll around or do other lookups on the internet to find what they need for their specific project.

</details>

<details>
<summary>Source: Docs and oitnof box devices.m4a</summary>

> In the docks section and on our landing page in the docks you sawers one I want to make it make it very clear as a key differentiator what chipsets and manufacturers we provide out of the box also with a link to

</details>
