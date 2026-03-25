import React from 'react';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import styles from './index.module.css';

// ---------------------------------------------------------------------------
// Animated waveform visual
// ---------------------------------------------------------------------------

const WAVEFORM_HEIGHTS = [35, 65, 45, 80, 55, 40, 70, 30, 75, 50, 85, 45, 60, 35, 90, 55, 40, 70, 45, 80, 30, 65, 50, 85, 40, 60, 75, 35, 55, 70, 45, 80];

function AnimatedWaveform() {
  return (
    <div className={styles.waveformVisual} aria-hidden="true">
      {WAVEFORM_HEIGHTS.map((h, i) => (
        <div
          key={i}
          className={styles.waveformBar}
          style={{ '--delay': `${(i * 0.09) % 1.5}s`, '--bar-height': `${h}%` }}
        />
      ))}
    </div>
  );
}

// ---------------------------------------------------------------------------
// Inline SVG icons (Material Design Icons, viewBox 0 0 24 24)
// ---------------------------------------------------------------------------

function IconOpenLock() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-lock-open-variant-outline */}
      <path d="M18 1C16.34 1 15 2.34 15 4V9H4C2.9 9 2 9.9 2 11V20C2 21.1 2.9 22 4 22H16C17.1 22 18 21.1 18 20V11C18 10.63 17.9 10.29 17.72 10H18C19.1 10 20 9.1 20 8V4C20 2.34 18.66 1 18 1M10 18C8.9 18 8 17.1 8 16C8 14.9 8.9 14 10 14C11.1 14 12 14.9 12 16C12 17.1 11.1 18 10 18M18 8H17V4C17 3.45 17.45 3 18 3C18.55 3 19 3.45 19 4V8H18Z" />
    </svg>
  );
}

function IconChip() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-chip */}
      <path d="M6,6H18V18H6V6M14,8H10V10H8V14H10V16H14V14H16V10H14V8M10,10H14V14H10V10M2,8V10H4V14H2V16H4V17A1,1 0 0,0 5,18H6V20H8V18H10V20H14V18H16V20H18V18H19A1,1 0 0,0 20,17V16H22V14H20V10H22V8H20V7A1,1 0 0,0 19,6H18V4H16V6H14V4H10V6H8V4H6V6H5A1,1 0 0,0 4,7V8H2Z" />
    </svg>
  );
}

function IconRocket() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-rocket-launch-outline */}
      <path d="M13.13 22.19L11.5 18.36C13.07 17.78 14.54 17 15.9 16.09L13.13 22.19M5.64 12.5L1.81 10.87L7.91 8.1C7 9.46 6.22 10.93 5.64 12.5M21.61 2.39C21.61 2.39 16.66 .269 11 5.93C8.81 8.12 7.5 10.53 6.65 12.64C6.37 13.39 6.56 14.21 7.11 14.77L9.24 16.89C9.79 17.45 10.61 17.63 11.36 17.35C13.5 16.53 15.88 15.19 18.07 13C23.73 7.34 21.61 2.39 21.61 2.39M14.54 9.46C13.76 8.68 13.76 7.41 14.54 6.63S16.59 5.85 17.37 6.63C18.14 7.41 18.15 8.68 17.37 9.46C16.59 10.24 15.32 10.24 14.54 9.46M8.88 16.53L7.47 15.12L8.88 16.53M6.24 22L9.88 18.36C9.54 18.27 9.21 18.12 8.91 17.91L4.83 22H6.24M2 22H3.41L8.18 17.24L6.76 15.83L2 20.59V22M2 19.17L6.09 15.09C5.88 14.79 5.73 14.46 5.64 14.12L2 17.76V19.17Z" />
    </svg>
  );
}

function IconEqualizer() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-equalizer */}
      <path d="M18,2A2,2 0 0,1 20,4V20A2,2 0 0,1 18,22H6A2,2 0 0,1 4,20V4A2,2 0 0,1 6,2H18M11,16V18H13V16H11M11,12V14H13V12H11M11,8V10H13V8H11M7,16V18H9V16H7M7,12V14H9V12H7M7,8V10H9V8H7M15,16V18H17V16H15M15,12V14H17V12H15M15,8V10H17V8H15Z" />
    </svg>
  );
}

function IconWifi() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-access-point-network */}
      <path d="M12,21L15.6,16.2C14.6,15.45 13.35,15 12,15C10.65,15 9.4,15.45 8.4,16.2L12,21M12,3A9,9 0 0,0 3,12C3,14.61 4.03,16.97 5.71,18.71L7.13,17.29C5.82,15.95 5,14.07 5,12A7,7 0 0,1 12,5A7,7 0 0,1 19,12C19,14.07 18.18,15.95 16.87,17.29L18.29,18.71C19.97,16.97 21,14.61 21,12A9,9 0 0,0 12,3M12,9A3,3 0 0,0 9,12A3,3 0 0,0 12,15A3,3 0 0,0 15,12A3,3 0 0,0 12,9Z" />
    </svg>
  );
}

function IconFactory() {
  return (
    <svg viewBox="0 0 24 24" width="36" height="36" fill="currentColor" aria-hidden="true">
      {/* mdi-factory */}
      <path d="M10 2H14V3H19V5H5V3H10V2M5 6H19L18 20H6L5 6M8 8V18H10V8H8M14 8V18H16V8H14Z"/>
    </svg>
  );
}

