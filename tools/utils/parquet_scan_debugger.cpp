#include "duckdb.hpp"

namespace duckdb {

void scan_by_query(const char *file_path) {
	DBConfig config({{"threads", 1}}, false);
    auto db = DuckDB(file_path, &config);
    auto conn = Connection(db);
	conn.RelationFromQuery("SELECT * FROM sample")
        ->Filter("1000 < x AND x < 2048")
	    ->Project(vector<std::string>({"y"}))
	    ->WriteCSV("/dev/null");
}

} // namespace duckdb

int main(int argc, char **argv) {
	duckdb::scan_by_query(argv[1]);
	return 0;
}
