#pragma once

#include "duckdb.hpp"
#include "duckdb/common/constants.hpp"

namespace duckdb {

struct Zone {
	idx_t begin;
	idx_t end;

	inline bool IsEmpty() const {
		return begin >= end;
	}
	inline size_t Size() const {
		return end - begin;
	}
	inline Zone Intersect(const Zone & other) const {
		return {MaxValue(begin, other.begin), MinValue(end, other.end)};
	}
};


vector<Zone> GetZonesFromZoneEnds(const idx_t * zone_ends_p, const SelectionVector & sel, idx_t size);


vector<Zone> FindIntersectedZones(const vector<Zone> & zones_a, const vector<Zone> & zones_b);


class ZoneIterator {
public:
	ZoneIterator(vector<Zone> zones) : zones(zones) {
		ResetIteration();
	}

	ZoneIterator(idx_t count) {
		D_ASSERT(count >= 0);
		zones.push_back({0, count});
		ResetIteration();
	}

	inline bool HasNext() const {
		return cur_i < zones.size();
	}

	void ResetIteration();

	Zone Next(idx_t max_size = 0);

	idx_t GetRemaining(){
		return remaining_tuples;
	}

public:
	vector<Zone> zones;

private:
	idx_t cur_i;
	Zone cur;
	idx_t remaining_tuples;
};

} // namespace duckdb