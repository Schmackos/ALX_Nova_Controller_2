// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  userSidebar: [
    'about',
    'user/intro',
    {
      type: 'category',
      label: 'Getting Started',
      items: ['user/getting-started'],
    },
    {
      type: 'category',
      label: 'Features',
      items: [
        'user/web-interface',
        'user/smart-sensing',
        {
          type: 'doc',
          id: 'user/wifi-configuration',
          label: 'Network Configuration',
        },
        'user/mqtt-home-assistant',
        'user/ota-updates',
        'user/button-controls',
      ],
    },
    'user/troubleshooting',
  ],

  devSidebar: [
    'developer/overview',
    {
      type: 'category',
      label: 'Architecture',
      items: [
        'developer/architecture',
        'developer/build-setup',
      ],
    },
    {
      type: 'category',
      label: 'API Reference',
      items: [
        'developer/api/rest-main',
        'developer/api/rest-hal',
        'developer/api/rest-dsp',
        'developer/api/rest-pipeline',
        'developer/api/rest-eeprom',
        'developer/api/rest-health',
        'developer/api/versioning-policy',
        'developer/websocket',
      ],
    },
    {
      type: 'category',
      label: 'HAL Framework',
      items: [
        'developer/hal/overview',
        'developer/hal/device-lifecycle',
        'developer/hal/driver-guide',
        'developer/hal/drivers',
        'developer/hal/mezzanine-connector',
        'developer/hal/power-supply',
      ],
    },
    {
      type: 'category',
      label: 'User Flows',
      items: [
        'developer/user-flows/overview',
        'developer/user-flows/mezzanine-adc-insert',
        'developer/user-flows/mezzanine-dac-insert',
        'developer/user-flows/mezzanine-removal',
        'developer/user-flows/manual-configuration',
        'developer/user-flows/custom-device-creation',
        'developer/user-flows/device-toggle',
        'developer/user-flows/device-reinit',
        'developer/user-flows/matrix-routing',
        'developer/user-flows/dsp-peq-config',
        'developer/user-flows/first-boot',
      ],
    },
    'developer/audio-pipeline',
    'developer/dsp-system',
    {
      type: 'category',
      label: 'Testing',
      items: [
        'developer/testing/overview',
        'developer/testing/device-testing',
        'developer/testing/ci-device-tests',
      ],
    },
    'developer/contributing',
  ],

  enterpriseSidebar: [
    'enterprise/overview',
    'enterprise/oem-integration',
    'enterprise/production-deployment',
    'enterprise/certification',
    'enterprise/support-tiers',
  ],
};

module.exports = sidebars;
