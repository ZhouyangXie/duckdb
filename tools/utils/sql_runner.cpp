#include <fstream>
#include <iostream>
#include <iterator>

#include "duckdb.hpp"


int main(int argc, char **argv) {
	D_ASSERT(argc == 2);
	std::string sql_file_path = argv[1];
	std::ifstream sql_file(sql_file_path);
	if (!sql_file) {
		std::cerr << "Failed to open SQL file: " << sql_file_path << '\n';
		return 1;
	}

	std::string sql((std::istreambuf_iterator<char>(sql_file)), std::istreambuf_iterator<char>());
	duckdb::DuckDB db;
	duckdb::Connection con(db);
	auto result = con.Query(sql);

	if (result->HasError()) {
		std::cerr << result->GetError() << '\n';
		return 1;
	}
	return 0;
}
