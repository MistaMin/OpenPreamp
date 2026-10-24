#!/usr/bin/env python3
"""Copies the license texts of bundled third-party code into Licenses/third-party.
Run after changing the JUCE version (needs a configured build in build3/ so the sources are present)."""
import glob, os, re, shutil, sys
root = os.path.join(os.path.dirname(__file__), '..')
D = os.path.join(root, 'build3/_deps')
J = os.path.join(D, 'juce-src/modules')
out = os.path.join(root, 'Licenses/third-party')
shutil.rmtree(out, ignore_errors=True); os.makedirs(out)

def find(pattern):
    hits = glob.glob(pattern)
    if not hits: sys.exit('missing: ' + pattern)
    return hits[0]
def cp(src, name): shutil.copyfile(find(src), os.path.join(out, name))

vst3 = 'juce_audio_processors*/format_types/VST3_SDK'
cp(f'{J}/{vst3}/LICENSE.txt', 'VST3-SDK-LICENSE-MIT.txt')
for n in ['lv2', 'lilv', 'serd', 'sord', 'sratom']:
    cp(f'{J}/juce_audio_processors*/format_types/LV2_SDK/{n}/COPYING', f'LV2-{n}-COPYING.txt')
cp(D + '/clap_juce_extensions-src/clap-libs/clap/LICENSE', 'CLAP-LICENSE.txt')
cp(D + '/clap_juce_extensions-src/clap-libs/clap-helpers/LICENSE', 'CLAP-helpers-LICENSE.txt')
cp(D + '/clap_juce_extensions-src/LICENSE.md', 'clap-juce-extensions-LICENSE.txt')
cp(J + '/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt', 'AudioUnitSDK-LICENSE-Apache-2.0.txt')
# Bundled libraries. File locations differ between JUCE versions, so each entry lists alternatives
# ('required' ones must exist, the others are copied when this JUCE version has them).
def cp_any(patterns, name, required=False):
    for pat in patterns:
        hits = glob.glob(f'{J}/{pat}')
        if hits:
            shutil.copyfile(hits[0], os.path.join(out, name)); return
    if required: sys.exit('missing: ' + patterns[0])

cp_any(['juce_audio_formats/codecs/flac/Flac Licence.txt'], 'FLAC-Licence.txt', True)
cp_any(['juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt'], 'OggVorbis-Licence.txt')
cp_any(['juce_audio_formats/codecs/oggvorbis/libvorbis-*/COPYING', 'juce_audio_formats/codecs/vorbis/COPYING'], 'libvorbis-COPYING.txt', True)
cp_any(['juce_audio_formats/codecs/ogg/COPYING'], 'libogg-COPYING.txt')
cp_any(['juce_audio_formats/codecs/opus/opus/COPYING'], 'libopus-COPYING.txt')
cp_any(['juce_audio_formats/codecs/opus/opus/LICENSE_PLEASE_READ.txt'], 'libopus-LICENSE_PLEASE_READ.txt')
cp_any(['juce_audio_formats/codecs/opus/opusfile/COPYING'], 'opusfile-COPYING.txt')
cp_any(['juce_audio_formats/codecs/opus/libopusenc/COPYING'], 'libopusenc-COPYING.txt')
cp_any(['juce_graphics/image_formats/pnglib/LICENSE'], 'libpng-LICENSE.txt', True)
cp_any(['juce_graphics/image_formats/jpglib/README'], 'libjpeg-IJG-README.txt', True)
cp_any(['juce_graphics/image_formats/libwebp/COPYING'], 'libwebp-COPYING.txt')
cp_any(['juce_graphics/drawables/lunasvg/LICENSE'], 'lunasvg-LICENSE.txt')
cp_any(['juce_graphics/drawables/lunasvg/plutovg/LICENSE'], 'plutovg-LICENSE.txt')
cp_any(['juce_graphics/fonts/harfbuzz/COPYING'], 'HarfBuzz-COPYING.txt', True)
cp_any(['juce_graphics/unicode/sheenbidi/LICENSE'], 'SheenBidi-LICENSE.txt', True)
# zlib: newer JUCE ships a LICENSE file, older ones only the notice at the top of zlib.h
if glob.glob(f'{J}/juce_core/zip/zlib/LICENSE'):
    cp_any(['juce_core/zip/zlib/LICENSE'], 'zlib-LICENSE.txt', True)
else:
    zh = open(find(J + '/juce_core/zip/zlib/zlib.h')).read()
    open(os.path.join(out, 'zlib-LICENSE.txt'), 'w').write(re.search(r'/\* zlib\.h.*?\*/', zh, re.S).group(0) + '\n')
# VST3 usage guidelines (trademark / logo rules) shipped with the SDK
usage = glob.glob(f'{J}/{vst3}/VST3_Usage_Guidelines.pdf')
if usage: shutil.copyfile(usage[0], os.path.join(out, 'VST3-Usage-Guidelines.pdf'))
open(os.path.join(out, 'JUCE-NOTICE.txt'), 'w').write('''JUCE
HybridEQ is built with the JUCE framework (https://juce.com). The official HybridEQ binaries are built under
the publisher's JUCE commercial license; end users do not need a JUCE license to install or use them.
JUCE is not covered by the MIT licenses of HybridEQ and GoodLookinUI. Anyone building their own version
must comply with the JUCE license that applies to their build (JUCE 9: https://juce.com/legal/juce-9-licence/).
Libraries bundled inside JUCE keep their own licenses; the ones compiled into HybridEQ are reproduced in
this folder: zlib, libpng, libjpeg (IJG), libwebp, FLAC, Ogg, Vorbis, Opus, HarfBuzz, SheenBidi and LunaSVG. The plug-in format
SDKs are also reproduced here: VST3 (MIT), LV2 (and lilv/serd/sord/sratom), CLAP and Audio Unit.
''')
open(os.path.join(out, 'VST-TRADEMARK-NOTICE.txt'), 'w').write('''VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.
The VST3 plug-in format uses the Steinberg VST3 SDK (MIT license); see VST3-SDK-LICENSE-MIT.txt and
VST3-Usage-Guidelines.pdf. CLAP is an open plug-in standard (https://cleveraudio.org); LV2 is an open
plug-in standard (https://lv2plug.in). Audio Units is a technology of Apple Inc.; AAX is a technology of
Avid Technology, Inc.
''')
sdk = os.path.normpath(os.path.join(root, '../aax-sdk-2-8-1/LICENSE.txt'))
aax_out = os.path.join(out, 'AAX-SDK')
if os.path.isfile(sdk):
    os.makedirs(aax_out, exist_ok=True)
    shutil.copyfile(sdk, os.path.join(aax_out, 'LICENSE.txt'))
print(len(os.listdir(out)), 'files in', out)
