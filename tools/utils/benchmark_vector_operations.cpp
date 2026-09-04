#include <algorithm>
#include <random>
#include <string>
#include <chrono>
#include <iostream>

#include "duckdb.hpp"
#include "duckdb/common/vector/constant_vector.hpp"
#include "duckdb/common/vector/flat_vector.hpp"
#include "duckdb/common/vector/string_vector.hpp"
#include "duckdb/common/types/selection_vector.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/planner/filter/table_filter_functions.hpp"


namespace duckdb {

std::pair<Vector, SelectionVector> CreateRandomInt32Vector(int cardinality, bool gathered){
    D_ASSERT(0 < cardinality && cardinality <= STANDARD_VECTOR_SIZE);
	Vector values(LogicalType::INTEGER, STANDARD_VECTOR_SIZE);
	auto data = FlatVector::GetDataMutable<int32_t>(values);

	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int32_t> distribution(0, 2048);
	for (idx_t row = 0; row < STANDARD_VECTOR_SIZE; row++) {
		data[row] = distribution(rng);
	}

	SelectionVector selection(STANDARD_VECTOR_SIZE);
    for (idx_t row = 0; row < STANDARD_VECTOR_SIZE; row++) {
        selection.set_index(row, row);
    }
    if (!gathered) {
        std::shuffle(selection.data(), selection.data() + STANDARD_VECTOR_SIZE, rng);
        std::sort(selection.data(), selection.data() + cardinality);
    }
	return {std::move(values), std::move(selection)};
}

std::pair<Vector, SelectionVector> CreateRandomStringVector(int cardinality, int str_max_length, bool gathered){
    D_ASSERT(0 <= cardinality && cardinality <= STANDARD_VECTOR_SIZE);
    D_ASSERT(str_max_length > string_t::INLINE_LENGTH);
	Vector values(LogicalType::VARCHAR, STANDARD_VECTOR_SIZE);
	std::mt19937 rng(std::random_device{}());
	std::uniform_int_distribution<int32_t> char_distribution('a', 'z');
	std::uniform_int_distribution<int32_t> sel_distribution(0, 2048);
	std::uniform_int_distribution<int32_t> length_distribution(string_t::INLINE_LENGTH + 1, str_max_length);

    idx_t generated_count = gathered? cardinality:STANDARD_VECTOR_SIZE;
	for (idx_t row = 0; row < generated_count; row++) {
		std::string random_string = "SOMERANDOMFIXEDPRIFIX";
		auto length = length_distribution(rng);
		random_string.reserve(random_string.size()+length);
		for (int32_t idx = 0; idx < length; idx++) {
	        char c = static_cast<char>(char_distribution(rng));
			random_string.push_back(c);
		}
        values.SetValue(row, string_t(random_string.c_str(), UnsafeNumericCast<uint32_t>(random_string.size())));
	}

	SelectionVector selection(STANDARD_VECTOR_SIZE);
    for (idx_t row = 0; row < STANDARD_VECTOR_SIZE; row++) {
        selection.set_index(row, row);
    }
    if (!gathered) {
        std::shuffle(selection.data(), selection.data() + STANDARD_VECTOR_SIZE, rng);
        std::sort(selection.data(), selection.data() + cardinality);
    }
    return {std::move(values), std::move(selection)};
}

idx_t LookupHashesScattered(const BloomFilter & bf, const Vector &hashes_v, const SelectionVector & sel, const idx_t count, SelectionVector & result_sel) {
	D_ASSERT(hashes_v.GetVectorType() == VectorType::FLAT_VECTOR);
	D_ASSERT(hashes_v.GetType() == LogicalType::HASH);

	const auto hashes = FlatVector::GetData<uint64_t>(hashes_v);
	idx_t found_count = 0;
	for (idx_t i = 0; i < count; i++) {
        if(bf.LookupOne(hashes[sel[i]])){
            result_sel.set_index(found_count++, i);
        }
	}
	return found_count;
}

} // namespace duckdb

