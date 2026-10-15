-- ESM compatibility guard for the observed 1.0 Rested slot permutation.
-- No game bytecode is embedded here. Compare the supplied author's payload
-- with the live vanilla program before accepting this additional revision.
return function(impact, patch_code, patch_shutdown, patch_layout, patch_data)
    local function hash(values)
        local h = 2166136261
        for _, value in ipairs(values) do h = ((h ~ value) * 16777619) & 0xffffffff end
        return h
    end
    if tostring(impact.programGuid):lower() ~= "91715e7c-46b6-4dae-9058-09780481a9bb"
        or impact.id.value ~= 3051912094 or impact.stackSize ~= 256
        or #impact.usedStreams ~= 0 or #impact.queryCallIds ~= 0
        or #impact.code ~= 101 or hash(impact.code) ~= 2827437012
        or #impact.codeShutdown ~= 35 or hash(impact.codeShutdown) ~= 1151015868
        or #impact.data ~= 353 or hash(impact.data) ~= 855777475
        or #impact.dataLayout ~= 23 then return false end

    -- Compiler variable identities are distinct from the author's generated
    -- FNV(debug-name) identities. Pin the observed vanilla identities as well.
    local names = {441743926,2888693178,1408611773,2317951013,3930131906,2051517989,
        476080012,2714597011,3626796097,2811652970,1839082610,181100535,3950593566,
        291451096,3766711260,2816281919,2997381587,2119169691,2475976570,4046702943,
        901315702,2881211152,2575410331}
    local permutation = {}
    for i, actual in ipairs(impact.dataLayout) do
        local found
        for j = 1, 23 do
            if actual.dbgName == patch_layout[j].dbgName then found = j; break end
        end
        if not found or permutation[found] then return false end
        local expected = patch_layout[found]
        if actual.name.value ~= names[i] or actual.configId.value ~= expected.configId.value
            or actual.type.value ~= expected.type.value or actual.size ~= expected.size
            or actual.offsetInBytes ~= (i - 1) * 16 then return false end
        permutation[found] = i
        for byte = 1, actual.size do
            local expected_byte = patch_data[expected.offsetInBytes + byte]
            -- The author's intended enhancement changes these two multipliers.
            if byte == 1 and actual.dbgName == "Stamina_Max_Increase::value" then expected_byte = 8 end
            if byte == 1 and actual.dbgName == "Stamina_Recharge_Increase::value" then expected_byte = 4 end
            if impact.data[actual.offsetInBytes + byte] ~= expected_byte then return false end
        end
    end
    local function same_relocated_prefix(actual, expected, words, halts)
        local p = 1
        while p <= words do
            local op = expected[p]
            if actual[p] ~= op then return false end
            if op == 11 then
                local slot = permutation[(expected[p + 1] & 0xffff) + 1]
                if not slot or actual[p + 1] ~= 0xffff0000 + slot - 1 then return false end
                p = p + 2
            elseif op == 19 then
                if actual[p + 1] ~= expected[p + 1] or actual[p + 2] ~= expected[p + 2] then return false end
                p = p + 3
            elseif op == 12 or op == 27 then
                p = p + 1
            else return false end
        end
        if p ~= words + 1 or #actual ~= words + halts then return false end
        for i = p, #actual do if actual[i] ~= 30 then return false end end
        return true
    end
    return same_relocated_prefix(impact.code, patch_code, 98, 3)
        and same_relocated_prefix(impact.codeShutdown, patch_shutdown, 34, 1)
end
