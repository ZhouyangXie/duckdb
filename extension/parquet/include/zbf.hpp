#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>

#include "MurmurHash3.h"


inline std::uint64_t splitmix64(std::uint64_t &state)
{
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

template <uint32_t M>
void get_inverse_permutation(uint32_t seed, uint32_t *data)
{
    uint32_t p[M];
    for (uint32_t i = 0; i < M; ++i)
    {
        p[i] = i;
    }
    std::uint64_t state = seed; // cast to 64-bit
    for (uint32_t i = M - 1; i > 0; --i)
    {
        std::uint64_t r = splitmix64(state);
        uint32_t j = r % (i + 1);
        std::swap(p[i], p[j]);
    }
    for (uint32_t i = 0; i < M; i++)
    {
        data[p[i]] = i;
    }
}


class UntemplatedZBF {
protected:
    static constexpr unsigned MAX_KEYS = 32;
    static constexpr uint32_t seeds[MAX_KEYS] = {
        63313703,
        3724891943,
        1565336352,
        2493380941,
        2535289652,
        1671536233,
        2107311265,
        4242727406,
        203245942,
        1983277553,
        2823723655,
        919467398,
        2314609418,
        1514871762,
        3989243614,
        879636795,
        945145794,
        2620685030,
        4187170082,
        3450780267,
        3016303618,
        2510150471,
        4258339034,
        489631798,
        4010432533,
        3282322075,
        61988919,
        1987863476,
        1761944863,
        4179663006,
        3794687493,
        3088785056
    };
    static constexpr uint32_t h_perm_seed = 2445215149;
    static constexpr uint8_t offset_bit[8] = {
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    };

public:
    UntemplatedZBF(const uint8_t *mask, size_t size, uint32_t k): mask(mask), size(size), k(k){
        assert(1 <= k && k <= MAX_KEYS);
    };
    virtual void probe(
        const void * key,
        int key_len,
        bool* result
    ) = 0;

protected:
    const uint8_t * mask;
    size_t size;
    uint32_t k;

};

template <uint32_t M>
class ZBF: public UntemplatedZBF
{
    static constexpr uint32_t block_size = M/8; // bytes

private:
    uint32_t nblocks;

public:
    ZBF(const uint8_t *mask, size_t size, uint32_t k): UntemplatedZBF(mask, size, k){
        static_assert(M % 8 == 0);
        assert(size % block_size == 0);
        nblocks = size / block_size;
    }

    void probe(
        const void * key,
        int key_len,
        bool* result
    ){
        uint8_t bit_resutls[block_size];
        for (uint32_t i = 0; i < block_size; i++)
            bit_resutls[i] = 255;
        for(uint32_t seed_i = 0; seed_i < k; seed_i++){
            uint32_t block_idx;
            MurmurHash3_x86_32(key, key_len, seeds[seed_i], &block_idx);
            block_idx = block_idx % nblocks;
            const uint8_t * block = mask + (block_idx * block_size);
            for(uint32_t i = 0; i < block_size; i++){
                bit_resutls[i] &= block[i];
            }
        }
        // get inverse permutation
        uint32_t perm_seed;
        uint32_t perm_inv[M];
        MurmurHash3_x86_32(key, key_len, h_perm_seed, &perm_seed);
        get_inverse_permutation<M>(perm_seed, perm_inv);
        for(uint32_t i = 0; i < M; i++){
            if(bit_resutls[i/8] & offset_bit[i%8]){
                result[perm_inv[i]] = true;
            }
        }
    };
};
