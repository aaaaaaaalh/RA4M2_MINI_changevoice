# Project working agreement

- Repository: aaaaaaaalh/RA4M2_MINI_changevoice. The user requires every future project update to be version-controlled here.
- Preserve the working v0.2.0 baseline. Make subsequent changes on a development branch; do not force-push or rewrite published history.
- Update VERSION and CHANGELOG.md for delivered versions. Commit related source, documentation and meaningful tests together. Report the resulting commit/link. If GitHub upload fails, state that clearly; never claim a local-only version is uploaded.
- Keep the Chinese MATLAB interface and both real-time and offline processing. Preserve original duration and speech rate.
- PC quality is the current priority. Specific character/person voice conversion is planned but not implemented. Clarify GPU, execution environment, voice target and data availability before choosing a model.
- Hardware: RA4M2MINI, microphone AO to P001, G to GND, power to 3.3V, DO unused. SCI9/CH340E, nominal921600 baud,16kHz,256 samples. Do not silently change the wire protocol or pin configuration.
- Do not conflate host mocks/static checks with MATLAB, Renesas cross-compilation or hardware tests. Record evidence separately.
- Do not commit user recordings, model weights, credentials, build products or local machine settings. Preserve existing third-party copyright/license notices.
