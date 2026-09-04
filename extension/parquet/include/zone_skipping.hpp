#pragma once

#include "duckdb.hpp"
#include "duckdb/common/enums/expression_type.hpp"
#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types.hpp"
#include "duckdb/common/types/vector.hpp"

#include "parquet_types.h"

namespace duckdb {

struct ZoneSkipState {
    size_t num_zones;
    unique_ptr<Vector> min_values = nullptr;
    unique_ptr<Vector> max_values = nullptr;
    unique_ptr<SelectionVector> zone_sel = nullptr;
    size_t zone_sel_size = 0U;

    ZoneSkipState(size_t num_zones): num_zones(num_zones) {}

    bool HasMinMax() const {
        return min_values != nullptr && max_values != nullptr;
    }
    bool IsSelInitialized() const {
        return zone_sel != nullptr;
    }
    void InitializeSel(){
        zone_sel = make_uniq<SelectionVector>(SelectionVector::Incremental(num_zones));
        zone_sel_size = num_zones;
    }
};


unique_ptr<ZoneSkipState> ZoneStatisticsToState(const duckdb_parquet::ZoningStatistics & zone_stats, const LogicalType type);

void EvaluateExpressionFilterOnZoneMaps(const ExpressionFilter & expr_filter, ZoneSkipState & state);

unique_ptr<Vector> EvaluateExpressionOnZoneMaps(const Expression & expr, const ZoneSkipState & state);

}
