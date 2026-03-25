import React from 'react';
import Link from '@docusaurus/Link';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import styles from './index.module.css';

const PLACEHOLDER_PROJECTS = [
  {
    title: 'ALX Nova Reference Carrier',
    description: 'The official reference carrier board — half-rack chassis, 4 mezzanine slots, 12V DC SELV input. Ships with ES8311 onboard ADC and PCM1808 stereo ADC.',
    author: 'Schmackos',
    href: 'https://github.com/Schmackos/ALX_Nova_Controller_2',
    tags: ['carrier', 'reference'],
  },
];

function ProjectCard({ title, description, author, href, tags }) {
  return (
    <div className={styles.audienceCard}>
      <p className={styles.audienceLabel}>{title}</p>
      <p className={styles.audienceTagline}>{description}</p>
      <p style={{ fontSize: '0.8rem', color: '#666', margin: 0 }}>by {author}</p>
      <div style={{ display: 'flex', gap: '0.4rem', flexWrap: 'wrap', marginTop: '0.5rem' }}>
        {tags.map((t) => (
          <span key={t} style={{ fontSize: '0.75rem', background: 'rgba(255,152,0,0.1)', color: '#FF9800', border: '1px solid rgba(255,152,0,0.3)', borderRadius: '4px', padding: '0.1rem 0.5rem' }}>{t}</span>
        ))}
      </div>
      <Link to={href} className={styles.audienceCta} style={{ marginTop: '0.75rem' }}>View project &rarr;</Link>
    </div>
  );
}

export default function Showcase() {
  return (
    <Layout
      title="Showcase"
      description="Projects, carrier boards, and mezzanine modules built with ALX Nova."
    >
      <main>
        <section style={{ background: 'linear-gradient(160deg, #0d0d0d 0%, #1a1a1a 60%, #1f1208 100%)', padding: '4rem 2rem 3rem', textAlign: 'center', borderBottom: '1px solid #2a2a2a' }}>
          <Heading as="h1" style={{ color: '#fff', fontSize: '2.5rem', fontWeight: 800, marginBottom: '1rem' }}>
            Built with <span style={{ color: '#FF9800' }}>ALX Nova</span>
          </Heading>
          <p style={{ color: '#b0b0b0', fontSize: '1.05rem', maxWidth: '600px', margin: '0 auto 2rem' }}>
            Carrier boards, mezzanine modules, and audio products from the community. Submit your project to be listed here.
          </p>
          <Link
            to="https://github.com/Schmackos/ALX_Nova_Controller_2/discussions/new?title=[Showcase]"
            style={{ display: 'inline-block', padding: '0.65rem 1.5rem', background: '#FF9800', color: '#000', fontWeight: 700, borderRadius: '6px', textDecoration: 'none' }}
          >
            Submit your project
          </Link>
        </section>

        <section className={styles.audience}>
          <p className={styles.audienceHeading}>Community Projects</p>
          <div className={styles.audienceGrid}>
            {PLACEHOLDER_PROJECTS.map((p) => (
              <ProjectCard key={p.title} {...p} />
            ))}
          </div>
          <p style={{ textAlign: 'center', color: '#666', fontSize: '0.9rem', marginTop: '2rem' }}>
            Open a GitHub Discussion with the <code>[Showcase]</code> tag to have your project added here.
          </p>
        </section>
      </main>
    </Layout>
  );
}
