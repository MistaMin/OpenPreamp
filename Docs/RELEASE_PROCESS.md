# OpenPreamp release process

1. **Freeze the source.** Set VERSION, update CHANGELOG.md and RELEASE_NOTES.md,
   finish the saved design CSVs, and commit the source, screenshots and manual.
   Keep private reference documents, SDKs and AAX binaries out of Git history.
2. **Build production formats.** Use build-release, developer mode OFF, Release,
   arm64 and x86_64, and macOS deployment target 11.0. Build OpenPreamp_VST3,
   OpenPreamp_AU, OpenPreamp_LV2 and OpenPreamp_CLAP. OpenPreamp_AAX is a separate
   private artifact when the SDK is available; never include it in staging.
3. **Validate.** Run OpenPreampSmoke and OpenPreampADAA natively and via Rosetta;
   inspect screenshots at minimum/default/maximum sizes. Validate AU and format
   metadata/loading. Test in DAWs, including mono/stereo/M/S, automation, reopen,
   bypass and latency. Automated checks do not replace a listening session.
4. **Build the manual.** Run tools/build_user_manual.py using the bundled Python
   runtime; it uses the smoke test screenshots. Render the PDF and review every
   page. Ship the PDF plus the release notes and every license text.
5. **Stage only public formats.** Run tools/package_openpreamp.py stage. It
   rejects developer builds, non-universal binaries and missing manual/licenses.
   It explicitly excludes AAX. Inspect the staged directory before upload.
6. **Sign and notarize.** Sign the VST3/AU/CLAP bundles and LV2 executable with
   Developer ID Application, hardened runtime and timestamp. Verify signatures,
   generate the manifest, create/sign the DMG, submit it with notarytool, staple
   the accepted ticket and verify Gatekeeper assessment. Keychain identities and
   notary credentials are local; do not commit or export them.
7. **Publish the reviewed files.** Push the MIT source to MistaMin/OpenPreamp;
   tag v1.0.0 and publish the signed/notarized Mac DMG, manual PDF and archive
   checksums. README screenshots and manual links must resolve. Inspect release
   assets again: no AAX, private SDK, Reference folder or signing credentials.

Signing reference: https://developer.apple.com/developer-id/
Notarization reference: https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution
JUCE licensing reference: https://juce.com/legal/juce-9-licence/

The project source is MIT. Official binary terms and third-party permissions are
separate; the project MIT license does not cover JUCE or the optional AAX SDK.
Public publishing does not establish that an independent builder has permission
to redistribute dependencies under different terms.
