#pragma once

#include "duckdb/common/types/selection_vector.hpp"
#include "zone_manager.hpp"

namespace duckdb
{


class ZonedSelectionVector {
public:
    ZonedSelectionVector(size_t capacity = STANDARD_VECTOR_SIZE);

    ZonedSelectionVector(vector<Zone> && zones_, size_t capacity = STANDARD_VECTOR_SIZE);

    ZonedSelectionVector(vector<Zone> && zones_, const SelectionVector & sel, size_t count,  size_t capacity = STANDARD_VECTOR_SIZE);

    size_t NumZones(){
        return zones.size();
    }

    unique_ptr<SelectionVector> GetSel(idx_t i){
        idx_t sel_start = i > 0? zone_sel_ends[i - 1] : 0;
        return make_uniq<SelectionVector>(get_buffer_start() + sel_start, zone_sel_ends[i] - sel_start);
    }

    void ToUnifiedSel(SelectionVector & sel);

    size_t GetSelSize(idx_t i){
        return zone_sel_sizes[i];
    }

    void SetSelSize(idx_t i, size_t size){
        D_ASSERT(size < zone_sel_sizes[i]);
        total_count -= (zone_sel_sizes[i] - size);
        zone_sel_sizes[i] = size;
    }

    void AddZone(Zone new_zone);

    Zone GetZone(idx_t i){
        return zones[i];
    }

    void UpdateBy(const SelectionVector & sel, size_t count);

protected:
    void Compact();
    sel_t * get_buffer_start(){
        return reinterpret_cast<sel_t *>(selection_data->owned_data.get());
    }

private:
    size_t capacity;
    size_t total_count = 0;
    buffer_ptr<SelectionData> selection_data;
    vector<Zone> zones;
    vector<size_t> zone_sel_ends;
    vector<size_t> zone_sel_sizes;
};

} // namespace duckdb
