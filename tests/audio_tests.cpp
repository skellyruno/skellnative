#include "skellnative/audio.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace {
int failures = 0;
void check(bool condition, const char* label) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", label); ++failures; }
}
}

int main() {
    using namespace skellnative;
    std::array<float, 8> left{{1.f, -.5f, 0.f, 1.25f, .25f, -.25f, 0.f, 9.f}};
    std::array<float, 8> right{{-.1f, .2f, .3f, -.4f, 1.f, 0.f, -1.f, 2.f}};
    std::array<float, 8> outL{{}};
    std::array<float, 8> outR{{}};
    const float* in[] = {left.data(), right.data()};
    float* out[] = {outL.data(), outR.data()};

    check(copyAudio({in, 2, 8}, {out, 2, 8}) == AudioStatus::ok, "stereo pass-through");
    check(std::memcmp(left.data(), outL.data(), sizeof(left)) == 0, "left bit-exact");
    check(std::memcmp(right.data(), outR.data(), sizeof(right)) == 0, "right bit-exact");

    // Corresponding channels may be edited in place safely.
    float* inPlaceOut[] = {left.data(), right.data()};
    check(copyAudio({in, 2, 8}, {inPlaceOut, 2, 8}) == AudioStatus::ok, "in-place pass-through");
    check(left[0] == 1.f && right[0] == -.1f, "in-place content unchanged");

    // Partial per-channel overlap must copy safely.
    float overlap[5] = {1.f, 2.f, 3.f, 4.f, 5.f};
    const float* overlapIn[] = {overlap};
    float* overlapOut[] = {overlap + 1};
    check(copyAudio({overlapIn, 1, 4}, {overlapOut, 1, 4}) == AudioStatus::ok, "partial overlap");
    check(overlap[1] == 1.f && overlap[4] == 4.f, "memmove semantics");

    // Validation must happen before any channel gets modified.
    outL.fill(42.f);
    outR.fill(42.f);
    const float* brokenIn[] = {left.data(), nullptr};
    check(copyAudio({brokenIn, 2, 8}, {out, 2, 8}) == AudioStatus::invalidBuffer, "reject null input channel");
    check(outL[0] == 42.f && outR[0] == 42.f, "invalid input makes no writes");
    check(copyAudio({in, 2, 8}, {out, 1, 8}) == AudioStatus::incompatibleLayout, "reject channel mismatch");
    check(copyAudio({in, 2, 8}, {out, 2, 7}) == AudioStatus::incompatibleLayout, "reject frame mismatch");
    check(outL[0] == 42.f && outR[0] == 42.f, "layout failure makes no writes");

    check(copyAudio({nullptr, 2, 0}, {nullptr, 2, 0}) == AudioStatus::ok, "zero-frame pass-through");
    check(copyAudio({nullptr, 0, 8}, {nullptr, 0, 8}) == AudioStatus::ok, "zero-channel pass-through");
    check(clearAudio({out, 2, 8}) == AudioStatus::ok, "clear output");
    for (float sample : outL) check(sample == 0.f && !std::signbit(sample), "left cleared positive zero");
    for (float sample : outR) check(sample == 0.f && !std::signbit(sample), "right cleared positive zero");
    check(clearAudio({nullptr, 2, 0}) == AudioStatus::ok, "zero-frame clear");
    float* brokenOut[] = {outL.data(), nullptr};
    outL.fill(13.f);
    check(clearAudio({brokenOut, 2, 8}) == AudioStatus::invalidBuffer, "reject null output channel");
    check(outL[0] == 13.f, "invalid output makes no writes");

    // Passthrough never creates NaNs or changes pre-existing sample bits.
    std::array<float, 3> specials{{-0.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}};
    std::array<float, 3> copied{{}};
    const float* specialIn[] = {specials.data()};
    float* specialOut[] = {copied.data()};
    check(copyAudio({specialIn, 1, 3}, {specialOut, 1, 3}) == AudioStatus::ok, "special values pass-through");
    check(std::memcmp(specials.data(), copied.data(), sizeof(specials)) == 0, "pass-through bit preservation");

    if (failures == 0) std::puts("PASS: SkellNative audio contract tests");
    return failures ? 1 : 0;
}
