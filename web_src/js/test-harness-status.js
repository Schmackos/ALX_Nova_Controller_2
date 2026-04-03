// test-harness-status.js — Test harness validation status card
// This is a standalone module for test harness validation.
// Safe to remove: grep for 'test-harness' prefix.

/* eslint-disable no-unused-vars */

/**
 * Build a badge element indicating pass or fail status.
 * @param {string} status - 'pass' or 'fail'
 * @returns {HTMLElement}
 */
function buildTestHarnessBadge(status) {
    var badge = document.createElement('span');
    badge.style.display = 'inline-block';
    badge.style.padding = '2px 8px';
    badge.style.borderRadius = '4px';
    badge.style.fontWeight = 'bold';
    badge.style.fontSize = '12px';
    if (status === 'pass') {
        badge.textContent = 'PASS';
        badge.style.background = '#2a7a2a';
        badge.style.color = '#d4f4d4';
    } else {
        badge.textContent = 'FAIL';
        badge.style.background = '#7a2a2a';
        badge.style.color = '#f4d4d4';
    }
    return badge;
}

/**
 * Build a single module row showing its status and assertion count.
 * @param {string} label - Human-readable module name
 * @param {string} status - 'pass' or 'fail'
 * @param {number} assertions - Number of assertions run
 * @returns {HTMLElement}
 */
function buildTestHarnessModuleRow(label, status, assertions) {
    var row = document.createElement('div');
    row.style.display = 'flex';
    row.style.alignItems = 'center';
    row.style.gap = '10px';
    row.style.padding = '6px 0';
    row.style.borderBottom = '1px solid rgba(255,255,255,0.08)';

    var nameEl = document.createElement('span');
    nameEl.textContent = label;
    nameEl.style.flex = '1';
    nameEl.style.fontFamily = 'monospace';
    nameEl.style.fontSize = '13px';

    var assertEl = document.createElement('span');
    assertEl.textContent = assertions + ' assertions';
    assertEl.style.fontSize = '12px';
    assertEl.style.opacity = '0.65';

    var badge = buildTestHarnessBadge(status);

    row.appendChild(nameEl);
    row.appendChild(assertEl);
    row.appendChild(badge);
    return row;
}

/**
 * Render a test harness status card into the given container.
 * @param {HTMLElement} container - The element to render the card into
 * @param {Object} data - Status data
 * @param {string} data.ringbufStatus - 'pass' or 'fail'
 * @param {string} data.utilsStatus - 'pass' or 'fail'
 * @param {number} data.ringbufAssertions - Number of assertions for ringbuf module
 * @param {number} data.utilsAssertions - Number of assertions for utils module
 * @param {string} data.lastRun - ISO timestamp of last test run
 * @returns {HTMLElement} The rendered card element
 */
function renderTestHarnessStatus(container, data) {
    var card = document.createElement('div');
    card.style.background = 'rgba(255,255,255,0.05)';
    card.style.border = '1px solid rgba(255,255,255,0.12)';
    card.style.borderRadius = '8px';
    card.style.padding = '16px';
    card.style.fontFamily = 'sans-serif';
    card.style.color = '#e0e0e0';

    // Title
    var title = document.createElement('div');
    title.textContent = 'Test Harness Status';
    title.style.fontWeight = 'bold';
    title.style.fontSize = '15px';
    title.style.marginBottom = '12px';
    title.style.letterSpacing = '0.03em';
    card.appendChild(title);

    // Module rows
    var modules = document.createElement('div');
    modules.appendChild(buildTestHarnessModuleRow(
        'ringbuf',
        data.ringbufStatus,
        data.ringbufAssertions
    ));
    modules.appendChild(buildTestHarnessModuleRow(
        'utils',
        data.utilsStatus,
        data.utilsAssertions
    ));
    card.appendChild(modules);

    // Last run timestamp
    var lastRunEl = document.createElement('div');
    lastRunEl.style.marginTop = '10px';
    lastRunEl.style.fontSize = '11px';
    lastRunEl.style.opacity = '0.55';

    var lastRunLabel = document.createElement('span');
    lastRunLabel.textContent = 'Last run: ';

    var lastRunValue = document.createElement('span');
    lastRunValue.textContent = data.lastRun || 'never';
    lastRunValue.style.fontFamily = 'monospace';

    lastRunEl.appendChild(lastRunLabel);
    lastRunEl.appendChild(lastRunValue);
    card.appendChild(lastRunEl);

    container.appendChild(card);
    return card;
}
