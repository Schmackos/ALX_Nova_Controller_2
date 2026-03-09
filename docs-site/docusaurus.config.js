// @ts-check

const { themes: prismThemes } = require('prism-react-renderer');

/** @type {import('@docusaurus/types').Config} */
const config = {
  title: 'ALX Nova Documentation',
  tagline: 'ESP32-P4 Intelligent Amplifier Controller',
  favicon: 'img/favicon.ico',

  url: 'https://schmackos.github.io',
  baseUrl: '/ALX_Nova_Controller_2/',

  organizationName: 'Schmackos',
  projectName: 'ALX_Nova_Controller_2',

  onBrokenLinks: 'throw',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  markdown: {
    mermaid: true,
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },

  themes: [
    '@docusaurus/theme-mermaid',
    [
      require.resolve('@easyops-cn/docusaurus-search-local'),
      {
        hashed: true,
        language: ['en'],
        highlightSearchTermsOnTargetPage: true,
        searchResultLimits: 8,
      },
    ],
  ],

  presets: [
    [
      'classic',
      /** @type {import('@docusaurus/preset-classic').Options} */
      ({
        docs: {
          // Docs served under /docs/ — landing page at / is handled by src/pages/index.js
          routeBasePath: 'docs',
          sidebarPath: './sidebars.js',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      colorMode: {
        defaultMode: 'dark',
        respectPrefersColorScheme: true,
      },
      navbar: {
        title: 'ALX Nova',
        items: [
          {
            type: 'docSidebar',
            sidebarId: 'userSidebar',
            position: 'left',
            label: 'User Guide',
          },
          {
            type: 'docSidebar',
            sidebarId: 'devSidebar',
            position: 'left',
            label: 'Developer Docs',
          },
          {
            href: 'https://github.com/Schmackos/ALX_Nova_Controller_2',
            label: 'GitHub',
            position: 'right',
          },
        ],
      },
      footer: {
        style: 'dark',
        links: [
          {
            title: 'User Docs',
            items: [
              {
                label: 'Introduction',
                to: '/docs/user/intro',
              },
              {
                label: 'Getting Started',
                to: '/docs/user/getting-started',
              },
              {
                label: 'Troubleshooting',
                to: '/docs/user/troubleshooting',
              },
            ],
          },
          {
            title: 'Developer',
            items: [
              {
                label: 'Overview',
                to: '/docs/developer/overview',
              },
              {
                label: 'Architecture',
                to: '/docs/developer/architecture',
              },
              {
                label: 'Build Setup',
                to: '/docs/developer/build-setup',
              },
            ],
          },
          {
            title: 'Community',
            items: [
              {
                label: 'GitHub',
                href: 'https://github.com/Schmackos/ALX_Nova_Controller_2',
              },
              {
                label: 'Issues',
                href: 'https://github.com/Schmackos/ALX_Nova_Controller_2/issues',
              },
              {
                label: 'Releases',
                href: 'https://github.com/Schmackos/ALX_Nova_Controller_2/releases',
              },
            ],
          },
        ],
        copyright: `Copyright ${new Date().getFullYear()} ALX Nova. Built with Docusaurus.`,
      },
      prism: {
        theme: prismThemes.github,
        darkTheme: prismThemes.dracula,
        additionalLanguages: ['bash', 'cpp', 'json', 'yaml'],
      },
    }),
};

module.exports = config;
