#include "events.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

unsigned checks = 0;
void Require(bool value, const char* label) {
    ++checks;
    if (!value) { std::cerr << label << '\n'; std::exit(1); }
}
template<class T> void Put(std::array<unsigned char, 232>& data, std::size_t offset, T value) {
    std::memcpy(data.data() + offset, &value, sizeof(value));
}
int main() {
    using namespace Probe;
    std::array<unsigned char, 232> data{};
    Row row{};
    Put(data, 8, std::uint32_t{42}); Put(data, 12, std::uint32_t{7001}); Put(data, 16, std::uint32_t{19});
    Require(DecodeEvent(Kind::Attempt, data.data(), 24, row), "manual attempt parsed");
    Require(row.player == 42 && row.target == 7001 && row.offer == 19, "attempt identities preserved");
    for (std::size_t length = 0; length < 24; ++length)
        Require(!DecodeEvent(Kind::Attempt, data.data(), length, row), "truncated event rejected");
    data[20] = 1; data[21] = 0;
    Require(DecodeEvent(Kind::Loot, data.data(), 120, row) && row.flags == 1, "loot-all flag decoded");
    data[21] = 1;
    Require(DecodeEvent(Kind::Loot, data.data(), 120, row) && row.flags == 257, "all-players flag separate");
    Require(!DecodeEvent(Kind::Loot, data.data(), 24, row), "attempt is not a loot event");
    Put(data, 144, std::uint32_t{44}); Put(data, 160, std::uint32_t{8002}); Put(data, 188, std::int32_t{-12});
    Require(DecodeEvent(Kind::Hit, data.data(), data.size(), row) && row.player == 44 && row.target == 8002 && row.healthChange == -12,
        "hit fields preserve source, target and signed damage");
    Require(DecodeEvent(Kind::Death, data.data(), 16, row) && row.target == 42 && !row.player, "death does not invent kill credit");
    Require(DecodeEvent(Kind::Transformed, data.data(), 80, row) && row.target == 42, "loot transformation target parsed");
    Require(!DecodeEvent(Kind::Candidate, data.data(), data.size(), row), "candidate is not an event");
    Require(!DecodeEvent(Kind::Death, nullptr, 16, row), "null rejected");
    Put(data, 8, std::uint32_t{0});
    Require(!DecodeEvent(Kind::Death, data.data(), 16, row), "empty target rejected");
    constexpr std::int64_t unit = 4294967296LL;
    const std::int64_t origin[3]{}, boundary[3]{unit * 2, 0, 0}, outside[3]{unit * 2 + unit / 100, 0, 0};
    Require(Distance(origin, boundary) == HarvestRadiusMetres && Distance(origin, boundary) == CorpseRadiusMetres, "both ranges exactly two metres");
    Require(Distance(origin, outside) > CorpseRadiusMetres, "outside two metres rejected");
    const std::int64_t lowest[3]{(std::numeric_limits<std::int64_t>::min)(), 0, 0};
    const std::int64_t highest[3]{(std::numeric_limits<std::int64_t>::max)(), 0, 0};
    Require(std::isfinite(Distance(lowest, highest)) && Distance(lowest, highest) > 4e9, "fixed-point difference cannot overflow");
    std::cout << checks << " diagnostic checks passed\n";
}
