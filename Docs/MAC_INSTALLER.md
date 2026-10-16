# macOS system installer

The 1.1.1 DMG contains `OpenPreamp-1.1.1-Installer.pkg`, the illustrated manual
and licenses. Quit audio hosts and run the PKG. Administrator authorization is
required. The installer places universal Apple Silicon/Intel plugins in:

| Format | Destination |
|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/OpenPreamp.vst3` |
| AU | `/Library/Audio/Plug-Ins/Components/OpenPreamp.component` |
| CLAP | `/Library/Audio/Plug-Ins/CLAP/OpenPreamp.clap` |
| LV2 | `/Library/Audio/Plug-Ins/LV2/OpenPreamp.lv2` |

Bundle relocation and home-directory installation are disabled. Existing copies
in a user's home cannot redirect the payload away from these system folders.
The manual, notices and cleanup helper are installed in
`/Library/Application Support/OpenGrid/OpenPreamp`.

After successful installation in the graphical Installer, an unprivileged helper
checks the logged-in console user's four matching plugin paths. If matching
older/current copies exist, a dialog lists each path/version and asks to move
them to that user's Trash. **Move to Trash is the default button**, as requested;
**Keep User Copies** and Escape decline. Nothing moves until the user responds.
There is no permanent deletion and no root-powered user-folder cleanup.

Known newer copies, renamed comparison plugins, unrelated bundle identities,
symlinked folders/bundles and other accounts are retained. Versions without
usable metadata are listed as unknown if their identity matches. Paths, bundle
identity/version and inode/timestamp are rechecked after confirmation. Failures
are reported and retained copies may continue to shadow the system installation.

Unattended installs retain user copies and do not wait for a dialog. The user
can run `Check User Copies.command` in the support folder later to review them.
The helper refuses to perform cleanup as root. This development machine's
installed comparison plugins are not modified during package validation.

## Build and verify

Reuse the already signed universal plugin bundles; no DSP rebuild is required:

```sh
python3 tools/build_openpreamp_installer.py \
  --plugins dist/OpenPreamp-1.1.1-macOS/Plugins \
  --work build/installer-1.1.1 \
  --out dist/OpenPreamp-1.1.1-Installer-DMG \
  --application-identity 'Developer ID Application: YOUR NAME (TEAM)' \
  --installer-identity 'Developer ID Installer: YOUR NAME (TEAM)'
```

The builder refuses existing work/output folders. It compiles the native helper
for both architectures, runs fixture-based consent/identity/version/symlink
checks on each architecture, disables bundle relocation in component metadata,
and signs the product installer. The PKG has one payload containing all four
formats, documentation and the helper. Inspect it with `pkgutil --expand-full`
and verify payload paths, scripts, signatures and distribution settings before
publication. Do not run a live install on the development machine for testing.

Notarize and staple the PKG, create a DMG from its output folder, sign the DMG,
then notarize/staple and validate the DMG. Update release checksums after stapling.
Both the PKG and enclosing DMG must pass signature and Gatekeeper assessment.
Publish them on the 1.1.1 release and update its installation notes.
The plugin DSP binary source revision and the installer source revision are
recorded separately; the measurement renderer provenance remains unchanged.

Apple references: [distribution XML](https://developer.apple.com/library/archive/documentation/DeveloperTools/Reference/DistributionDefinitionRef/Chapters/Distribution_XML_Ref.html),
[move to Trash](https://developer.apple.com/documentation/foundation/filemanager/trashitem(at:resultingitemurl:)).
