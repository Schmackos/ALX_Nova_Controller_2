import mediumZoom from 'medium-zoom';
import ExecutionEnvironment from '@docusaurus/ExecutionEnvironment';

if (ExecutionEnvironment.canUseDOM) {
  document.addEventListener('DOMContentLoaded', () => {
    const zoom = mediumZoom('.theme-doc-markdown img', {
      background: 'rgba(0, 0, 0, 0.85)',
    });

    // Re-attach after client-side navigation (SPA routing)
    const observer = new MutationObserver(() => {
      const unzoomedImgs = document.querySelectorAll(
        '.theme-doc-markdown img:not(.medium-zoom-image)'
      );
      if (unzoomedImgs.length > 0) {
        zoom.attach(unzoomedImgs);
      }
    });
    observer.observe(document.body, { childList: true, subtree: true });
  });
}
