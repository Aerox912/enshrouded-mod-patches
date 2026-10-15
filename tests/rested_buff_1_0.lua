local apply = assert(loadfile("tools/rested_1_0_buff.lua"))()
local function fixture()
    local other = { buffTypeId={value=1}, uiValues={1} }
    local target = {buffTypeId={value=3451821184},slot="Rested",
        impact="91715e7c-46b6-4dae-9058-09780481a9bb",uiValues={42},applyType="ResetTime",isSaved=true}
    local collection={guid="02022e86-8efe-40cb-8f6f-d22071cc3be3",data={buffs={other,target},buffMap={sentinel=7}}}
    return {collection},target,other
end
local collections,target,other=fixture()
local map=collections[1].data.buffMap
assert(apply(collections)==0 and #target.uiValues==0)
assert(target.applyType=="ResetTime" and target.isSaved and other.uiValues[1]==1)
assert(collections[1].data.buffMap==map and map.sentinel==7)
assert(apply(collections)==0)
for _, change in ipairs({
    function(c,t) c[1].guid="foreign" end,
    function(c,t) t.buffTypeId.value=1 end,
    function(c,t) t.slot="Other" end,
    function(c,t) t.impact="foreign" end,
    function(c,t) c[1].data.buffs[3]=t end,
}) do
    local c,t=fixture(); change(c,t)
    assert(not pcall(apply,c))
    assert(t.uiValues[1]==42, "failure must precede mutation")
end
assert(not pcall(apply,{}))
print("PASS: Rested collection identity, coexistence, idempotence and refusal cases")
