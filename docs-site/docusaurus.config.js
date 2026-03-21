// @ts-check

const { themes: prismThemes } = require('prism-react-renderer');
const remarkMath = require('remark-math').default;
const rehypeKatex = require('rehype-katex').default;
const latestVersion = process.env.LATEST_RELEASE_VERSION || null;

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
          showLastUpdateTime: true,
          editUrl: 'https://github.com/Schmackos/ALX_Nova_Controller_2/edit/main/docs-site/',
          remarkPlugins: [remarkMath],
          rehypePlugins: [rehypeKatex],
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      }),
    ],
  ],

  stylesheets: [
    {
      href: 'https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css',
      type: 'text/css',
      integrity: 'sha384-n8MVd4RsNIU0tAv4RzvK/7ptJjBhJMtzsSbiXTkl8YQsBu/eRCe35YgEL09Q0D5',
      crossorigin: 'anonymous',
    },
  ],

  clientModules: [require.resolve('./src/zoom.js')],

  plugins: [
    [
      '@docusaurus/plugin-pwa',
      {
        debug: false,
        offlineModeActivationStrategies: ['appInstalled', 'standalone', 'queryString'],
        pwaHead: [
          { tagName: 'link', rel: 'icon', href: '/ALX_Nova_Controller_2/img/favicon.ico' },
          { tagName: 'link', rel: 'manifest', href: '/ALX_Nova_Controller_2/manifest.json' },
          { tagName: 'meta', name: 'theme-color', content: '#FF9800' },
          { tagName: 'meta', name: 'apple-mobile-web-app-capable', content: 'yes' },
          { tagName: 'meta', name: 'apple-mobile-web-app-status-bar-style', content: 'default' },
          { tagName: 'meta', name: 'apple-mobile-web-app-title', content: 'ALX Nova Docs' },
        ],
      },
    ],
  ],

  themeConfig:
    /** @type {import('@docusaurus/preset-classic').ThemeConfig} */
    ({
      ...(latestVersion && {
        announcementBar: {
          id: `firmware_release_${latestVersion.replace(/[^a-zA-Z0-9]/g, '_')}`,
          content: `🎉 ALX Nova ${latestVersion} is out! <a href="/ALX_Nova_Controller_2/docs/user/ota-updates">Update via OTA</a> &nbsp;·&nbsp; <a href="https://github.com/Schmackos/ALX_Nova_Controller_2/releases" target="_blank" rel="noopener noreferrer">Release notes ↗</a>`,
          backgroundColor: '#1a1a1a',
          textColor: '#FF9800',
          isCloseable: true,
        },
      }),
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
