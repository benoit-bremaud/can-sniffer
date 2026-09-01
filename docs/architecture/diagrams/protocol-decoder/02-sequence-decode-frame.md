# Sequence diagram — protocol decoder — decode one frame

## Context

This sequence defines the synchronous, side-effect-free decoding path for one received frame.
Capture and UI adapters are outside the decoder boundary.

## Diagram

```mermaid
sequenceDiagram
    participant Capture as Capture use case
    participant Decoder as Protocol decoder
    participant Id as Identifier parser
    participant Payload as Payload decoder
    participant Result as Decode result

    Capture->>Decoder: decode(raw_frame)
    Decoder->>Id: parse(29-bit identifier)
    Id-->>Decoder: address, device, command, error code
    Decoder->>Payload: decode(command, 8-byte payload)
    Payload-->>Decoder: known fields or unknown payload
    Decoder->>Result: assemble raw and decoded values
    Result-->>Capture: immutable decode result
```

## Notes

- Invalid identifiers and payloads produce diagnostics without throwing away the raw frame.
- No UI, filesystem, clock, or hardware dependency is allowed in this sequence.
