# Firmware artifacts

- Keep generated application firmware in ignored `.pio/`, `release/` or `.tools/`; do not commit generated BIN/ZIP/UF2 release packages or signing secrets.
- Existing tracked `bin/` bootloader and erase images are upstream provisioning dependencies. Do not delete them merely because their extension is binary.
- The sibling MVT-Relay repository documents signed-package publication in `docs/artifact-storage.md`. Application firmware payloads go to private GitHub Release assets; the portal tracks only manifests/index metadata and its public verification key.
- Build, signed-package upload, production publication and device OTA are separate steps. Check the active server catalog and installed device version; a successful Git push is not evidence of either.
- Stage explicit paths. Do not overwrite other work or rewrite history to reclaim storage without a separately agreed cleanup scope.
