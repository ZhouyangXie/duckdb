#include "zone_skipping.hpp"
#include "duckdb/common/helper.hpp"
#include "duckdb/common/types/timebase.hpp"
#include "duckdb/planner/filter/table_filter_functions.hpp"
#include "duckdb/planner/expression/bound_operator_expression.hpp"
#include "duckdb/common/types/vector.hpp"


namespace duckdb {

static LogicalTypeId GetMinMaxStatsInterpretionType(LogicalTypeId type_id){
    switch (type_id)
    {
    case LogicalTypeId::UTINYINT:
    case LogicalTypeId::TINYINT:
    case LogicalTypeId::USMALLINT:
    case LogicalTypeId::SMALLINT:
    case LogicalTypeId::UINTEGER:
    case LogicalTypeId::INTEGER:
    case LogicalTypeId::BIGINT:
        return LogicalTypeId::BIGINT;
    // Time/Date types are all physically int64
    case LogicalTypeId::DATE:
    case LogicalTypeId::TIME:
	case LogicalTypeId::TIMESTAMP_SEC:
	case LogicalTypeId::TIMESTAMP_MS:
	case LogicalTypeId::TIMESTAMP:
	case LogicalTypeId::TIMESTAMP_NS:
	case LogicalTypeId::TIMESTAMP_TZ:
	case LogicalTypeId::TIMESTAMP_TZ_NS:
	case LogicalTypeId::TIME_TZ:
	case LogicalTypeId::TIME_NS:
        return LogicalTypeId::BIGINT;
    // the first 8-byte of CHAR/VARCHAR are extracted to be a BIGINT
    // tail-filled with 0 if not long enough
    case LogicalTypeId::CHAR:
    case LogicalTypeId::VARCHAR:
        return LogicalTypeId::UBIGINT;
    case LogicalTypeId::DECIMAL:
    case LogicalTypeId::FLOAT:
    case LogicalTypeId::DOUBLE:
        return LogicalTypeId::DOUBLE;
    default:
        throw InternalException("Invalid data type to interpret ZoneStatistics.");
    }
}

static Value ConvertConstantForComparison(Value value){
    auto type_id = value.type().id();
    int64_t raw_value = 0;
    switch (type_id)
    {
    case LogicalTypeId::UTINYINT:
    case LogicalTypeId::TINYINT:
    case LogicalTypeId::USMALLINT:
    case LogicalTypeId::SMALLINT:
    case LogicalTypeId::UINTEGER:
    case LogicalTypeId::INTEGER:
    case LogicalTypeId::BIGINT:
        return value.DefaultCastAs(LogicalType::BIGINT);
    // Time/Date types are converted to int64 as diff from epoch
    case LogicalTypeId::DATE:
        raw_value = value.GetValue<date_t>().days;
        break;
    case LogicalTypeId::TIME:
        raw_value = value.GetValue<time_t>();
        break;
    case LogicalTypeId::TIMESTAMP_SEC:
        raw_value = value.GetValue<timestamp_sec_t>().value;
        break;
	case LogicalTypeId::TIMESTAMP_MS:
        raw_value = value.GetValue<timestamp_ms_t>().value;
        break;
	case LogicalTypeId::TIMESTAMP:
        raw_value = value.GetValue<timestamp_t>().value;
        break;
	case LogicalTypeId::TIMESTAMP_NS:
        raw_value = value.GetValue<timestamp_ns_t>().value;
        break;
	case LogicalTypeId::TIMESTAMP_TZ:
        raw_value = value.GetValue<timestamp_tz_t>().value;
        break;
	case LogicalTypeId::TIMESTAMP_TZ_NS:
        raw_value = value.GetValue<timestamp_tz_ns_t>().value;
        break;
	case LogicalTypeId::TIME_TZ:
        raw_value = value.GetValue<timestamp_tz_t>().value;
        break;
	case LogicalTypeId::TIME_NS:
        raw_value = value.GetValue<timestamp_ns_t>().value;
        break;
    case LogicalTypeId::DECIMAL:
    case LogicalTypeId::FLOAT:
    case LogicalTypeId::DOUBLE:
        return value.DefaultCastAs(LogicalType::DOUBLE);
    case LogicalTypeId::CHAR:
    case LogicalTypeId::VARCHAR:
        return Value::UBIGINT(
            BSwapIfLE(Load<uint64_t>(const_data_ptr_cast(value.GetValueUnsafe<string_t>().GetPrefix())))
        );
    default:
        throw InternalException("Invalid data type to interpret ZoneStatistics.");
    }
    return Value::BIGINT(raw_value);
}


unique_ptr<ZoneSkipState> ZoneStatisticsToState(
    const duckdb_parquet::ZoningStatistics & zone_stats,
    const LogicalType type
){
    auto zone_map = make_uniq<ZoneSkipState>(zone_stats.zone_offset.size());

    if (zone_stats.__isset.min_values && zone_stats.__isset.max_values){
        auto interpret_type_id = GetMinMaxStatsInterpretionType(type.id());
        auto interpret_type = LogicalType(interpret_type_id);
        zone_map->min_values = make_uniq<Vector>(interpret_type, (data_ptr_t)zone_stats.min_values.data(), zone_map->num_zones);
        zone_map->max_values = make_uniq<Vector>(interpret_type, (data_ptr_t)zone_stats.max_values.data(), zone_map->num_zones);
    }
    if (zone_stats.__isset.zbf){
        zone_map->InitializeZBF(
            (const uint8_t *)zone_stats.zbf.mask.data(), zone_stats.zbf.mask.size(), zone_stats.zbf.k);
    }
    return zone_map;
}

static unique_ptr<Vector> EvaluateEqualsOnZoneMaps(const Value & constant, const ZoneSkipState & state){

    unique_ptr<Vector> result = nullptr;

    if(state.HasZBF()){
        result = make_uniq<Vector>(LogicalType::BOOLEAN, state.num_zones, VectorDataInitialization::ZERO_INITIALIZE);
        result->BufferMutable().SetVectorSize(state.num_zones);
        if(constant.type().id() == LogicalTypeId::CHAR || constant.type().id() == LogicalTypeId::VARCHAR){
            const auto & str = constant.GetValueUnsafe<string_t>();
            const uint8_t * probe_key = (const uint8_t *)str.GetData();
            unsigned probe_key_size = str.GetSize();
            auto result_data = FlatVector::GetDataMutable<bool>(*result);
            state.zbf->probe(probe_key, probe_key_size, result_data);
        }
        else {
            auto result_data = FlatVector::GetDataMutable<bool>(*result);
            const auto & constant_cast = ConvertConstantForComparison(constant);
            if(constant_cast.type().id() == LogicalTypeId::BIGINT){
                auto probe_key = constant_cast.GetValue<int64_t>();
                state.zbf->probe(&probe_key, sizeof(probe_key), result_data);
            } else {
                // D_ASSERT(constant_cast.type().id() == LogicalTypeId::DOUBLE);
                auto probe_key = constant_cast.GetValue<double_t>();
                state.zbf->probe(&probe_key, sizeof(probe_key), result_data);
            }
        }
    }
    else if (state.HasMinMax()){
        result = make_uniq<Vector>(LogicalType::BOOLEAN, state.num_zones, VectorDataInitialization::ZERO_INITIALIZE);
        result->BufferMutable().SetVectorSize(state.num_zones);
        Value constant_cast = ConvertConstantForComparison(constant);
        Vector constant_vector(constant_cast.type(), 0);
        ConstantVector::Reference(constant_vector, constant_cast, count_t(state.num_zones));
        VectorOperations::LessThanEquals(*(state.min_values), constant_vector, *result);
        auto result_right = make_uniq<Vector>(LogicalType::BOOLEAN, state.num_zones);
        result_right->BufferMutable().SetVectorSize(state.num_zones);
        VectorOperations::GreaterThanEquals(*(state.max_values), constant_vector, *result_right);
        VectorOperations::And(*result_right, *result, *result);
    }
    return result;
}

// We extend ExpressionFilter's evaluation on Statistics to ZoneMaps
static unique_ptr<Vector> EvaluateFunctionOnZoneMaps(
    const BoundFunctionExpression & func_expr,
    const ZoneSkipState & state
){
    if(!BoundComparisonExpression::IsComparison(func_expr.GetExpressionType())){
        if(!func_expr.bind_info || func_expr.function.GetName() != OptionalFilterScalarFun::NAME){
            return nullptr;
        }
        // optional filter
        const auto & func_data = func_expr.bind_info->Cast<OptionalFilterFunctionData>();
        if(!func_data.child_filter_expr){
            return nullptr;
        }
        return EvaluateExpressionOnZoneMaps(*func_data.child_filter_expr, state);
    }

    auto comparison_type = func_expr.GetExpressionType();
    if (!state.HasMinMax() && !(comparison_type == ExpressionType::COMPARE_EQUAL && state.HasZBF())){
        return nullptr;
    }

	optional_ptr<const BoundConstantExpression> constant_expr;
	auto &left = BoundComparisonExpression::Left(func_expr);
	auto &right = BoundComparisonExpression::Right(func_expr);
	if (right.GetExpressionType() == ExpressionType::VALUE_CONSTANT) {
		constant_expr = &right.Cast<BoundConstantExpression>();
	} else if (left.GetExpressionType() == ExpressionType::VALUE_CONSTANT) {
		constant_expr = &left.Cast<BoundConstantExpression>();
		comparison_type = FlipComparisonExpression(comparison_type);
	} else {
		return nullptr;
	}
    auto &constant = constant_expr->value;

    if (comparison_type == ExpressionType::COMPARE_EQUAL){
        return EvaluateEqualsOnZoneMaps(constant ,state);
    }

	if (constant.IsNull()) {
        return nullptr;
	}
    Value constant_cast = ConvertConstantForComparison(constant);
    Vector constant_vector(constant_cast.type(), 0);
    ConstantVector::Reference(constant_vector, constant_cast, count_t(state.num_zones));

    auto result = make_uniq<Vector>(LogicalType::BOOLEAN, state.num_zones);
    result->BufferMutable().SetVectorSize(state.num_zones);

    switch(comparison_type){
        case ExpressionType::COMPARE_GREATERTHAN:
            VectorOperations::GreaterThan(*(state.max_values), constant_vector, *result);
            return result;
        case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
            VectorOperations::GreaterThanEquals(*(state.max_values), constant_vector, *result);
            return result;
        case ExpressionType::COMPARE_LESSTHAN:
            VectorOperations::LessThan(*(state.min_values), constant_vector, *result);
            return result;
        case ExpressionType::COMPARE_LESSTHANOREQUALTO:
            VectorOperations::LessThanEquals(*(state.min_values), constant_vector, *result);
            return result;
        case ExpressionType::COMPARE_EQUAL:
            throw InternalException("Should have been handled elsewhere.");
        default:
            return nullptr;
    }
}

static unique_ptr<Vector> EvaluateConjunctionZoneMaps(const BoundConjunctionExpression &conj, const ZoneSkipState & state){
    unique_ptr<Vector> result = nullptr;

	switch (conj.GetExpressionType()) {
    case ExpressionType::CONJUNCTION_AND:
        for(size_t i = 0; i < conj.children.size(); i++){
            auto child_result = EvaluateExpressionOnZoneMaps(*(conj.children[i]), state);
            if(child_result){
                if(result){
                    VectorOperations::And(*child_result, *result, *result);
                } else {
                    result = std::move(child_result);
                }
            }
        }
        return result;
    case ExpressionType::CONJUNCTION_OR:
        for(size_t i = 0; i < conj.children.size(); i++){
            auto child_result = EvaluateExpressionOnZoneMaps(*(conj.children[i]), state);
            if(child_result){
                if (result){
                    VectorOperations::Or(*child_result, *result, *result);
                } else {
                    result = std::move(child_result);
                }
            } else {
                return nullptr;
            }
        }
        return result;
    default:
        return nullptr;
    }
}

static unique_ptr<Vector> EvaluateOperatorOnZoneMaps(const BoundOperatorExpression & op_expr, const ZoneSkipState & state){
    unique_ptr<Vector> result = nullptr;
    switch (op_expr.GetExpressionType()) {
        case ExpressionType::OPERATOR_IS_NULL: {
            if (!state.HasZBF()){
                return nullptr;
            }
            result = make_uniq<Vector>(LogicalType::BOOLEAN, state.num_zones, VectorDataInitialization::ZERO_INITIALIZE);
            result->BufferMutable().SetVectorSize(state.num_zones);
            auto result_data = FlatVector::GetDataMutable<bool>(*result);
            const char * probe_key = "NULLENCODING";
            state.zbf->probe(probe_key, 12, result_data);
            return result;
        }
        case ExpressionType::COMPARE_IN: {
            for(size_t i = 1; i < op_expr.children.size(); i++){
                if(op_expr.children[i]->GetExpressionType() != ExpressionType::VALUE_CONSTANT){
                    return nullptr;
                }
        		auto &value = op_expr.children[i]->Cast<BoundConstantExpression>().value;
                auto child_result = EvaluateEqualsOnZoneMaps(value, state);
                if(child_result){
                    if (result){
                        VectorOperations::Or(*child_result, *result, *result);
                    } else {
                        result = std::move(child_result);
                    }
                } else {
                    return nullptr;
                }
            }
            return result;
        }
            // throw InternalException("IN filter should have been converted to Or conjunction.");
        case ExpressionType::OPERATOR_IS_NOT_NULL:
        default:
            return nullptr;
	}
    return nullptr;
}


unique_ptr<Vector> EvaluateExpressionOnZoneMaps(const Expression & expr, const ZoneSkipState & state){
    switch(expr.GetExpressionClass()){
    case ExpressionClass::BOUND_FUNCTION:
        return EvaluateFunctionOnZoneMaps(expr.Cast<BoundFunctionExpression>(), state);
    case ExpressionClass::BOUND_CONJUNCTION:
        return EvaluateConjunctionZoneMaps(expr.Cast<BoundConjunctionExpression>(), state);
    case ExpressionClass::BOUND_OPERATOR:
        return EvaluateOperatorOnZoneMaps(expr.Cast<BoundOperatorExpression>(), state);
    default:
        return nullptr;
    }
}

void EvaluateExpressionFilterOnZoneMaps(
    const ExpressionFilter & expr_filter,
    ZoneSkipState & state
){
    auto result = EvaluateExpressionOnZoneMaps(*expr_filter.expr, state);
    if (result){
        D_ASSERT(result->GetType().id() == LogicalTypeId::BOOLEAN);
        D_ASSERT(result->size() == state.num_zones);
        if(!state.IsSelInitialized()){
            state.InitializeSel();
        }
        size_t count = 0;
        for(size_t i = 0; i < state.num_zones; i++){
            if(result->GetValue(i).GetValue<bool>()){
                state.zone_sel->set_index(count, i);
                count++;
            }
        }
        state.zone_sel_size = count;
    }
}
}
