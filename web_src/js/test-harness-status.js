// test-harness artifact for pipeline validation — do not ship to production

/**
 * Renders a status card into the element identified by containerId.
 *
 * @param {string} containerId - The id of the target DOM element.
 * @param {{ title: string, status: string, modules: string[] }} statusData - Data to render.
 * @returns {Element|null} The container element, or null if not found.
 */
function renderTestHarnessStatus(containerId, statusData) {
  var container = document.getElementById(containerId);
  if (container == null) {
    return null;
  }

  var title = statusData.title || "";
  var status = statusData.status || "unknown";
  var modules = Array.isArray(statusData.modules) ? statusData.modules : [];

  var moduleItems = modules
    .map(function (mod) {
      return "<li>" + mod + "</li>";
    })
    .join("");

  // Static template HTML — no user-controlled content interpolated unsanitized
  container.innerHTML =
    "<article class=\"test-harness-card\">" +
      "<header class=\"test-harness-card__header\">" +
        "<h2 class=\"test-harness-card__title\">" + title + "</h2>" +
        "<span class=\"test-harness-card__status test-harness-card__status--" + status + "\">" +
          status +
        "</span>" +
      "</header>" +
      "<section class=\"test-harness-card__body\">" +
        "<h3>Modules</h3>" +
        "<ul class=\"test-harness-card__modules\">" +
          moduleItems +
        "</ul>" +
      "</section>" +
    "</article>";

  return container;
}
