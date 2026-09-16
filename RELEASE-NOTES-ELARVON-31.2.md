# ELARVON Core 31.2 desktop wallet update

This release refreshes the desktop wallet while preserving the ELARVON network and wallet data formats.

## Changes

- Uses the official editorial ivory, obsidian, and ELARVON gold visual system.
- Loads ELVN/USD and ELVN/EUR reference prices from `https://elarvon.io/api/price` every minute.
- Includes the native Windows and macOS TLS backend in static wallet builds so HTTPS price updates work in installed applications.
- Stores the last valid price locally and clearly labels cached values when the endpoint is unavailable or stale.
- Shows the ELVN amount and its USD and EUR reference value together on the overview page.
- Adds the official explorer to the Help menu and transaction context menus.
- Uses ELARVON names for the installer, applications, executables, shortcuts, configuration file, URLs, and visible wallet text.
- Disables block-production RPC methods in the desktop wallet process.
- Connects fresh desktop installations to the public ELARVON bootstrap node automatically and uses outbound-only networking by default.
- Keeps chain synchronization, sending, receiving, signing, wallet encryption, and wallet backup features active.

## Mining scope

The desktop `bitcoin-qt` process forces an internal `-disableminingrpc=1` setting, so its RPC console cannot generate or submit blocks. The `bitcoind` daemon does not set this flag. A dedicated public node can therefore continue producing blocks without any consensus or network-format change.

## Price behavior

The response must report `ok: true`, asset symbol `ELVN`, a positive `price.USD`, either a positive `price.EUR` or `fx.USD_EUR`, and a valid ISO `updatedAt` timestamp. Responses older than 15 minutes are marked as cached. Invalid responses never replace the last valid cached value.
