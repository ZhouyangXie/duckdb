#include "zone_manager.hpp"

namespace duckdb {

vector<Zone> GetZonesFromZoneEnds(const idx_t * zone_ends_p, const SelectionVector & sel, idx_t size){
	vector<Zone> zones;
	if (size == 0){
		return zones;
	}
	// count output size before push back
	D_ASSERT(size <= sel.Capacity());
	auto last_j = sel.get_index(0);
	idx_t count = 1;
	for (idx_t i = 1; i < size; i++){
		auto j = sel.get_index(i);
		if (j == last_j + 1){
			last_j++;
			continue;
		}
		last_j = j;
		count += 1;
	}

	zones.reserve(NextPowerOfTwo(count));

	// insert
	last_j = sel.get_index(0);
	idx_t begin = last_j == 0? 0 : (zone_ends_p[last_j - 1] + 1);
	for (idx_t i = 1; i < size; i++){
		auto j = sel.get_index(i);
		if (j == last_j + 1){
			last_j++;
		} else {
			zones.push_back({begin, zone_ends_p[last_j]});
			last_j = j;
			begin = zone_ends_p[last_j - 1] + 1;
		}
		if (i == size - 1){
			zones.push_back({begin, zone_ends_p[last_j]});
		}
	}
	D_ASSERT(zones.size() == count);
	return zones;
}

vector<Zone> FindIntersectedZones(const vector<Zone> & zones_a, const vector<Zone> & zones_b){
	vector<Zone> intersects;
	intersects.reserve(MaxValue(zones_a.capacity(), zones_b.capacity()));
	idx_t i = 0, j = 0;
	while (i < zones_a.size() && j < zones_b.size())
	{
		auto intersect = zones_a[i].Intersect(zones_b[j]);
		if (!intersect.IsEmpty()){
			intersects.push_back(intersect);
		}
		if (zones_a[i].end > zones_b[j].end){
			j++;
		} else {
			i++;
		}
	}
	std::sort(intersects.begin(), intersects.end(), [](const Zone & a, const Zone & b){return a.begin < b.begin;});
	return intersects;
}

void ZoneIterator::ResetIteration() {
    cur_i = 0;
    if (zones.size() > 0) {
        cur = zones[0];
    }
    remaining_tuples = 0;
    for (auto & z: zones){
        remaining_tuples += z.Size();
    }
}

Zone ZoneIterator::Next(idx_t max_size) {
    Zone out = {0, 0};
    if (!HasNext()) {
        return out;
    }
    if (max_size == 0 || (cur.end <= (cur.begin + max_size))) {
        out = cur;
        cur_i++;
        if (HasNext()) {
            cur = zones[cur_i];
        }
    } else {
        out = {cur.begin, cur.begin + max_size};
        cur = {cur.begin + max_size, cur.end};
    }
    remaining_tuples -= out.Size();
    return out;
}

}