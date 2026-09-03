"""Qt persistence adapter for display preferences."""

import logging

from PySide6.QtCore import QSettings

from can_sniffer.preferences import DisplayPreferences

logger = logging.getLogger(__name__)


class QtSettingsRepository:
    """Store display preferences in an injected QSettings instance."""

    _PREFIX = "display/"

    def __init__(self, settings: QSettings) -> None:
        self._settings = settings

    def load(self) -> DisplayPreferences:
        """Load preferences or domain defaults after a global storage failure."""
        self._settings.sync()
        if self._settings.status() is not QSettings.Status.NoError:
            logger.warning("Cannot read display preferences; using defaults")
            return DisplayPreferences.defaults()
        keys = (
            "identifier_format",
            "numeric_precision",
            "show_raw_payload",
            "show_decoded_values",
            "show_diagnostics",
            "show_temporal_statistics",
        )
        return DisplayPreferences.from_values(
            {key: self._settings.value(f"{self._PREFIX}{key}") for key in keys}
        )

    def save(self, preferences: DisplayPreferences) -> None:
        """Persist all preference values without interrupting the application on failure."""
        values: dict[str, object] = {
            "identifier_format": preferences.identifier_format.value,
            "numeric_precision": preferences.numeric_precision,
            "show_raw_payload": preferences.show_raw_payload,
            "show_decoded_values": preferences.show_decoded_values,
            "show_diagnostics": preferences.show_diagnostics,
            "show_temporal_statistics": preferences.show_temporal_statistics,
        }
        for key, value in values.items():
            self._settings.setValue(f"{self._PREFIX}{key}", value)
        self._settings.sync()
        if self._settings.status() is not QSettings.Status.NoError:
            logger.warning("Cannot persist display preferences")
