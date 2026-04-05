/* exported renderTestHarnessStatus */
/**
 * test-harness-status.js
 * Standalone module: renders a "Test Harness Validation" status card.
 * Self-contained — no globals from other project files are referenced.
 */
(function () {
  'use strict';

  var MODULES = [
    { id: 'ring-buffer', label: 'Ring Buffer' },
    { id: 'utils', label: 'Utils' },
    { id: 'eslint', label: 'ESLint' },
    { id: 'docs', label: 'Docs' }
  ];

  /**
   * Returns an SVG check-circle icon element (MDI mdiCheckCircle).
   * @returns {SVGElement}
   */
  function iconPass() {
    var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('width', '18');
    svg.setAttribute('height', '18');
    svg.setAttribute('fill', 'currentColor');
    svg.setAttribute('aria-hidden', 'true');
    var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute(
      'd',
      'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z'
    );
    svg.appendChild(path);
    return svg;
  }

  /**
   * Returns an SVG alert-circle icon element (MDI mdiAlertCircle).
   * @returns {SVGElement}
   */
  function iconFail() {
    var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('width', '18');
    svg.setAttribute('height', '18');
    svg.setAttribute('fill', 'currentColor');
    svg.setAttribute('aria-hidden', 'true');
    var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute(
      'd',
      'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z'
    );
    svg.appendChild(path);
    return svg;
  }

  /**
   * Returns an SVG clock-outline icon element (MDI mdiClockOutline).
   * @returns {SVGElement}
   */
  function iconPending() {
    var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('width', '18');
    svg.setAttribute('height', '18');
    svg.setAttribute('fill', 'currentColor');
    svg.setAttribute('aria-hidden', 'true');
    var path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute(
      'd',
      'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z'
    );
    svg.appendChild(path);
    return svg;
  }

  /**
   * Returns the appropriate icon element for a given status string.
   * @param {string} status - 'pass', 'fail', or 'pending'
   * @returns {SVGElement}
   */
  function statusIcon(status) {
    if (status === 'pass') {
      return iconPass();
    }
    if (status === 'fail') {
      return iconFail();
    }
    return iconPending();
  }

  /**
   * Returns the CSS color for a given status string.
   * @param {string} status - 'pass', 'fail', or 'pending'
   * @returns {string}
   */
  function statusColor(status) {
    if (status === 'pass') {
      return '#4caf50';
    }
    if (status === 'fail') {
      return '#f44336';
    }
    return '#ff9800';
  }

  /**
   * Returns a human-readable label for a given status string.
   * @param {string} status - 'pass', 'fail', or 'pending'
   * @returns {string}
   */
  function statusLabel(status) {
    if (status === 'pass') {
      return 'Pass';
    }
    if (status === 'fail') {
      return 'Fail';
    }
    return 'Pending';
  }

  /**
   * Formats a Date object as a locale timestamp string.
   * @param {Date} date
   * @returns {string}
   */
  function formatTimestamp(date) {
    return date.toLocaleString();
  }

  /**
   * Derives an overall status from an array of module status strings.
   * Returns 'fail' if any module has failed, 'pending' if any are pending,
   * and 'pass' only when all modules have passed.
   * @param {string[]} statuses
   * @returns {string}
   */
  function deriveOverallStatus(statuses) {
    var i;
    for (i = 0; i < statuses.length; i++) {
      if (statuses[i] === 'fail') {
        return 'fail';
      }
    }
    for (i = 0; i < statuses.length; i++) {
      if (statuses[i] === 'pending') {
        return 'pending';
      }
    }
    return 'pass';
  }

  /**
   * Renders a "Test Harness Validation" status card into the element with the
   * given container ID.  The card shows an overall status badge, a per-module
   * checklist, and a last-checked timestamp.
   *
   * Module statuses default to 'pending'.  Pass a moduleStatuses object to
   * override individual modules, e.g.:
   *   renderTestHarnessStatus('my-card', { eslint: 'pass', docs: 'fail' });
   *
   * @param {string} containerId - ID of the host element.
   * @param {Object} [moduleStatuses] - Optional map of module id to status.
   */
  function renderTestHarnessStatus(containerId, moduleStatuses) {
    var container = document.getElementById(containerId);
    if (!container) {
      return;
    }

    var statuses = moduleStatuses || {};

    // Resolve per-module status, defaulting to 'pending'.
    var resolvedStatuses = MODULES.map(function (mod) {
      var s = statuses[mod.id];
      if (s === 'pass' || s === 'fail') {
        return s;
      }
      return 'pending';
    });

    var overall = deriveOverallStatus(resolvedStatuses);
    var now = new Date();

    // --- Card wrapper ---
    var card = document.createElement('div');
    card.style.cssText = [
      'font-family: sans-serif',
      'border: 1px solid #ddd',
      'border-radius: 8px',
      'padding: 16px',
      'max-width: 360px',
      'background: #fff',
      'box-shadow: 0 2px 6px rgba(0,0,0,0.08)'
    ].join(';');

    // --- Header row ---
    var header = document.createElement('div');
    header.style.cssText = 'display:flex;align-items:center;gap:8px;margin-bottom:12px';

    var titleEl = document.createElement('span');
    titleEl.style.cssText = 'font-size:15px;font-weight:600;flex:1';
    titleEl.textContent = 'Test Harness Validation';

    // Overall status badge
    var badge = document.createElement('span');
    badge.style.cssText = [
      'display:inline-flex',
      'align-items:center',
      'gap:4px',
      'padding:2px 8px',
      'border-radius:12px',
      'font-size:12px',
      'font-weight:600',
      'color:#fff',
      'background:' + statusColor(overall)
    ].join(';');
    badge.appendChild(statusIcon(overall));
    var badgeText = document.createElement('span');
    badgeText.textContent = statusLabel(overall);
    badge.appendChild(badgeText);

    header.appendChild(titleEl);
    header.appendChild(badge);
    card.appendChild(header);

    // --- Divider ---
    var divider = document.createElement('hr');
    divider.style.cssText = 'border:none;border-top:1px solid #eee;margin:0 0 12px';
    card.appendChild(divider);

    // --- Module list ---
    var list = document.createElement('ul');
    list.setAttribute('aria-label', 'Module validation results');
    list.style.cssText = 'list-style:none;margin:0 0 12px;padding:0';

    MODULES.forEach(function (mod, idx) {
      var modStatus = resolvedStatuses[idx];

      var item = document.createElement('li');
      item.style.cssText = [
        'display:flex',
        'align-items:center',
        'gap:8px',
        'padding:5px 0',
        'border-bottom:1px solid #f5f5f5',
        'color:' + statusColor(modStatus)
      ].join(';');

      var iconWrapper = document.createElement('span');
      iconWrapper.style.cssText = 'display:inline-flex;flex-shrink:0';
      iconWrapper.appendChild(statusIcon(modStatus));

      var label = document.createElement('span');
      label.style.cssText = 'font-size:13px;flex:1;color:#333';
      label.textContent = mod.label;

      var statusSpan = document.createElement('span');
      statusSpan.style.cssText = 'font-size:11px;font-weight:600;text-transform:uppercase';
      statusSpan.textContent = statusLabel(modStatus);

      item.appendChild(iconWrapper);
      item.appendChild(label);
      item.appendChild(statusSpan);
      list.appendChild(item);
    });

    card.appendChild(list);

    // --- Timestamp ---
    var tsRow = document.createElement('div');
    tsRow.style.cssText = 'display:flex;align-items:center;gap:6px;color:#888;font-size:11px';

    var clockSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    clockSvg.setAttribute('viewBox', '0 0 24 24');
    clockSvg.setAttribute('width', '13');
    clockSvg.setAttribute('height', '13');
    clockSvg.setAttribute('fill', 'currentColor');
    clockSvg.setAttribute('aria-hidden', 'true');
    var clockPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    clockPath.setAttribute(
      'd',
      'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z'
    );
    clockSvg.appendChild(clockPath);

    var tsLabel = document.createElement('span');
    tsLabel.textContent = 'Last checked: ';

    var tsValue = document.createElement('time');
    tsValue.setAttribute('datetime', now.toISOString());
    tsValue.textContent = formatTimestamp(now);

    tsRow.appendChild(clockSvg);
    tsRow.appendChild(tsLabel);
    tsRow.appendChild(tsValue);
    card.appendChild(tsRow);

    // Clear and mount.
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }
    container.appendChild(card);
  }

  // Expose to global scope for external use.
  window.renderTestHarnessStatus = renderTestHarnessStatus;
})();