// ---------------------------------------------------------------------------
// Feature card data
// ---------------------------------------------------------------------------

const features = [
  {
    title: 'No Vendor Lock-in',
    description:
      'Mix and match certified hardware modules from any manufacturer. Replace components without rewriting code — the HAL keeps everything compatible.',
    Icon: IconOpenLock,
    to: '/docs/about',
  },
  {
    title: 'Modular by Design',
    description:
      'Hot-pluggable ADC, DSP, DAC, and amplifier components with I2C auto-discovery. Build the exact stack you need — nothing more.',
    Icon: IconChip,
    to: '/docs/developer/hal/overview',
  },
  {
    title: 'Prototype to Production',
    description:
      'Start on the ESP32-P4 dev kit, ship on a custom board. The same firmware scales from tinkerer bench to commercial audio product.',
    Icon: IconRocket,
    to: '/docs/user/getting-started',
  },
  {
    title: 'Web + MQTT Control',
    description:
      'Real-time web dashboard with WebSocket streaming, plus full Home Assistant integration via MQTT auto-discovery.',
    Icon: IconWifi,
    to: '/docs/user/mqtt-home-assistant',
  },
];

// ---------------------------------------------------------------------------
// Audience section data
// ---------------------------------------------------------------------------

const audiences = [
  {
    label: 'End Users',
    tagline: 'Set up, configure, and control your audio system.',
    ctaText: 'Get Started',
    to: '/docs/user/getting-started',
    Icon: IconRocket,
  },
  {
    label: 'Developers',
    tagline: 'Write HAL drivers, extend the firmware, build mezzanines.',
    ctaText: 'Developer Docs',
    to: '/docs/developer/overview',
    Icon: IconChip,
  },
  {
    label: 'Enterprise',
    tagline: 'OEM integration, carrier board customisation, production deployment.',
    ctaText: 'Enterprise Docs',
    to: '/docs/enterprise/overview',
    Icon: IconFactory,
  },
];

// ---------------------------------------------------------------------------
// Components
// ---------------------------------------------------------------------------

function FeatureCard({ title, description, Icon, to }) {
  return (
    <Link to={to} className={styles.featureCard}>
      <div className={styles.featureIconWrapper}>
        <Icon />
      </div>
      <h3 className={styles.featureTitle}>{title}</h3>
      <p className={styles.featureDescription}>{description}</p>
    </Link>
  );
}

function AudienceSection() {
  return (
    <section className={styles.audience}>
      <p className={styles.audienceHeading}>Who is this for?</p>
      <div className={styles.audienceGrid}>
        {audiences.map((a) => (
          <div key={a.label} className={styles.audienceCard}>
            <div className={styles.audienceIconWrapper}><a.Icon /></div>
            <p className={styles.audienceLabel}>{a.label}</p>
            <p className={styles.audienceTagline}>{a.tagline}</p>
            <Link to={a.to} className={styles.audienceCta}>{a.ctaText} &rarr;</Link>
          </div>
        ))}
      </div>
    </section>
  );
}

function HeroSection() {
  return (
    <section className={styles.hero}>
      <AnimatedWaveform />
      <div className={styles.heroInner}>
        <div className={styles.heroBadge}>
          ALX Nova &mdash; Open Source &middot; ESP32-P4 &middot; Community-Driven
        </div>
        <h1 className={styles.heroTitle}>
          Build exceptional audio products.
          <br />
          <span className={styles.heroTitleAccent}>Together, without vendor lock-in.</span>
        </h1>
        <p className={styles.heroDescription}>
          A modular platform for audio control, DSP, and playback devices. Prototype freely,
          scale to production &mdash; combine certified hardware modules with a flexible software stack.
        </p>
        <div className={styles.heroCta}>
          <Link className={styles.ctaPrimary} to="/docs/user/getting-started">
            Start Building
          </Link>
          <Link className={styles.ctaSecondary} to="/docs/developer/overview">
            Explore the Ecosystem
          </Link>
        </div>
        <div className={styles.heroStats}>
          <span>Hardware Abstraction Layer</span>
          <span aria-hidden="true">&middot;</span>
          <span>Multi-Channel DSP</span>
          <span aria-hidden="true">&middot;</span>
          <span>Home Assistant</span>
          <span aria-hidden="true">&middot;</span>
          <span>Open Source</span>
        </div>
      </div>
    </section>
  );
}

function FeaturesSection() {
  return (
    <section className={styles.features}>
      <p className={styles.featuresHeading}>What the platform gives you</p>
      <div className={styles.featuresGrid}>
        {features.map((f) => (
          <FeatureCard key={f.title} {...f} />
        ))}
      </div>
    </section>
  );
}

// ---------------------------------------------------------------------------
// Page root
// ---------------------------------------------------------------------------

export default function Home() {
  const { siteConfig } = useDocusaurusContext();
  return (
    <Layout
      title={siteConfig.title}
      description="Open-source smart amplifier controller for ESP32-P4 with auto-sensing, DSP, HAL device management, and Home Assistant integration."
    >
      <main>
        <HeroSection />
        <AudienceSection />
        <FeaturesSection />
      </main>
    </Layout>
  );
}
