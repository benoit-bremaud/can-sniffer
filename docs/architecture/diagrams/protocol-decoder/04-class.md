# Class diagram — protocol decoder

## Context

This class view identifies the minimum domain types required to decode the documented 29-bit
Infypower identifier and preserve unknown frames. Concrete adapters are intentionally absent.

## Diagram

```mermaid
classDiagram
    class CanFrame {
        +int arbitration_id
        +bytes data
        +bool is_extended_id
        +bool is_error_frame
    }
    class InfypowerIdentifier {
        +int raw
        +int error_code
        +int device_number
        +int command_number
        +int destination_address
        +int source_address
    }
    class DecodedFrame {
        +CanFrame raw_frame
        +InfypowerIdentifier identifier
        +DecodedPayload payload
        +tuple diagnostics
    }
    class DecodedPayload {
        +string description
        +map fields
    }
    class ProtocolDecoder {
        +DecodedFrame decode(CanFrame frame)
    }
    CanFrame --> DecodedFrame
    InfypowerIdentifier --> DecodedFrame
    DecodedPayload --> DecodedFrame
    ProtocolDecoder --> DecodedFrame
```

## Notes

- `CanFrame` is a domain representation, not `can.Message`.
- Mutable dictionaries should not become the long-term public domain API; typed payloads should
  be introduced when command-specific decoding is implemented.
- Error code and address bit masks must be tested against the protocol examples.
