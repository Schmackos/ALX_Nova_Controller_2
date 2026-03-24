"""Concurrent stress tests.

Fires parallel HTTP requests to verify the ESP32-P4 web server handles
concurrent load without errors. Stays within the 30 req/s rate limit.
"""

import time
from concurrent.futures import ThreadPoolExecutor, as_completed

import pytest


@pytest.mark.stress
class TestStress:
    """Verify device handles concurrent REST requests."""

    def test_concurrent_get_requests(self, auth_session, base_url):
        """25 parallel GETs to /api/diagnostics should all return 200."""
        url = f"{base_url}/api/diagnostics"
        results = []

        def fetch():
            start = time.monotonic()
            resp = auth_session.get(url, timeout=10)
            elapsed = (time.monotonic() - start) * 1000
            return resp.status_code, elapsed

        with ThreadPoolExecutor(max_workers=10) as pool:
            futures = [pool.submit(fetch) for _ in range(25)]
            for f in as_completed(futures):
                results.append(f.result())

        statuses = [r[0] for r in results]
        latencies = [r[1] for r in results]

        ok_count = statuses.count(200)
        max_latency = max(latencies)

        # Allow a few 429s under load but most should succeed
        assert ok_count >= 20, (
            f"Only {ok_count}/25 returned 200. Statuses: {statuses}"
        )
        assert max_latency < 5000, (
            f"Max latency {max_latency:.0f}ms exceeds 5s"
        )

    def test_concurrent_mixed_endpoints(self, auth_session, base_url):
        """25 parallel GETs across 5 endpoints should all succeed."""
        endpoints = [
            "/api/diagnostics",
            "/api/health",
            "/api/hal/devices",
            "/api/settings",
            "/api/psram/status",
        ]
        results = []

        def fetch(path):
            url = f"{base_url}{path}"
            start = time.monotonic()
            resp = auth_session.get(url, timeout=10)
            elapsed = (time.monotonic() - start) * 1000
            return path, resp.status_code, elapsed

        with ThreadPoolExecutor(max_workers=10) as pool:
            futures = []
            for path in endpoints:
                for _ in range(5):  # 5 requests per endpoint = 25 total
                    futures.append(pool.submit(fetch, path))
            for f in as_completed(futures):
                results.append(f.result())

        failed = [(p, s) for p, s, _ in results if s != 200]
        latencies = [e for _, _, e in results]

        assert len(failed) <= 5, (
            f"Too many failures: {len(failed)}/25. Failed: {failed[:5]}"
        )
        assert max(latencies) < 5000, (
            f"Max latency {max(latencies):.0f}ms exceeds 5s"
        )
