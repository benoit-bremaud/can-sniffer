# Class diagram — display settings — preference model and adapters

> **Feature**: issue #26 — [Add persistent display settings](https://github.com/benoit-bremaud/can-sniffer/issues/26)
> **Source specs**: `docs/architecture/specs/display-settings.md` §Preferences, §Rules

## Context

This diagram defines the minimum types needed for validated preferences, persistence, and UI
editing. It excludes capture and protocol classes whose behaviour is unchanged.

## Diagram

```mermaid
classDiagram
    class IdentifierFormat {
        <<enumeration>>
        HEXADECIMAL
        DECIMAL
    }

    class DisplayPreferences {
        <<immutable>>
        +IdentifierFormat identifier_format
        +int numeric_precision
        +bool show_raw_payload
        +bool show_decoded_values
        +bool show_diagnostics
        +bool show_temporal_statistics
        +defaults() DisplayPreferences
    }

    class PreferencesRepository {
        <<protocol>>
        +load() DisplayPreferences
        +save(preferences)
    }

    class QtSettingsRepository {
        +load() DisplayPreferences
        +save(preferences)
    }

    class SettingsWidget {
        +set_preferences(preferences)
        +preferences_changed
        +restore_defaults()
    }

    IdentifierFormat <-- DisplayPreferences
    PreferencesRepository <|.. QtSettingsRepository
    QtSettingsRepository --> DisplayPreferences
    SettingsWidget --> DisplayPreferences
    SettingsWidget --> PreferencesRepository
```

## Notes

- The repository protocol exists because persistence has a real test-substitution boundary.
- Validation belongs to `DisplayPreferences`; storage only converts stable primitive values.
- `SettingsWidget` edits preferences but does not own capture or decoding rules.
