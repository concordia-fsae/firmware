# conUDS

A UDS client written for use by the Concordia FSAE team.

# About

This UDS client (written in Rust) uses the attached CAN dongle (only socketcan supported for now)
to interact with a given ECU using the UDS protocl on the attached CAN bus.

## Supported Functionality

So far, this UDS client supports:

- ECU Resets
- Reading the current diagnostic session
- Changing the active diagnostic session
- Persistent tester present at 10ms
- App Downloading

# Usage

All examples assume a node name from the UDS manifest and a SocketCAN device such as `can0`.

Read the current diagnostic session:

```bash
conUDS -n mcu -t can0 read-session
```

Change the ECU into a different diagnostic session:

```bash
conUDS -n mcu -t can0 set-session extended
conUDS -n mcu -t can0 set-session programming
```

Supported session names are:

- `default`
- `programming`
- `extended`
- `safety-system`

Start persistent tester present and keep it running until `Ctrl+C`:

```bash
conUDS -n mcu -t can0 persistent-tester-present
```

## Functionality

Read the information provided by the `--help` flag for the most up-to-date information about the features
that this application supports.
