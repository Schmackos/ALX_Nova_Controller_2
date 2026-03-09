// @ts-check

/** @type {import('@docusaurus/plugin-content-docs').SidebarsConfig} */
const sidebars = {
  userSidebar: [
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
        'user/wifi-configuration',
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
        'developer/api/rest-dac',
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
      ],
    },
    'developer/audio-pipeline',
    'developer/dsp-system',
    'developer/testing',
    'developer/contributing',
  ],
};

module.exports = sidebars;
