#include "zoned_selection_vector.hpp"


namespace duckdb {

void test_zoned_sel() {
    vector<Zone> zones = {{1, 512}, {1024, 2048}, {4096, 4096 + 1024}};
    ZonedSelectionVector s;

    s.AddZone(zones[0]);
    D_ASSERT(s.NumZones() == 1);
    D_ASSERT(s.GetSelSize(0) == (512 - 1));
    auto sel0 = s.GetSel(0);
    D_ASSERT(sel0->get_index(0) == 0);
    D_ASSERT(sel0->get_index(128) == 128);
    D_ASSERT(sel0->get_index(510) == 510);
    sel0->set_index(0, 13);
    sel0->set_index(1, 17);
    s.SetSelSize(0, 2);
    D_ASSERT(sel0->get_index(0) == 13);
    D_ASSERT(sel0->get_index(1) == 17);
    D_ASSERT(s.GetSelSize(0) == 2);

    s.AddZone(zones[1]);
    D_ASSERT(s.NumZones() == 2);
    D_ASSERT(s.GetSelSize(1) == (2048 - 1024));
    auto sel1 = s.GetSel(1);
    D_ASSERT(sel1->get_index(0) == 0);
    D_ASSERT(sel1->get_index(456) == 456);
    D_ASSERT(sel1->get_index(1023) == 1023);
    sel1->set_index(0, 110);
    sel1->set_index(1, 111);
    sel1->set_index(2, 134);
    s.SetSelSize(1, 3);
    D_ASSERT(sel1->get_index(0) == 110);
    D_ASSERT(sel1->get_index(1) == 111);
    D_ASSERT(sel1->get_index(2) == 134);
    D_ASSERT(s.GetSelSize(1) == 3);

    s.AddZone(zones[2]);
    D_ASSERT(s.NumZones() == 3);
    D_ASSERT(s.GetSelSize(0) == 2);
    D_ASSERT(s.GetSelSize(1) == 3);
    D_ASSERT(s.GetSelSize(2) == 1024);
    auto sel0_ = s.GetSel(0);
    auto sel1_ = s.GetSel(1);
    auto sel2_ = s.GetSel(2);
    D_ASSERT(sel0_->get_index(0) == 13);
    D_ASSERT(sel0_->get_index(1) == 17);
    D_ASSERT(sel1_->get_index(0) == 110);
    D_ASSERT(sel1_->get_index(1) == 111);
    D_ASSERT(sel1_->get_index(2) == 134);
    D_ASSERT(sel2_->get_index(0) == 0);
    D_ASSERT(sel2_->get_index(1) == 1);
    D_ASSERT(sel2_->get_index(1023) == 1023);
}

void test_update_zoned_sel() {
    ZonedSelectionVector z({{0, 5}, {20, 23}, {4096, 4096 + 2}});
    SelectionVector s(10);
    s.set_index(0, 5);
    s.set_index(1, 7);
    z.UpdateBy(s, 2);
    D_ASSERT(z.GetSelSize(0) == 0);
    D_ASSERT(z.GetSelSize(1) == 2);
    D_ASSERT(z.GetSelSize(2) == 0);
    auto sel1 = z.GetSel(1);
    D_ASSERT(sel1->get_index(0) == (5 - 5));
    D_ASSERT(sel1->get_index(1) == (7 - 5));
}

} // namespace duckdb

int main() {
	duckdb::test_zoned_sel();
    duckdb::test_update_zoned_sel();
	return 0;
}
