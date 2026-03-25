import React from 'react';
import Link from '@docusaurus/Link';
import Layout from '@theme/Layout';
import Heading from '@theme/Heading';
import indexStyles from './index.module.css';
import styles from './showcase.module.css';

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
    <div className={indexStyles.audienceCard}>
      <p className={indexStyles.audienceLabel}>{title}</p>
      <p className={indexStyles.audienceTagline}>{description}</p>
      <p className={styles.projectAuthor}>by {author}</p>
      <div className={styles.tagList}>
        {tags.map((t) => (
          <span key={t} className={styles.tag}>{t}</span>
        ))}
      </div>
      <Link to={href} className={indexStyles.audienceCta}>View project &rarr;</Link>
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
        <section className={styles.hero}>
          <Heading as="h1" className={styles.heroTitle}>
            Built with <span className={styles.heroTitleAccent}>ALX Nova</span>
          </Heading>
          <p className={styles.heroDescription}>
            Carrier boards, mezzanine modules, and audio products from the community. Submit your project to be listed here.
          </p>
          <Link
            to="https://github.com/Schmackos/ALX_Nova_Controller_2/discussions/new?title=[Showcase]"
            className={styles.heroButton}
          >
            Submit your project
          </Link>
        </section>

        <section className={indexStyles.audience}>
          <p className={indexStyles.audienceHeading}>Community Projects</p>
          <div className={indexStyles.audienceGrid}>
            {PLACEHOLDER_PROJECTS.map((p) => (
              <ProjectCard key={p.title} {...p} />
            ))}
          </div>
          <p className={styles.emptyNote}>
            Open a GitHub Discussion with the <code>[Showcase]</code> tag to have your project added here.
          </p>
        </section>
      </main>
    </Layout>
  );
}
