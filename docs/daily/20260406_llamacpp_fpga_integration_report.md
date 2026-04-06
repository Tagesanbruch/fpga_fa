# 2026-04-06 llama.cpp FPGA Integration Report

## Summary

This iteration completed a first usable `llama.cpp` integration path for the HLS FlashAttention kernel.
The integration is structured around a queue-oriented runtime shared by:

- the standalone XRT runner path
- a new `ggml-fpga` backend inside `llama.cpp`

The backend now builds successfully in software mode and the top-level `llama-cli` / `llama-server` targets also build successfully against it.

The Docker-based KV260 build flow was prepared and validated at the config/script level, but the actual container image build is currently blocked by a host Docker daemon proxy misconfiguration (`127.0.0.1:8888` refused while resolving `ubuntu:22.04`).

## Implemented Pieces

### 1. Queue-oriented runtime

Added a reusable runtime layer:

- `fpga/integration/llama/fa_task_queue_runtime.hpp`
- `fpga/integration/llama/fa_task_queue_runtime.cpp`

Key properties:

- accepts attention tasks in a board-facing ABI aligned with the HLS kernel
- supports three execution modes:
  - `software_hls`
  - `strict_q8_8`
  - `xrt`
- validates the current hardware constraints:
  - `D = 64`
  - `seq_len <= 256`
  - `seq_len` multiple of both `32` and `64`
  - dense Q8.8 layout
- reuses XRT BOs across queued tasks when XRT is enabled

### 2. New ggml backend

Added a new backend entrypoint:

- `inference/xcomp/llama.cpp/ggml/include/ggml-fpga.h`
- `inference/xcomp/llama.cpp/ggml/src/ggml-fpga/ggml-fpga.cpp`

The backend currently offloads only `GGML_OP_FLASH_ATTN_EXT` and only when all of the following are true:

- Q/K/V head dimension is exactly `64`
- `q_len == kv_len`
- `seq_len <= 256`
- `seq_len` is compatible with the current HLS tiling (`32`, `64`)
- no sinks
- `max_bias == 0`
- `logit_softcap == 0`
- mask is either null or contiguous F16

Unsupported cases fall back to the normal backend selection path.

### 3. Task queue behavior inside llama.cpp

The initial backend version drained one task at a time, which did not really exercise the queue runtime.
This was improved so that `ggml-fpga` now batches pending attention tasks up to `queue_depth()` before draining and writing back results.

This is still host-side batching, not an RTL/HLS in-kernel queue, but it aligns the llama.cpp integration with the queue-oriented runtime model and reduces the amount of immediate per-task drain traffic.

### 4. Build system wiring

Updated build wiring so the backend is part of ggml/llama.cpp:

- `inference/xcomp/llama.cpp/ggml/CMakeLists.txt`
- `inference/xcomp/llama.cpp/ggml/src/CMakeLists.txt`
- `inference/xcomp/llama.cpp/ggml/src/ggml-backend-reg.cpp`
- `inference/xcomp/llama.cpp/ggml/src/ggml-fpga/CMakeLists.txt`

### 5. Docker build flow for KV260

Prepared the Docker-based build assets:

- `inference/xcomp/Dockerfile`
- `inference/xcomp/docker-compose.yml`
- `inference/xcomp/build_kv260_llama_fpga.sh`

Notable updates:

- mounted the whole repo into the container, not just `llama.cpp`
- installed `ninja-build`, `pkg-config`, `uuid-dev`
- forced safer llama.cpp configure defaults for this integration:
  - `GGML_NATIVE=OFF`
  - `GGML_CCACHE=OFF`

## Verification

### Local CMake configure

Succeeded with:

```bash
cmake -S inference/xcomp/llama.cpp \
  -B /tmp/llama-fpga-build \
  -G Ninja \
  -DGGML_BACKEND_DL=ON \
  -DGGML_FPGA=ON \
  -DGGML_FPGA_XRT=OFF \
  -DGGML_OPENMP=OFF \
  -DGGML_NATIVE=OFF \
  -DGGML_CCACHE=OFF \
  -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=ON \
  -DLLAMA_BUILD_SERVER=ON
```

### Local backend build

Succeeded with:

```bash
CCACHE_DISABLE=1 cmake --build /tmp/llama-fpga-build --target ggml-fpga -j4
```

Produced:

- `/tmp/llama-fpga-build/bin/libggml-fpga.so`

### Local llama targets build

Succeeded with:

```bash
CCACHE_DISABLE=1 cmake --build /tmp/llama-fpga-build --target llama-cli llama-server -j4
```

Produced:

- `/tmp/llama-fpga-build/bin/llama-cli`
- `/tmp/llama-fpga-build/bin/llama-server`

### Docker compose config

Succeeded with:

```bash
docker compose -f inference/xcomp/docker-compose.yml config
```

### Docker image build

Attempted with:

```bash
docker compose -f inference/xcomp/docker-compose.yml build zynq-builder
```

Current blocker:

```text
failed to resolve source metadata for docker.io/library/ubuntu:22.04:
proxyconnect tcp: dial tcp 127.0.0.1:8888: connect: connection refused
```

This is a host Docker daemon / proxy issue, not a repo script issue.

## Current Backend Scope

The current integration is intentionally narrow and safe:

- square prefill-style flash attention only
- HLS-compatible dimensions only
- Q8.8 quantize -> runtime -> dequantize bridge in host code
- XRT path prepared through the shared runtime API

It is **not** yet a full llama.cpp FPGA backend for all attention modes.
In particular, decode-style `q_len != kv_len` remains unsupported for FPGA offload today.

## Important Limitations

1. The queue optimization is host-side batching only.
2. The HLS kernel ABI is still single-task, so queueing is drained sequentially underneath.
3. Docker KV260 build is blocked by host daemon proxy configuration.
4. Board-side XRT execution is not revalidated in this iteration because `xclbin` generation for KV260 remains a separate platform/link step.

## Next Steps

1. Generate a KV260-compatible `xclbin` from the HLS kernel and enable `GGML_FPGA_XRT=ON` builds.
2. Run the backend on KV260 with a synthetic `FLASH_ATTN_EXT` invocation through `llama-cli` or a dedicated harness.
3. Extend support from square prefill attention to decode-style attention if the hardware path is broadened.
4. Decide whether task queue semantics should remain host-side or be pushed into a future HLS/RTL control wrapper.
