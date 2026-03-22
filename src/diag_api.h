#pragma once

// Register diagnostic REST API endpoints on the global web server:
//   GET  /api/diagnostics          — full diagnostics export
//   GET  /api/diagnostics/journal  — diagnostic journal entries
//   DELETE /api/diagnostics/journal — clear journal
//   GET  /api/diag/snapshot        — compact snapshot for AI debugging
void registerDiagApiEndpoints();
