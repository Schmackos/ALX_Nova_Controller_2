"""Parse health check and milestone log lines from serial output."""

import re
from dataclasses import dataclass, field


@dataclass
class HealthEntry:
    """A single parsed health log entry."""
    level: str          # "I", "W", or "E"
    module: str         # e.g., "Health", "Audio", "HAL"
    message: str        # the rest of the log line
    raw: str            # original line


@dataclass
class BootMilestones:
    """Tracks key boot milestones from serial output."""
    auth_initialized: bool = False
    settings_loaded: bool = False
    hal_discovery_done: bool = False
    wifi_connected: bool = False
    mqtt_connected: bool = False
    audio_initialized: bool = False
    gui_initialized: bool = False
    boot_complete: bool = False
    errors: list = field(default_factory=list)
    warnings: list = field(default_factory=list)

    @property
    def critical_milestones_met(self):
        """Check that the minimum milestones for a healthy boot are met."""
        return (
            self.auth_initialized
            and self.settings_loaded
            and self.hal_discovery_done
        )


# Pattern: [I][ModuleName] message  or  [W][ModuleName] message
_LOG_LINE_RE = re.compile(
    r"\[([IWED])\]\s*\[([^\]]+)\]\s*(.*)"
)

# Milestone detection patterns
_MILESTONES = {
    "auth_initialized": re.compile(
        r"\[Auth\].*(?:initialized|system initialized)", re.IGNORECASE
    ),
    "settings_loaded": re.compile(
        r"\[Settings\].*(?:loaded|load complete)", re.IGNORECASE
    ),
    "hal_discovery_done": re.compile(
        r"\[HAL Discovery\].*(?:complete|done|finished)", re.IGNORECASE
    ),
    "wifi_connected": re.compile(
        r"\[WiFi\].*(?:connected|IP:)", re.IGNORECASE
    ),
    "mqtt_connected": re.compile(
        r"\[MQTT\].*connected", re.IGNORECASE
    ),
    "audio_initialized": re.compile(
        r"\[Audio\].*(?:initialized|init complete|pipeline.*ready)", re.IGNORECASE
    ),
    "gui_initialized": re.compile(
        r"\[GUI\].*(?:initialized|init complete|task started)", re.IGNORECASE
    ),
    "boot_complete": re.compile(
        r"(?:setup\(\) complete|Boot complete|Main loop starting)", re.IGNORECASE
    ),
}


class HealthParser:
    """Parse serial log lines into structured health data."""

    def __init__(self):
        self.entries = []
        self.milestones = BootMilestones()

    def feed(self, lines):
        """Parse a list of serial log lines.

        Args:
            lines: list of strings (raw serial output lines)
        """
        for line in lines:
            self._parse_line(line)

    def _parse_line(self, line):
        m = _LOG_LINE_RE.search(line)
        if not m:
            # Check milestones even in non-structured lines
            self._check_milestones(line)
            return

        level, module, message = m.group(1), m.group(2), m.group(3)
        entry = HealthEntry(level=level, module=module, message=message, raw=line)
        self.entries.append(entry)

        if level == "E":
            self.milestones.errors.append(entry)
        elif level == "W":
            self.milestones.warnings.append(entry)

        self._check_milestones(line)

    def _check_milestones(self, line):
        for attr, pattern in _MILESTONES.items():
            if not getattr(self.milestones, attr) and pattern.search(line):
                setattr(self.milestones, attr, True)

    def get_errors(self):
        """Return all ERROR-level entries."""
        return [e for e in self.entries if e.level == "E"]

    def get_warnings(self):
        """Return all WARNING-level entries."""
        return [e for e in self.entries if e.level == "W"]

    def get_by_module(self, module):
        """Return all entries from a specific module."""
        return [e for e in self.entries if e.module == module]

    def summary(self):
        """Return a summary dict suitable for issue reporting."""
        return {
            "total_entries": len(self.entries),
            "errors": len(self.get_errors()),
            "warnings": len(self.get_warnings()),
            "milestones": {
                "auth_initialized": self.milestones.auth_initialized,
                "settings_loaded": self.milestones.settings_loaded,
                "hal_discovery_done": self.milestones.hal_discovery_done,
                "wifi_connected": self.milestones.wifi_connected,
                "mqtt_connected": self.milestones.mqtt_connected,
                "audio_initialized": self.milestones.audio_initialized,
                "gui_initialized": self.milestones.gui_initialized,
                "boot_complete": self.milestones.boot_complete,
            },
        }
