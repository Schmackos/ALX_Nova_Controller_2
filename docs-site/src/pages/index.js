import React from 'react';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import styles from './index.module.css';

// ---------------------------------------------------------------------------
// Inline SVG icons (Material Design Icons, viewBox 0 0 24 24)
// ---------------------------------------------------------------------------

function IconWaveform() {
  return (
    <svg
      viewBox="0 0 24 24"
      width="36"
      height="36"
      fill="currentColor"
      aria-hidden="true"
    >
      {/* mdi-waveform */}
      <path d="M17,12H15V8H13V16H11V10H9V14H7V12H2V14H6.27C6.64,15.16 7.72,16 9,16A3,3 0 0,0 12,13.27V12H13V16A1,1 0 0,0 14,17H16A1,1 0 0,0 17,16V14H22V12H17Z" />
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

// ---------------------------------------------------------------------------
// Feature card data
// ---------------------------------------------------------------------------

const features = [
  {
    title: 'Smart Auto-Sensing',
    description:
      'Automatic signal detection with configurable voltage thresholds, auto-off timers, and amplifier relay control.',
    Icon: IconWaveform,
  },
  {
    title: 'HAL Device Framework',
    description:
      'Plug-and-play hardware abstraction with I2C discovery, EEPROM identification, and hot-swap device lifecycle management.',
    Icon: IconChip,
  },
  {
    title: 'Multi-Channel DSP',
    description:
      '4-channel parametric EQ, crossovers, limiters, and compressors with double-buffered glitch-free config swap.',
    Icon: IconEqualizer,
  },
  {
    title: 'Web + MQTT Control',
    description:
      'Real-time web dashboard with WebSocket streaming, plus full Home Assistant integration via MQTT auto-discovery.',
    Icon: IconWifi,
  },
];

// ---------------------------------------------------------------------------
// Components
// ---------------------------------------------------------------------------

function FeatureCard({ title, description, Icon }) {
  return (
    <div className={styles.featureCard}>
      <div className={styles.featureIconWrapper}>
        <Icon />
      </div>
      <h3 className={styles.featureTitle}>{title}</h3>
      <p className={styles.featureDescription}>{description}</p>
    </div>
  );
}

function HeroSection() {
  return (
    <section className={styles.hero}>
      <div className={styles.heroInner}>
        <h1 className={styles.heroTitle}>ALX Nova</h1>
        <p className={styles.heroTagline}>
          Intelligent Amplifier Control for ESP32-P4
        </p>
        <p className={styles.heroDescription}>
          Open-source smart amplifier controller with auto-sensing, multi-channel
          DSP, HAL device management, and seamless Home Assistant integration.
        </p>
        <div className={styles.heroCta}>
          <Link className={styles.ctaPrimary} to="/docs/user/getting-started">
            Get Started
          </Link>
          <Link className={styles.ctaSecondary} to="/docs/developer/overview">
            Developer Guide
          </Link>
        </div>
      </div>
    </section>
  );
}

function FeaturesSection() {
  return (
    <section className={styles.features}>
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
        <FeaturesSection />
      </main>
    </Layout>
  );
}
