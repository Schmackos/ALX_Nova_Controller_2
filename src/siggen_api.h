#pragma once

// Register signal generator REST API endpoints on the global web server:
//   GET  /api/signalgenerator — read current signal generator state
//   POST /api/signalgenerator — update signal generator parameters
void registerSignalGenApiEndpoints();
