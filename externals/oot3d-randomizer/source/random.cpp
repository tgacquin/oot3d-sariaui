#include "random.hpp"

#include <random>

static bool init = false;
static std::mt19937_64 generator;
static std::mt19937_64 generator_C;

// Initialize with seed specified
void Random_Init(uint32_t seed) {
    init        = true;
    generator   = std::mt19937_64{ seed };
    generator_C = std::mt19937_64{ seed };
}

// Returns a random integer in range [min, max-1]
uint32_t Random(int min, int max, bool forCosmetic /*= false*/) {
    if (!init) {
        // No seed given, get a random number from device to seed
        const auto seed = static_cast<uint32_t>(std::random_device{}());
        Random_Init(seed);
    }
    const uint64_t range = static_cast<uint64_t>(max - min);
    if (!range) return min;
    const uint64_t threshold = -range % range;
    auto& rng = forCosmetic ? generator_C : generator;
    uint64_t value;
    do { value = rng(); } while (value < threshold);
    return static_cast<uint32_t>(min + value % range);
}

// Returns a random floating point number in [0.0, 1.0]
double RandomDouble(bool forCosmetic /*= false*/) {
    auto& rng = forCosmetic ? generator_C : generator;
    return (rng() >> 11) * (1.0 / 9007199254740992.0);
}
