#pragma once
#include "string"

#include "duckdb.hpp"
#include "duckdb/common/enums/expression_type.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/vector.hpp"
#include "zbf.hpp"

#include "parquet_types.h"

namespace duckdb {

struct ZoneSkipState {
    size_t num_zones;
    unique_ptr<Vector> min_values = nullptr;
    unique_ptr<Vector> max_values = nullptr;
    unique_ptr<SelectionVector> zone_sel = nullptr;
    unique_ptr<UntemplatedZBF> zbf = nullptr;
    size_t zone_sel_size = 0U;


    ZoneSkipState(size_t num_zones): num_zones(num_zones) {}

    bool HasMinMax() const {
        return min_values != nullptr && max_values != nullptr;
    }

    bool HasZBF() const {
        return zbf != nullptr;
    }

    bool IsSelInitialized() const {
        return zone_sel != nullptr;
    }
    void InitializeSel(){
        zone_sel = make_uniq<SelectionVector>(SelectionVector::Incremental(num_zones));
        zone_sel_size = num_zones;
    }

    void InitializeZBF(const uint8_t *mask, size_t size, uint32_t k){
        switch (num_zones){
        case 8:
            zbf = make_uniq<ZBF<8>>(mask, size, k);
            break;
        case 16:
            zbf = make_uniq<ZBF<16>>(mask, size, k);
            break;
        case 32:
            zbf = make_uniq<ZBF<32>>(mask, size, k);
            break;
        case 64:
            zbf = make_uniq<ZBF<64>>(mask, size, k);
            break;
        case 128:
            zbf = make_uniq<ZBF<128>>(mask, size, k);
            break;
        case 256:
            zbf = make_uniq<ZBF<256>>(mask, size, k);
            break;
        case 512:
            zbf = make_uniq<ZBF<512>>(mask, size, k);
            break;
        case 1024:
            zbf = make_uniq<ZBF<1024>>(mask, size, k);
            break;
        case 2048:
            zbf = make_uniq<ZBF<2048>>(mask, size, k);
            break;
        case 4096:
            zbf = make_uniq<ZBF<4096>>(mask, size, k);
            break;
        case 8192:
            zbf = make_uniq<ZBF<8192>>(mask, size, k);
            break;
        case 16384:
            zbf = make_uniq<ZBF<16384>>(mask, size, k);
            break;
        default:
            throw InternalException("Having invalid number of zones to initialize ZBF.");
            break;
        }
    }
};


unique_ptr<ZoneSkipState> ZoneStatisticsToState(const duckdb_parquet::ZoningStatistics & zone_stats, const LogicalType type);

void EvaluateExpressionFilterOnZoneMaps(const ExpressionFilter & expr_filter, ZoneSkipState & state);

unique_ptr<Vector> EvaluateExpressionOnZoneMaps(const Expression & expr, const ZoneSkipState & state);

}
