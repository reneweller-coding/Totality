/**
 * @file vectest.cpp
 * @brief Lane paths against the scalar reference, bit for bit.
 *
 * Built once per vector path (Tests/CMakeLists.txt): AVX2, NEON through the x86 shim, and scalar. Every lane of every
 * vector operation, of the half-band filters and of the percussion kit's kernel must equal the float instantiation
 * exactly -- not within a tolerance.
 * @note The operations and the half-band are copied from Ephemeris `Tests/vectest.cpp` at d047d79 (27.09.2026); the
 *       kit section follows Phosphene's.
 */
#include "tot/Halfband.h"
#include "tot/Params.h"
#include "tot/Vec.h"
#include "tot/synth/Kit.h"
#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

using namespace tot;
using namespace tottest;

namespace {

constexpr int W = kVecWidth;

float testValue(uint32_t i)
{
    uint32_t h = i * 2654435761u ^ 0x9E3779B9u;
    h ^= h >> 15; h *= 2246822519u; h ^= h >> 13;
    const float u = static_cast<float>(h) / 4294967296.0f;
    switch (i % 5) {
    case 0:  return u * 2.0f - 1.0f;
    case 1:  return (u * 2.0f - 1.0f) * 1.0e-6f;
    case 2:  return (u * 2.0f - 1.0f) * 1000.0f;
    case 3:  return u + 0.5f;
    default: return -(u + 0.25f);
    }
}

bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

void testOps()
{
    section("vector operations, lane by lane");
    int bad = 0, total = 0;
    float a[8], b[8], c[8];
    for (uint32_t round = 0; round < 2000; ++round) {
        for (int i = 0; i < 8; ++i) { a[i] = testValue(round * 8 + i); b[i] = testValue(round * 8 + i + 11); c[i] = testValue(round * 8 + i + 23); }
        const VecF va = loadLanes<VecF>(a), vb = loadLanes<VecF>(b), vc = loadLanes<VecF>(c);
        const VecF r[] = {
            va + vb, va - vb, va * vb, va / vb, vfmadd(va, vb, vc), vfnmadd(va, vb, vc),
            vmin(va, vb), vmax(va, vb), vsqrt(vabs(va)), vabs(va), vfloor(va), vselect(vlt(va, vb), vc, va),
        };
        for (int l = 0; l < W; ++l) {
            const float x = a[l], y = b[l], z = c[l];
            const float s[] = {
                x + y, x - y, x * y, x / y, vfmadd(x, y, z), vfnmadd(x, y, z),
                vmin(x, y), vmax(x, y), vsqrt(vabs(x)), vabs(x), vfloor(x), vselect(vlt(x, y), z, x),
            };
            for (size_t k = 0; k < sizeof(s) / sizeof(s[0]); ++k) {
                ++total;
                if (!sameBits(laneOf(r[k], l), s[k])) ++bad;
            }
        }
    }
    check(bad == 0, "12 operations identical to scalar", fmt("%d of %d lanes differ", bad, total));
}

void testHalfband()
{
    section("half-band lanes against scalar");
    const HalfbandDesign d = designHalfband(96.0, 0.1);
    HalfbandDown<VecF> vd;
    HalfbandUp<VecF> vu;
    HalfbandDown<float> sd[8];
    HalfbandUp<float> su[8];
    vd.setup(d); vu.setup(d);
    for (auto& s : sd) s.setup(d);
    for (auto& s : su) s.setup(d);
    int bad = 0;
    float a[8], b[8];
    for (uint32_t n = 0; n < 5000; ++n) {
        for (int l = 0; l < 8; ++l) { a[l] = testValue(n * 8 + l); b[l] = testValue(n * 8 + l + 5); }
        const VecF yd = vd.process(loadLanes<VecF>(a), loadLanes<VecF>(b));
        VecF u0, u1;
        vu.process(loadLanes<VecF>(a), u0, u1);
        for (int l = 0; l < W; ++l) {
            float s0, s1;
            su[l].process(a[l], s0, s1);
            if (!sameBits(laneOf(yd, l), sd[l].process(a[l], b[l]))) ++bad;
            if (!sameBits(laneOf(u0, l), s0) || !sameBits(laneOf(u1, l), s1)) ++bad;
        }
    }
    check(bad == 0, "decimator and interpolator identical to scalar", fmt("%d differing samples", bad));
}

/**
 * The kit: the default twelve lanes (every engine, the metal table, bursts, chokes, the auto-pan), a pattern of hits
 * with sub-sample onsets, rendered once through the lane path and once through the scalar reference.
 */
void testKit()
{
    section("percussion kit lanes against the scalar kit");
    auto params = std::make_unique<ParamStore>();
    PercKit vk, sk;
    vk.prepare(48000.0);
    sk.prepare(48000.0);
    for (PercKit* k : { &vk, &sk }) {
        k->setTempo(130.0);
        for (int l = 0; l < kPercLanes; ++l) {
            float v[64];
            params->readModule(Module::Perc, l, v);
            k->update(l, v, 9, 0);
        }
    }
    int bad = 0, samples = 0;
    float peak = 0.0f;
    for (int block = 0; block < 1500; ++block) {
        if (block % 3 == 0) {
            const int lane = (block / 3) % kPercLanes;
            const float vel = 0.4f + 0.05f * static_cast<float>(block % 12);
            const double late = static_cast<double>(block % 7) / 7.0;
            vk.trigger(lane, vel, block % 5 == 0 ? 2 : 0, late);
            sk.trigger(lane, vel, block % 5 == 0 ? 2 : 0, late);
        }
        vk.processLanes(32);
        sk.processLanesWith<float>(32);
        for (int i = 0; i < 32 * PercKit::kStride; ++i) {
            if (!sameBits(vk.laneL()[i], sk.laneL()[i]) || !sameBits(vk.laneR()[i], sk.laneR()[i])) ++bad;
            peak = std::max(peak, std::fabs(sk.laneL()[i]));
            ++samples;
        }
    }
    check(bad == 0, "every lane identical to scalar", fmt("%d of %d samples differ", bad, samples));
    check(peak > 0.01f && std::isfinite(peak), "the kit sounds", fmt("peak %.3f", static_cast<double>(peak)));
}

} // namespace

int main()
{
    std::printf("tot_vectest: path %s, %d lanes\n", kVecPathName, W);
#if defined(TOT_EXPECT_PATH)
    check(std::strcmp(kVecPathName, TOT_EXPECT_PATH) == 0, "built for the expected path", fmt("expected %s, got %s", TOT_EXPECT_PATH, kVecPathName));
#endif
    testOps();
    testHalfband();
    testKit();
    return finish();
}
