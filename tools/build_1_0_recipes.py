"""Build minimal deltas from privately supplied, hash-pinned original scripts.

Never stores originals or game assets. Existing 2026-10-09 patches are prerequisites.
"""
import argparse
import base64
import difflib
import hashlib
import json
from pathlib import Path

SPECS = {
    'hoarder-1.0': ('HoardersHelper', '2b5f3b087508a484dd907de457446903873171106bd5ac06061e04a282dd6253'),
    'blink-1.0': ('ElectricBlink', 'e91de259f8fbfa616f7ee63f06556b4ba96738a2249d7791e9a80c2660de6a94'),
    'creative-forges-1.0': ('creative-gem-forges', '01e083bc3b07acec714df0185424a65285b13897610908d9de725e1a7bcde4b0'),
    'rested-1.0': ('XHL-Rested-Enhanced', '3123d2d0bb9c38a39a30adc1b9979306c009576cce1a04654cf38be31619e29e'),
}


def replace(text, before, after, count=1):
    if text.count(before) != count:
        raise ValueError(f'Expected {count} occurrences of {before!r}')
    return text.replace(before, after)


BLINK_CONTENT = '''local function make_data_hash(bytes)
    local buf = buffer.create(#bytes)
    for i = 1, #bytes do buf:write_u8(bytes[i]) end
    local content = assert(game.assets.create_content(buf), "ESM Blink: content creation failed")
    return game.guid.to_content_hash(content.guid)
end

local function get_vanilla_blink_defaults()
    local container = assert(game.assets.get_resource(VanillaBlinkSoundGuid, "keen::SoundContainerResource", 0),
        "ESM Blink: baseline sound container missing")
    for _, entry in ipairs(container.data.entries) do
        local value = entry.value or entry
        if value.sound then
            local sound = assert(game.assets.get_resource(value.sound, "keen::SoundResource", 0))
            return container.data, assert(sound.data.streamedData, "ESM Blink: expected 1.0 content reference")
        end
    end
    error("ESM Blink: baseline sound entry missing")
end

local function make_streamed_reference(hash)
    local _, reference = get_vanilla_blink_defaults()
    return {
        hash = hash,
        category = { value = reference.category.value },
        bundleId = reference.bundleId,
        sortKey = reference.sortKey,
    }
end

'''


def migrate(name, source):
    newline = '\r\n' if b'\r\n' in source else '\n'
    text = source.decode('utf-8').replace('\r\n', '\n')
    if name == 'hoarder-1.0':
        # Update only the nested hash; retain category, bundleId and sortKey.
        text = replace(text, 'loc.dataHash', 'loc.data.hash', 3)
    elif name == 'blink-1.0':
        start = text.index('local function make_data_hash(bytes)')
        end = text.index('-- Create SoundResource + SoundContainerResource', start)
        text = text[:start] + BLINK_CONTENT + text[end:]
        text = replace(text, '        dataHash = data_hash,', '        streamedData = make_streamed_reference(data_hash),')
        text = replace(text, '    sound_res.data.dataHash = make_data_hash(bytes)',
                       '    sound_res.data.streamedData.hash = make_data_hash(bytes)')
        text = replace(text, '    local data_hash = make_data_hash(bytes)',
                       '    assert(#bytes == pcm.frameCount * 2 and pcm.channels == 1, "ESM Blink: invalid mono PCM size")\n'
                       '    local defaults = get_vanilla_blink_defaults()\n'
                       '    local data_hash = make_data_hash(bytes)')
        text = replace(text, '        gamepadVibrationIntensity = 0.0,',
                       '        gamepadVibrationIntensity = 0.0,\n'
                       '        useMaxCountLimiting = defaults.useMaxCountLimiting,\n'
                       '        maxCount = defaults.maxCount,\n'
                       '        stealingMode = defaults.stealingMode,\n'
                       '        priority = defaults.priority,\n'
                       '        extraRumble = defaults.extraRumble,')
        text = replace(text, 'print(string.format(\n    "[ElectricBlink] Done.',
                       'if UseCustomBlinkSound and sound_mode == "none" then\n'
                       '    error("ESM Blink: required custom sound could not be installed")\nend\n\n'
                       'print(string.format(\n    "[ElectricBlink] Done.')
    elif name == 'rested-1.0':
        guard = Path(__file__).with_name('rested_1_0_guard.lua').read_text(encoding='utf-8')
        text = replace(text, 'local programs = game.assets.get_resources_by_type("keen::ImpactProgram")',
                       'local matches_relocated_1_0 = (function()\n' + guard + '\nend)()\n\n'
                       'local programs = game.assets.get_resources_by_type("keen::ImpactProgram")')
        text = replace(text, 'local wasOriginal = state_matches(impact, ORIGINAL)',
                       'local wasOriginal = state_matches(impact, ORIGINAL) or\n'
                       '    matches_relocated_1_0(impact, PATCH_CODE, PATCH_SHUTDOWN, PATCH_LAYOUT, PATCH_DATA)')
        text = replace(text, 'language.dataHash', 'language.data.hash', 2)
        text = replace(text, 'localization.keenglishDataHash', 'localization.keenglishData.hash', 2)
        text = replace(text, '    return patchedContent', '    return game.guid.to_content_hash(patchedContent.guid)')
        start = text.index('local function restore_rested_ui_values()')
        end = text.index('local PATCH_CODE = {', start)
        buff = Path(__file__).with_name('rested_1_0_buff.lua').read_text(encoding='utf-8')
        text = (text[:start] + 'local function restore_rested_ui_values()\n'
                + '    local apply = (function()\n' + buff + '\nend)()\n'
                + '    return apply(game.assets.get_resources_by_type("keen::BuffTypeCollectionResource"))\nend\n\n'
                + text[end:])
    elif name == 'creative-forges-1.0':
        text = replace(text, '"keen::ecs::InteractionOffer"', '"keen::ecs::InteractionOfferRuntime"', 2)
        anchor = ' local offer=one(d.components,"keen::ecs::InteractionOfferRuntime",e.targetGuid)'
        text = replace(text, anchor, anchor + '''
 local client_offer=one(d.components,"keen::ecs::ClientInteractionOffer",e.targetGuid)
 if tonumber(offer.defaultSequence.value)~=4201246690 or tonumber(offer.defaultVerbId.value)~=3449552028 or
    offer.isDisabled~=false or client_offer.verbId==nil then
  fail(e.targetGuid.." 1.0 native interaction contract changed")
 end''')
    else:
        raise ValueError(f'Unknown migration: {name}')
    return text.replace('\n', newline).encode('utf-8')


def recipe(source, result):
    edits = []
    for op, i, j, a, b in difflib.SequenceMatcher(None, source, result, autojunk=False).get_opcodes():
        if op != 'equal':
            edits.append({'offset': i, 'remove': j-i, 'data': base64.b64encode(result[a:b]).decode('ascii')})
    return {'kind': 'delta', 'input': hashlib.sha256(source).hexdigest(),
            'output': hashlib.sha256(result).hexdigest(), 'edits': edits}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original_mods', type=Path)
    parser.add_argument('recipes', type=Path)
    args = parser.parse_args()
    recipes = json.loads(args.recipes.read_text(encoding='utf-8'))
    for name, (folder, expected) in SPECS.items():
        source = (args.original_mods / folder / 'src/mod.lua').read_bytes()
        if hashlib.sha256(source).hexdigest() != expected:
            raise ValueError(f'{name}: unsupported original script')
        recipes[name] = recipe(source, migrate(name, source))
    args.recipes.write_text(json.dumps(recipes, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
