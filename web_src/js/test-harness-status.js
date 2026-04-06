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

  container.textContent = "";

  var article = document.createElement("article");
  article.className = "test-harness-card";

  var header = document.createElement("header");
  header.className = "test-harness-card__header";

  var h2 = document.createElement("h2");
  h2.className = "test-harness-card__title";
  h2.textContent = title;
  header.appendChild(h2);

  var span = document.createElement("span");
  span.className = "test-harness-card__status test-harness-card__status--" + status;
  span.textContent = status;
  header.appendChild(span);

  article.appendChild(header);

  var section = document.createElement("section");
  section.className = "test-harness-card__body";

  var h3 = document.createElement("h3");
  h3.textContent = "Modules";
  section.appendChild(h3);

  var ul = document.createElement("ul");
  ul.className = "test-harness-card__modules";
  modules.forEach(function (mod) {
    var li = document.createElement("li");
    li.textContent = mod;
    ul.appendChild(li);
  });
  section.appendChild(ul);

  article.appendChild(section);
  container.appendChild(article);

  return container;
}
