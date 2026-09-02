# CAN frame cadence jitter analysis

> **Feature**: epic #23 — [Add CAN frame cadence jitter analysis](https://github.com/benoit-bremaud/can-sniffer/issues/23)
> **Status**: planned

## Objective

Extend the existing read-only capture analysis so an operator can identify variations in
the arrival cadence of each CAN arbitration identifier.

## Scope

For each arbitration identifier, the analyzer groups captured frames in capture order and
calculates the intervals between consecutive timestamps. It keeps the existing count,
first timestamp, last timestamp, average period, and frequency metrics, and adds:

- minimum observed interval;
- maximum observed interval;
- maximum absolute deviation from the average period.

The statistics remain descriptive. Version 1 does not classify frames as anomalous, apply a
tolerance threshold, trigger alarms, or transmit CAN frames.

## Rules

1. Intervals are calculated independently for each arbitration identifier.
2. The existing average period remains `(last_timestamp - first_timestamp) / (count - 1)`
   when at least two valid positive intervals are available.
3. Minimum and maximum interval values are derived from valid positive consecutive
   intervals.
4. Maximum absolute deviation is the greatest `abs(interval - average_period)` among the
   valid intervals.
5. An identifier with no valid positive interval exposes unavailable variation metrics and
   does not cause the capture analysis to fail. A single valid interval is reported as both
   the minimum and maximum, with zero maximum deviation.
6. Empty captures continue to return no statistics.
7. Timestamps that do not increase are ignored for interval metrics. The analyzer remains
   deterministic and does not reorder captured records.
8. The domain analysis remains independent from PySide6, python-can, SocketCAN, and
   filesystem I/O.

## Presentation

The existing statistics area displays the three additional values for each identifier.
Unavailable values use the existing `n/a` convention. The feature does not change capture,
filtering, export, or protocol decoding behaviour.

## Verification

Tests must cover:

- regular intervals;
- irregular intervals and the resulting min/max/deviation values;
- separate identifiers;
- empty captures;
- one occurrence and insufficient valid intervals;
- duplicate timestamps;
- non-monotonic timestamps;
- unchanged existing period and frequency calculations.
