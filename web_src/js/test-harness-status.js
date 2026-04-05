// ===== Test Harness Status Card =====
// Standalone module — no dependencies on main app globals.
// Renders a status card summarising test-harness validation info
// (ring buffer, utils) into a given DOM container.

function renderTestHarnessStatus(containerId) {
    var container = document.getElementById(containerId);
    if (!container) { return; }

    // ===== Module Definitions =====
    var modules = [
        {
            name: 'Ring Buffer',
            key: 'ring_buffer',
            description: 'Circular buffer used by the audio pipeline for lock-free inter-task data transfer.',
            tests: ['overflow guard', 'underflow guard', 'head/tail wrap-around', 'capacity boundary']
        },
        {
            name: 'Utils',
            key: 'utils',
            description: 'General-purpose utility helpers shared across firmware modules.',
            tests: ['string sanitisation', 'numeric clamp', 'bit-manipulation helpers', 'safe memcpy bounds']
        }
    ];

    // ===== Build Module Rows =====
    var moduleRowsHtml = '';
    for (var i = 0; i < modules.length; i++) {
        var mod = modules[i];
        var testListItems = '';
        for (var t = 0; t < mod.tests.length; t++) {
            testListItems += '<li class="th-test-item">' +
                '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true">' +
                '<path d="M21,7L9,19L3.5,13.5L4.91,12.09L9,16.17L19.59,5.59L21,7Z"/>' +
                '</svg> <span class="th-test-label"></span></li>';
        }
        moduleRowsHtml +=
            '<div class="th-module-card">' +
            '<div class="th-module-header">' +
            '<svg viewBox="0 0 24 24" width="18" height="18" fill="currentColor" aria-hidden="true">' +
            '<path d="M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M11,17V16H9V14H13V13H10A1,1 0 0,1 9,12V9A1,1 0 0,1 10,8H11V7H13V8H15V10H11V11H14A1,1 0 0,1 15,12V15A1,1 0 0,1 14,16H13V17H11Z"/>' +
            '</svg>' +
            '<span class="th-module-name"></span>' +
            '</div>' +
            '<p class="th-module-desc"></p>' +
            '<ul class="th-test-list">' + testListItems + '</ul>' +
            '</div>';
    }

    // ===== Inject Static Template =====
    container.innerHTML =
        '<div class="th-status-card">' +
        '<div class="th-card-header">' +
        '<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor" aria-hidden="true">' +
        '<path d="M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18A2,2 0 0,0 20,20V8L14,2M18,20H6V4H13V9H18V20M10,19L8,14H9.5L11,17.5L12.5,14H14L12,19H10M8,13V12H16V13H8Z"/>' +
        '</svg>' +
        '<span class="th-card-title">Test Harness Validation</span>' +
        '<span class="th-card-badge">kitchen-sink</span>' +
        '</div>' +
        '<p class="th-card-summary"></p>' +
        '<div class="th-modules-grid">' + moduleRowsHtml + '</div>' +
        '<div class="th-card-footer">' +
        '<svg viewBox="0 0 24 24" width="14" height="14" fill="currentColor" aria-hidden="true">' +
        '<path d="M12,20A8,8 0 0,1 4,12A8,8 0 0,1 12,4A8,8 0 0,1 20,12A8,8 0 0,1 12,20M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,12.5A1.5,1.5 0 0,1 10.5,11A1.5,1.5 0 0,1 12,9.5A1.5,1.5 0 0,1 13.5,11A1.5,1.5 0 0,1 12,12.5M12,7.2C9.9,7.2 8.2,8.9 8.2,11C8.2,14 12,17.5 12,17.5C12,17.5 15.8,14 15.8,11C15.8,8.9 14.1,7.2 12,7.2Z"/>' +
        '</svg>' +
        '<span class="th-footer-label">ESP32-P4 native platform — no hardware required</span>' +
        '</div>' +
        '</div>';

    // ===== Populate Dynamic Text with textContent =====
    var summary = container.querySelector('.th-card-summary');
    if (summary) {
        summary.textContent =
            'Validates ' + modules.length + ' core test-harness modules used by the ALX Nova audio firmware. ' +
            'All tests run on the PlatformIO native platform.';
    }

    var moduleCards = container.querySelectorAll('.th-module-card');
    for (var m = 0; m < moduleCards.length; m++) {
        var card = moduleCards[m];
        var modData = modules[m];

        var nameEl = card.querySelector('.th-module-name');
        if (nameEl) { nameEl.textContent = modData.name; }

        var descEl = card.querySelector('.th-module-desc');
        if (descEl) { descEl.textContent = modData.description; }

        var testItems = card.querySelectorAll('.th-test-item');
        for (var ti = 0; ti < testItems.length; ti++) {
            var labelEl = testItems[ti].querySelector('.th-test-label');
            if (labelEl && modData.tests[ti] !== undefined) {
                labelEl.textContent = modData.tests[ti];
            }
        }
    }
}
