"""Create or update GitHub issues for device test failures using the gh CLI."""

import json
import subprocess


class IssueCreator:
    """Manages GitHub issue creation with deduplication.

    Uses the `gh` CLI tool to search for existing issues and either
    create a new one or add a comment to an existing match.
    """

    def __init__(self, repo=None):
        """Initialize.

        Args:
            repo: GitHub repo in "owner/repo" format. If None, gh uses
                  the repo from the current working directory.
        """
        self._repo = repo

    def _gh(self, args, check=True):
        """Run a gh CLI command and return stdout."""
        cmd = ["gh"] + args
        if self._repo:
            cmd.extend(["--repo", self._repo])
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=check
        )
        return result.stdout.strip()

    def find_existing(self, title_search):
        """Search for an open issue matching the title.

        Args:
            title_search: search string to match against issue titles

        Returns:
            Issue number (int) if found, None otherwise.
        """
        try:
            output = self._gh([
                "issue", "list",
                "--search", f'"{title_search}" in:title',
                "--state", "open",
                "--label", "device-test",
                "--json", "number,title",
                "--limit", "5",
            ], check=False)
            if not output:
                return None
            issues = json.loads(output)
            # Exact prefix match preferred
            for issue in issues:
                if title_search.lower() in issue["title"].lower():
                    return issue["number"]
            return None
        except (json.JSONDecodeError, subprocess.CalledProcessError):
            return None

    def create_issue(self, title, body, labels=None):
        """Create a new GitHub issue.

        Args:
            title: issue title
            body: issue body (markdown)
            labels: list of label strings

        Returns:
            URL of the created issue.
        """
        args = ["issue", "create", "--title", title, "--body", body]
        if labels:
            for label in labels:
                args.extend(["--label", label])
        return self._gh(args)

    def add_comment(self, issue_number, body):
        """Add a comment to an existing issue.

        Args:
            issue_number: GitHub issue number
            body: comment body (markdown)
        """
        self._gh(["issue", "comment", str(issue_number), "--body", body])

    def report_failure(self, check_name, firmware_version, failed_checks,
                       device_state, serial_excerpt=None):
        """Report a device test failure, deduplicating against existing issues.

        Args:
            check_name: short name for the failing test area
            firmware_version: current firmware version string
            failed_checks: markdown table of failed check results
            device_state: markdown block with heap/PSRAM/uptime info
            serial_excerpt: optional last N lines of serial output

        Returns:
            Tuple of (issue_url_or_number, created_new: bool)
        """
        search_title = f"[Device Test] {check_name}"

        body_parts = [
            f"## Firmware Version\n{firmware_version}\n",
            f"## Failed Checks\n{failed_checks}\n",
            f"## Device State\n{device_state}\n",
        ]
        if serial_excerpt:
            body_parts.append(
                f"## Serial Log Excerpt\n```\n{serial_excerpt}\n```\n"
            )
        body_parts.append(
            "## Reproduction Steps\n"
            f"1. Flash firmware version {firmware_version}\n"
            f"2. Run: `cd device_tests && pytest tests/ -k {check_name} "
            f"--device-port COM8 -v`\n"
        )
        body = "\n".join(body_parts)

        existing = self.find_existing(search_title)
        if existing:
            comment = (
                f"**Re-occurrence on firmware {firmware_version}**\n\n{body}"
            )
            self.add_comment(existing, comment)
            return existing, False

        url = self.create_issue(
            title=search_title,
            body=body,
            labels=["device-test", "bug"],
        )
        return url, True