using namespace duckdb;
int main(int argc, char const *argv[])
{
#ifdef DEBUG
    constexpr int RUN_N_TIMES = 1;
    constexpr int WARM_UP_RUNS = 0;
    int cardinalities[] = {32, 2047};
#else
    constexpr int RUN_N_TIMES = 256;
    constexpr int WARM_UP_RUNS = 32;
    // int cardinalities[] = {32, 64, 128, 256, 128*3, 512, 3*256, 1024, 1024 + 256, 512 * 3, 512*3 + 256, 2048 - 1};
    int cardinalities[] = {16, 32, 64, 96, 128, 128 + 64, 256, 128*3, 512};
#endif

    constexpr int COMPARE_INT = 512;
    constexpr int VARCHAR_LEN = 64;
    const std::string compared_string = "SOMERANDOMFIXEDPRIFIXaaa";
    constexpr idx_t BF_CARD = 2048 * 128;

    // std::cout<<"Benchmarking Int Compare (Scattered/Unsliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomInt32Vector(card, false);
			Vector constant(LogicalType::INTEGER, STANDARD_VECTOR_SIZE);
			ConstantVector::Reference(constant, Value::INTEGER(COMPARE_INT), count_t(STANDARD_VECTOR_SIZE));
            SelectionVector true_sel(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

			idx_t true_count = VectorOperations::GreaterThan(vec, constant, &sel, card, &true_sel, nullptr, nullptr);
            sel.SliceInPlace(true_sel, true_count);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking Int Compare (Contiguous/Sliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, _] = CreateRandomInt32Vector(card, true);
			Vector constant(LogicalType::INTEGER, STANDARD_VECTOR_SIZE);
			ConstantVector::Reference(constant, Value::INTEGER(COMPARE_INT), count_t(STANDARD_VECTOR_SIZE));
            SelectionVector true_sel(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

			auto _count = VectorOperations::GreaterThan(vec, constant, nullptr, card, &true_sel, nullptr, nullptr);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    DBConfig config(false);
    auto db = DuckDB(":memory:", &config);
    Connection con(db);
    BloomFilter bf;
    bf.Initialize(*con.context, BF_CARD);
    Vector hashes(LogicalType::HASH, STANDARD_VECTOR_SIZE);
    for (unsigned int i = 0; i < BF_CARD; i+=STANDARD_VECTOR_SIZE){
        for (unsigned int j = 0; j < STANDARD_VECTOR_SIZE; j++){
            hashes.SetValue(j, Value::HASH(Hash(i + j + 1024)));
        }
        bf.InsertHashes(hashes);
    }

    // std::cout<<"Benchmarking Int BFProbe (Scattered/Unsliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomInt32Vector(card, false);
            Vector hashes(LogicalType::HASH, STANDARD_VECTOR_SIZE);
            SelectionVector bf_probe_result(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

            VectorOperations::Hash(vec, hashes, sel, card);
            auto probe_count = LookupHashesScattered(bf, hashes, sel, card, bf_probe_result);
            sel.Slice(bf_probe_result, probe_count);

            auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking Int BFProbe (Contiguous/Sliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomInt32Vector(card, true);
            Vector hashes(LogicalType::HASH, STANDARD_VECTOR_SIZE);
            SelectionVector bf_probe_result(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

            VectorOperations::Hash(vec, hashes, card);
            auto probe_count = bf.LookupHashes(hashes, sel, card);

            auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // // std::cout<<"Benchmarking Int Slicing "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomInt32Vector(card, false);
            auto [vec_buffered, _] = CreateRandomInt32Vector(1, true);

            auto start = std::chrono::steady_clock::now();
            vec_buffered.Append(vec, sel, card);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking String Compare (Scattered/Unsliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomStringVector(card, VARCHAR_LEN, false);
			Vector constant(LogicalType::VARCHAR, STANDARD_VECTOR_SIZE);
			ConstantVector::Reference(constant, Value(compared_string), count_t(STANDARD_VECTOR_SIZE));
            SelectionVector true_sel(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

			idx_t true_count = VectorOperations::GreaterThan(vec, constant, &sel, card, &true_sel, nullptr, nullptr);
            sel.SliceInPlace(true_sel, true_count);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking String Compare (Contiguous/Sliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, _] = CreateRandomStringVector(card, VARCHAR_LEN, true);
			Vector constant(LogicalType::VARCHAR, STANDARD_VECTOR_SIZE);
			ConstantVector::Reference(constant, Value(compared_string), count_t(STANDARD_VECTOR_SIZE));
            SelectionVector true_sel(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

			idx_t true_count = VectorOperations::GreaterThan(vec, constant, nullptr, card, &true_sel, nullptr, nullptr);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }


    // std::cout<<"Benchmarking String BF Probe (Scattered/Unsliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomStringVector(card, VARCHAR_LEN, false);
            Vector hashes(LogicalType::HASH, STANDARD_VECTOR_SIZE);
            SelectionVector bf_probe_result(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

            VectorOperations::Hash(vec, hashes, sel, card);
            auto probe_count = LookupHashesScattered(bf, hashes, sel, card, bf_probe_result);
            sel.Slice(bf_probe_result, probe_count);

            auto end = std::chrono::steady_clock::now();

            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking String BF Probe (Contiguous/Sliced) "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomStringVector(card, VARCHAR_LEN, true);
            Vector hashes(LogicalType::HASH, STANDARD_VECTOR_SIZE);
            SelectionVector bf_probe_result(STANDARD_VECTOR_SIZE);

            auto start = std::chrono::steady_clock::now();

            VectorOperations::Hash(vec, hashes, card);
            auto probe_count = bf.LookupHashes(hashes, sel, card);

            auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    // std::cout<<"Benchmarking String Slicing "<< RUN_N_TIMES <<" Rounds\n";
    for (auto card: cardinalities){
        long elapse_total = 0;
        for (int i = 0; i < RUN_N_TIMES + WARM_UP_RUNS; i++)
        {
            auto [vec, sel] = CreateRandomStringVector(card, VARCHAR_LEN, false);
            auto [vec_buffered, _] = CreateRandomStringVector(0, VARCHAR_LEN, true);
            vec_buffered.GetBufferRef()->Reserve(VARCHAR_LEN * card, VectorAppendMode::ALLOW_RESIZE);

            auto start = std::chrono::steady_clock::now();

            vec_buffered.Append(vec, sel, card);

			auto end = std::chrono::steady_clock::now();
            auto elapse = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (i >= WARM_UP_RUNS) elapse_total += elapse;
        }
        std::cout<<elapse_total<<std::endl;
    }

    return 0;
}
