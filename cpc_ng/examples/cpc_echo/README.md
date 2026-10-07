# cpc_echo

Simplest CPC application pair: a **primary** (Host MCU) and **secondary** (co-processor) exchange raw payloads on endpoint **90** (IDs **0–89** are reserved for Silicon Labs).

| Role | Project | Behavior |
|------|---------|----------|
| **Secondary** | [`secondary/`](secondary/) | Listens, echoes each RX buffer back |
| **Primary** | [`primary/`](primary/) | Connects, sends `cpc_echo` **10** times, validates each reply, closes, prints PASS |

## Notice — app structure

This example is intentionally a **minimal CPC API demo** (connect/listen/send/recv/close, short callback, deferred work in `app_process_action()`). It uses a simple poll loop so the sample stays easy to read.

That is **not** a template for a production FreeRTOS app. Under a kernel, the start task busy-polls and relies on running **below** the CPC task priority. Do not copy that pattern as-is. A proper kernel design would block a dedicated task on a semaphore/queue signaled from the CPC callback.

Both apps use sl_main (`app_init` / `app_process_action`). With FreeRTOS, `sl_main_start_task_should_continue()` keeps the start task running.

## Output (primary SWO)

```
cpc_echo: primary start, 10 echoes on ep 90
cpc_echo: exchange
cpc_echo: echo 1/10
…
cpc_echo: echo 10/10
cpc_echo: close
cpc_echo: PASS 10/10 echoes
TEST(cpc_echo, echo_exchange) PASS
-----------------------
1 Tests 0 Failures 0 Ignored
OK
```

Bad payloads trip `EFM_ASSERT` (`DEBUG_EFM`).

## Build

Requires `slt`: [Command Line Development](https://docs.silabs.com/command-line-development/1.0.0/command-line-environment-setup-use/install-configure-command-line-packages).

```sh
slt install -f pkg.slt
```

Pass the target **board** and **CPC driver** with `--with`. The same projects build for different boards and PHYs (UART, SDIO, …); Match a primary driver to a secondary driver on the same physical link.

Example — UART VCOM on `BRD4186C`:

```sh
mkdir -p build/secondary && cd build/secondary
$(slt where slc-cli)/slc generate ../../cpc_echo_secondary.slcp -d . \
  --slconf ../../user.slconf \
  --with brd4186c,cpc_ng_drv_uart_secondary:vcom

mkdir -p build/primary && cd build/primary
$(slt where slc-cli)/slc generate ../../cpc_echo_primary.slcp -d . \
  --slconf ../../user.slconf \
  --with brd4186c,cpc_ng_drv_uart_primary:vcom
```

Example — SDIO on `BRD6360A`:

```sh
mkdir -p build/secondary && cd build/secondary
$(slt where slc-cli)/slc generate ../../cpc_echo_secondary.slcp -d . \
  --slconf ../../user.slconf \
  --with brd6360a,cpc_ng_drv_sdio_device_secondary:instance

mkdir -p build/primary && cd build/primary
$(slt where slc-cli)/slc generate ../../cpc_echo_primary.slcp -d . \
  --slconf ../../user.slconf \
  --with brd6360a,cpc_ng_drv_sdio_host_primary:instance
```

Either side can be flashed first; the primary retries connect until the secondary is listening.
