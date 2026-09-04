#include "zoned_selection_vector.hpp"

namespace duckdb {

ZonedSelectionVector::ZonedSelectionVector(size_t capacity): capacity(capacity) {
    selection_data = make_shared_ptr<SelectionData>(capacity);
}

ZonedSelectionVector::ZonedSelectionVector(vector<Zone> && zones_, size_t capacity): capacity(capacity) {
    zones = std::move(zones_);
    zone_sel_ends.reserve(zones.capacity());
    zone_sel_sizes.reserve(zones.capacity());
    selection_data = make_shared_ptr<SelectionData>(capacity);
    for(auto & zone: zones){
        for(size_t j = 0; j < zone.Size(); j++){
            sel_t * p = get_buffer_start() + total_count;
            p[j] = j;
        }
        total_count += zone.Size();
        zone_sel_sizes.push_back(zone.Size());
        zone_sel_ends.push_back(total_count);
    }
    D_ASSERT(total_count <= capacity);
}

// ZonedSelectionVector::ZonedSelectionVector(
//     vector<Zone> && zones_,
//     const SelectionVector & sel,
//     size_t count,
//     size_t capacity): capacity(capacity)
// {
//     zones = std::move(zones_);
//     zone_sel_ends.reserve(zones.capacity());
//     zone_sel_sizes.reserve(zones.capacity());
//     selection_data = make_shared_ptr<SelectionData>(capacity);

//     size_t cur_zone_id = 0;
//     zone_sel_ends.push_back(0);
//     zone_sel_sizes.push_back(0);
//     size_t acc_zone_size = 0;
//     for(size_t i = 0; i < count; i++){
//         auto unified_offset = sel.get_index(i);
//         while(acc_zone_size + zones[cur_zone_id].Size() <= unified_offset){
//             acc_zone_size += zones[cur_zone_id].Size();
//             cur_zone_id += 1;
//             // D_ASSERT(cur_zone_id < NumZones());
//             zone_sel_ends.push_back(i);
//             zone_sel_sizes.push_back(0);
//         }
//         get_buffer_start()[i] = (unified_offset - acc_zone_size);
//         zone_sel_sizes.back() = (i + 1);
//         zone_sel_ends.back() += 1;
//     }

//     total_count = count;
//     D_ASSERT(zone_sel_ends.size() == NumZones());
//     D_ASSERT(zone_sel_sizes.size() == NumZones());
// }

void ZonedSelectionVector::Compact(){
    if (NumZones() == 0){
        return;
    }
    sel_t * dst = get_buffer_start() + zone_sel_sizes[0];
    for (idx_t i = 1; i < NumZones(); i++){
        std::memmove(
            dst,
            get_buffer_start() + zone_sel_ends[i - 1],
            zone_sel_sizes[i] * sizeof(sel_t)
        );
        dst += zone_sel_sizes[i];
    }
    zone_sel_ends[0] = zone_sel_sizes[0];
    for (idx_t i = 1; i < NumZones(); i++){
        zone_sel_ends[i] = zone_sel_ends[i - 1] + zone_sel_sizes[i];
    }
}

void ZonedSelectionVector::AddZone(Zone new_zone){
    D_ASSERT((total_count + new_zone.Size()) <= capacity);

    if (new_zone.Size() > (capacity - (NumZones() > 0? zone_sel_ends.back() : 0))){
        Compact();
    }
    zones.push_back(new_zone);
    zone_sel_sizes.push_back(new_zone.Size());
    size_t new_zone_start = zone_sel_ends.size() > 0? zone_sel_ends.back() : 0;
    zone_sel_ends.push_back(new_zone_start + new_zone.Size());
    sel_t * p = get_buffer_start() + new_zone_start;
    for (idx_t i = 0; i < new_zone.Size(); i++) {
        p[i] = sel_t(i);
    }
    total_count += new_zone.Size();
}

void ZonedSelectionVector::ToUnifiedSel(SelectionVector & sel){
    D_ASSERT(sel.Capacity() >= total_count);
    idx_t write_offset = 0;
    for(idx_t i = 0; i < NumZones(); i++){
        idx_t sel_start = i > 0? zone_sel_ends[i - 1] : 0;
        for(idx_t j = 0; j < zone_sel_sizes[i]; j++){
            sel.set_index(write_offset, get_buffer_start()[sel_start + j]);
            write_offset++;
        }
    }
}

void ZonedSelectionVector::UpdateBy(const SelectionVector & sel, size_t count){
    if (total_count == count){
        return;
    }
    size_t cur_zone_id = 0;
    size_t acc_zone_size = 0;
    for(size_t i = 0; i < count; i++){
        auto unified_offset = sel.get_index(i);
        while(acc_zone_size + zones[cur_zone_id].Size() <= unified_offset){
            acc_zone_size += zones[cur_zone_id].Size();
            zone_sel_ends[cur_zone_id] = i;
            zone_sel_sizes[cur_zone_id] = i - (cur_zone_id == 0 ? 0 : zone_sel_ends[cur_zone_id - 1]);
            cur_zone_id += 1;
        }
        get_buffer_start()[i] = (unified_offset - acc_zone_size);
    }
    while (cur_zone_id < NumZones()){
        zone_sel_ends[cur_zone_id] = count;
        zone_sel_sizes[cur_zone_id] = count - (cur_zone_id == 0 ? 0 : zone_sel_ends[cur_zone_id - 1]);
        cur_zone_id++;
    }
    total_count = count;
};

}