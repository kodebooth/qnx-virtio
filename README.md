# qnx-virtio

VirtIO drivers and utilities for QNX.

## Prerequisites

- QNX SDP installed with `QNX_SDP_INSTALL_DIR` environment variable set
- CMake 3.10 or later
- Python 3.x with pytest (for running tests)

## Building

The project uses CMake with presets for different architectures.

### Build for x86_64

```bash
cmake --preset x86_64
cmake --build build/x86_64
```

### Build for aarch64le

```bash
cmake --preset aarch64le
cmake --build build/aarch64le
```

## Testing

Run the test suite using pytest:

```bash
pytest
```

To test a specific architecture:

```bash
pytest --arch=x86_64
# or
pytest --arch=aarch64le
```

## License

MIT - See [LICENSE.md](LICENSE.md) for details.
