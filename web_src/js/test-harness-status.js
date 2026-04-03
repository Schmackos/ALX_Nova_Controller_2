/**
 * test-harness-status.js
 * Standalone test artifact — pipeline validation status card.
 * No external dependencies. Works as a plain <script> tag.
 */

/**
 * Renders a test harness status card into the given container element.
 * This is a standalone validation artifact and is not wired to any backend.
 *
 * @param {string} containerId - The id of the container element to render into.
 */
function renderTestHarnessStatus(containerId) {
  var container = document.getElementById(containerId);
  if (!container) {
    return;
  }

  var modules = [
    { name: 'Ring Buffer', desc: 'Circular buffer utility — overflow and wrap-around tests' },
    { name: 'Utilities',   desc: 'String, math, and conversion helper validation' }
  ];

  // Static structural template — innerHTML is intentional here for layout.
  container.innerHTML = [
    '<div style="',
    '  font-family: system-ui, sans-serif;',
    '  border: 1px solid #ccc;',
    '  border-radius: 6px;',
    '  padding: 16px 20px;',
    '  max-width: 480px;',
    '  background: #f9f9f9;',
    '  color: #222;',
    '">',
    '  <div style="font-size: 1rem; font-weight: 600; margin-bottom: 10px;">',
    '    Test Harness Validation',
    '  </div>',
    '  <div id="_thsStatusBadge" style="',
    '    display: inline-block;',
    '    font-size: 0.75rem;',
    '    padding: 2px 8px;',
    '    border-radius: 12px;',
    '    background: #d4edda;',
    '    color: #155724;',
    '    margin-bottom: 12px;',
    '  "></div>',
    '  <ul id="_thsModuleList" style="',
    '    list-style: none;',
    '    margin: 0 0 12px 0;',
    '    padding: 0;',
    '  "></ul>',
    '  <div id="_thsNote" style="',
    '    font-size: 0.75rem;',
    '    color: #666;',
    '  "></div>',
    '</div>'
  ].join('\n');

  // Populate dynamic text via textContent to satisfy the ESLint innerHTML rule.
  var badge = container.querySelector('#_thsStatusBadge');
  if (badge) {
    badge.textContent = 'Validation artifacts present';
  }

  var list = container.querySelector('#_thsModuleList');
  if (list) {
    for (var i = 0; i < modules.length; i++) {
      var item = document.createElement('li');
      item.style.cssText = 'padding: 6px 0; border-bottom: 1px solid #e0e0e0; font-size: 0.875rem;';

      var nameSpan = document.createElement('span');
      nameSpan.style.fontWeight = '600';
      nameSpan.textContent = modules[i].name;

      var descSpan = document.createElement('span');
      descSpan.style.color = '#555';
      descSpan.textContent = ' \u2014 ' + modules[i].desc;

      item.appendChild(nameSpan);
      item.appendChild(descSpan);
      list.appendChild(item);
    }
  }

  var note = container.querySelector('#_thsNote');
  if (note) {
    note.textContent =
      'These are test artifacts for pipeline validation only. ' +
      'Not wired to any backend endpoint.';
  }
}
