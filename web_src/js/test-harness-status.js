(function () {
  'use strict';

  var SVG_NS = 'http://www.w3.org/2000/svg';

  /**
   * MDI path data for status icons.
   * check-circle, alert-circle, clock-outline
   */
  var ICONS = {
    pass: 'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 14.5l-4-4 ' +
          '1.41-1.41L10 13.67l6.59-6.59L18 8.5l-8 8z',
    fail: 'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z',
    pending: 'M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 ' +
             '0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z'
  };

  var ICON_COLORS = {
    pass: '#4caf50',
    fail: '#f44336',
    pending: '#ff9800'
  };

  var MODULE_LABELS = {
    'ring-buffer': 'Ring Buffer',
    utils: 'Utilities',
    eslint: 'ESLint',
    docs: 'Documentation'
  };

  var DEFAULT_MODULES = ['ring-buffer', 'utils', 'eslint', 'docs'];

  /**
   * Creates an MDI SVG icon element.
   * @param {string} status - 'pass', 'fail', or 'pending'
   * @returns {SVGElement}
   */
  function createIcon(status) {
    var iconKey = (status === 'pass' || status === 'fail') ? status : 'pending';
    var svg = document.createElementNS(SVG_NS, 'svg');
    svg.setAttribute('viewBox', '0 0 24 24');
    svg.setAttribute('width', '16');
    svg.setAttribute('height', '16');
    svg.setAttribute('fill', ICON_COLORS[iconKey]);
    svg.setAttribute('aria-hidden', 'true');
    var path = document.createElementNS(SVG_NS, 'path');
    path.setAttribute('d', ICONS[iconKey]);
    svg.appendChild(path);
    return svg;
  }

  /**
   * Derives overall status from a list of per-module statuses.
   * @param {Object} moduleStatuses - map of module name to status string
   * @returns {string} 'pass', 'fail', or 'pending'
   */
  function deriveOverallStatus(moduleStatuses) {
    var keys = Object.keys(moduleStatuses);
    var hasFail = false;
    var hasPending = false;
    var i;
    for (i = 0; i < keys.length; i++) {
      var s = moduleStatuses[keys[i]];
      if (s === 'fail') {
        hasFail = true;
      } else if (s !== 'pass') {
        hasPending = true;
      }
    }
    if (hasFail) {
      return 'fail';
    }
    if (hasPending) {
      return 'pending';
    }
    return 'pass';
  }

  /**
   * Formats a Date as a short locale timestamp string.
   * @param {Date} date
   * @returns {string}
   */
  function formatTimestamp(date) {
    return date.toLocaleString();
  }

  /**
   * Creates a badge element showing overall status text.
   * @param {string} status - 'pass', 'fail', or 'pending'
   * @returns {HTMLElement}
   */
  function createBadge(status) {
    var badge = document.createElement('span');
    badge.style.display = 'inline-block';
    badge.style.padding = '2px 8px';
    badge.style.borderRadius = '4px';
    badge.style.fontSize = '12px';
    badge.style.fontWeight = 'bold';
    badge.style.verticalAlign = 'middle';
    badge.style.marginLeft = '8px';
    if (status === 'pass') {
      badge.style.background = '#e8f5e9';
      badge.style.color = '#2e7d32';
      badge.textContent = 'PASS';
    } else if (status === 'fail') {
      badge.style.background = '#ffebee';
      badge.style.color = '#c62828';
      badge.textContent = 'FAIL';
    } else {
      badge.style.background = '#fff3e0';
      badge.style.color = '#e65100';
      badge.textContent = 'PENDING';
    }
    return badge;
  }

  /**
   * Renders a test harness status card into the given container element.
   *
   * @param {string} containerId - ID of the DOM element to render into
   * @param {Object} [moduleStatuses] - map of module names to status strings
   *   ('pass' | 'fail' | 'pending'). Recognised module keys: ring-buffer, utils, eslint, docs.
   *   Defaults to all modules in 'pending' state.
   */
  function renderTestHarnessStatus(containerId, moduleStatuses) {
    var container = document.getElementById(containerId);
    if (!container) {
      return;
    }

    // Build a normalised statuses map, applying defaults for missing keys
    var statuses = {};
    var i;
    for (i = 0; i < DEFAULT_MODULES.length; i++) {
      statuses[DEFAULT_MODULES[i]] = 'pending';
    }
    if (moduleStatuses && typeof moduleStatuses === 'object') {
      var keys = Object.keys(moduleStatuses);
      for (i = 0; i < keys.length; i++) {
        statuses[keys[i]] = moduleStatuses[keys[i]];
      }
    }

    var overall = deriveOverallStatus(statuses);

    // Clear previous content
    while (container.firstChild) {
      container.removeChild(container.firstChild);
    }

    // Card wrapper
    var card = document.createElement('div');
    card.style.border = '1px solid #ddd';
    card.style.borderRadius = '6px';
    card.style.padding = '16px';
    card.style.fontFamily = 'sans-serif';
    card.style.maxWidth = '400px';

    // Header row: title + overall badge
    var header = document.createElement('div');
    header.style.display = 'flex';
    header.style.alignItems = 'center';
    header.style.marginBottom = '12px';

    var title = document.createElement('span');
    title.style.fontWeight = 'bold';
    title.style.fontSize = '15px';
    title.textContent = 'Test Harness Validation';

    header.appendChild(title);
    header.appendChild(createBadge(overall));
    card.appendChild(header);

    // Module list
    var list = document.createElement('ul');
    list.style.listStyle = 'none';
    list.style.margin = '0';
    list.style.padding = '0';

    var moduleKeys = Object.keys(statuses);
    for (i = 0; i < moduleKeys.length; i++) {
      var key = moduleKeys[i];
      var moduleStatus = statuses[key];
      var label = MODULE_LABELS[key] || key;

      var item = document.createElement('li');
      item.style.display = 'flex';
      item.style.alignItems = 'center';
      item.style.padding = '5px 0';
      item.style.borderBottom = '1px solid #f0f0f0';

      var iconWrapper = document.createElement('span');
      iconWrapper.style.marginRight = '8px';
      iconWrapper.style.display = 'flex';
      iconWrapper.style.alignItems = 'center';
      iconWrapper.appendChild(createIcon(moduleStatus));

      var nameSpan = document.createElement('span');
      nameSpan.style.flex = '1';
      nameSpan.textContent = label;

      var statusSpan = document.createElement('span');
      statusSpan.style.fontSize = '12px';
      statusSpan.style.fontWeight = 'bold';
      if (moduleStatus === 'pass') {
        statusSpan.style.color = '#4caf50';
        statusSpan.textContent = 'pass';
      } else if (moduleStatus === 'fail') {
        statusSpan.style.color = '#f44336';
        statusSpan.textContent = 'fail';
      } else {
        statusSpan.style.color = '#ff9800';
        statusSpan.textContent = 'pending';
      }

      item.appendChild(iconWrapper);
      item.appendChild(nameSpan);
      item.appendChild(statusSpan);
      list.appendChild(item);
    }

    card.appendChild(list);

    // Timestamp footer
    var footer = document.createElement('div');
    footer.style.marginTop = '10px';
    footer.style.fontSize = '11px';
    footer.style.color = '#888';
    footer.textContent = 'Last check: ' + formatTimestamp(new Date());
    card.appendChild(footer);

    container.appendChild(card);
  }

  window.renderTestHarnessStatus = renderTestHarnessStatus;
})();
