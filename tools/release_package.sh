#!/bin/bash
# Signs the Release bundles, builds the macOS bundle archive for the OpenGrid Plugin Manager,
# and notarises it. No DMG or PKG is made any more. The Standalone app is not shipped.
# Run it in your own Terminal (codesign / notarytool need your interactive keychain access).
# Prerequisite: a universal Release build in build-release-1.3.0/
# Usage: tools/release_package.sh [VERSION]      (default: the VERSION file)
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION="${1:-$(cat VERSION)}"
APP_ID="Developer ID Application: Marcos Deida (ZQYU67JUBH)"
ART="build-release-1.3.0/OpenPreamp_artefacts/Release"
OUT="dist/OpenPreamp-$VERSION"
ZIP="$OUT/OpenPreamp-$VERSION-macos-bundles.zip"
AAX_WCGUID="${AAX_WCGUID:-9B14E120-CF31-11F1-BD18-005056928F3B}"
PACE_ACCOUNT="${PACE_ACCOUNT:-mistamin}"

test -s output/pdf/OpenPreamp-User-Manual.pdf || { echo "Build the current PDF manual first" >&2; exit 1; }
if [ "$VERSION" != "$(tr -d '\r\n' < VERSION)" ]; then echo "Version argument differs from VERSION" >&2; exit 1; fi

echo "== Signing identities"
security find-identity -v -p codesigning | grep -E "Developer ID" || { echo "No Developer ID identities found"; exit 1; }

echo "== 1/4 Signing bundles (Developer ID, timestamp, hardened runtime)"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/VST3/OpenPreamp.vst3"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AU/OpenPreamp.component"
codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/CLAP/OpenPreamp.clap"
# LV2 is a plain folder (not a bundle): sign the binary inside it
codesign --force --timestamp --options runtime --sign "$APP_ID" "$ART/LV2/OpenPreamp.lv2/libOpenPreamp.so"

if [ -d "$ART/AAX/OpenPreamp.aaxplugin" ]; then
    echo "== Signing AAX bundle with PACE Eden & Developer ID"
    codesign --force --deep --timestamp --options runtime --sign "$APP_ID" "$ART/AAX/OpenPreamp.aaxplugin"
    /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool sign --verbose \
        --account "$PACE_ACCOUNT" \
        --wcguid "$AAX_WCGUID" \
        --signid "$APP_ID" \
        --in "$ART/AAX/OpenPreamp.aaxplugin" \
        --out "$ART/AAX/OpenPreamp.aaxplugin" \
        --autoinstall on
    /Applications/PACEAntiPiracy/Eden/Fusion/Current/bin/wraptool verify --verbose --in "$ART/AAX/OpenPreamp.aaxplugin"
fi

for b in "$ART/VST3/OpenPreamp.vst3" "$ART/AU/OpenPreamp.component" "$ART/CLAP/OpenPreamp.clap" "$ART/LV2/OpenPreamp.lv2/libOpenPreamp.so"; do
    codesign --verify --deep --strict "$b"
    codesign -dv "$b" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
done
if [ -d "$ART/AAX/OpenPreamp.aaxplugin" ]; then
    codesign --verify --deep --strict "$ART/AAX/OpenPreamp.aaxplugin"
    codesign -dv "$ART/AAX/OpenPreamp.aaxplugin" 2>&1 | grep -E "Authority=Developer ID Application|Timestamp" | head -2
fi

echo "== 2/4 Building the bundle archive (install-tree layout)"
rm -rf "$OUT"; mkdir -p "$OUT/resources/Licenses"
{ cat BINARY_LICENSE.txt; echo; echo; cat THIRD_PARTY_NOTICES.txt; echo; echo; cat LICENSE; echo; echo; cat third_party/GoodLookinUI/LICENSE; } > "$OUT/resources/Licenses/LICENSE.txt"
cp -R Licenses/third-party "$OUT/resources/Licenses/third-party"
mkdir -p "$OUT/resources/Manual"
cp output/pdf/OpenPreamp-User-Manual.pdf Docs/UserManual.md "$OUT/resources/Manual/"
cp README.md RELEASE_NOTES.md "$OUT/resources/"
MIN_OS=11.0 RESOURCES_DIR="$OUT/resources" tools/make_bundle_zip.sh "OpenPreamp" "$VERSION" "$ART" "$OUT"

echo "== 3/4 Notarizing the archive"
xcrun notarytool submit "$ZIP" --keychain-profile AC_NOTARY --wait

echo "== 4/4 Checksum"
(cd "$OUT" && shasum -a 256 "$(basename "$ZIP")" > "OpenPreamp-$VERSION-macos.sha256")
cat "$OUT/OpenPreamp-$VERSION-macos.sha256"
echo "DONE: $ZIP"
echo "Next: write opengrid-manifest.json with tools/write_opengrid_manifest.py once the Windows archive exists."
