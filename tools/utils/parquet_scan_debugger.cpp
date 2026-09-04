#include <iostream>
#include "duckdb.hpp"

namespace duckdb {

void scan_by_query(const char *file_path, const char * sql) {
	DBConfig config({{"threads", 1}}, false);
    auto db = DuckDB(file_path, &config);
    auto conn = Connection(db);
	auto result = conn.Query(sql);
	std::cout << result->ToString() << std::endl;
}

} // namespace duckdb

int main(int argc, char **argv) {
	D_ASSERT(argc == 3);
	duckdb::scan_by_query(argv[1], argv[2]);
	return 0;
}
