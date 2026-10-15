-- Preserve the collection/map and every unrelated buff field.
return function(collections)
    assert(#collections == 1 and tostring(collections[1].guid):lower() == "02022e86-8efe-40cb-8f6f-d22071cc3be3",
        "ESM Rested: unsupported 1.0 buff collection")
    local target, count = nil, 0
    for _, buff in ipairs(collections[1].data.buffs) do
        if buff.buffTypeId.value == 3451821184 then target = buff; count = count + 1 end
    end
    assert(count == 1 and target.slot == "Rested"
        and tostring(target.impact):lower() == "91715e7c-46b6-4dae-9058-09780481a9bb",
        "ESM Rested: 1.0 buff identity mismatch")
    target.uiValues = {}
    assert(#target.uiValues == 0, "ESM Rested: UI value write failed")
    return 0
end
