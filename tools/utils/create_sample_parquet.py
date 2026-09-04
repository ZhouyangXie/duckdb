import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq


ROW_GROUP_SIZE = 1024

data = {
    # 'x': np.arange(ROW_GROUP_SIZE).astype(np.uint32),
    # 'x': np.random.rand(ROW_GROUP_SIZE).astype(np.float64),
    # 'y': np.random.rand(ROW_GROUP_SIZE).astype(np.float64),
    "x": np.random.randint(low=0, high=1024, size=ROW_GROUP_SIZE).astype(np.int64),
    "y": np.random.randint(low=0, high=1024, size=ROW_GROUP_SIZE).astype(np.int64),
    "z": np.random.randint(low=0, high=1024, size=ROW_GROUP_SIZE).astype(np.int64),
}

mask = np.full(ROW_GROUP_SIZE, fill_value=False, dtype=bool)
mask[16:24] = True
data["x"] = pa.array(data["x"], mask=mask)

table = pa.table(data)

pq.write_table(
    table,
    "sample.parquet",
    row_group_size=ROW_GROUP_SIZE,
    data_page_size=None,
    write_statistics=True,
    use_dictionary=False,
)

ROW_GROUP_SIZE = 1024


# data = {
#     # 'x': np.arange(ROW_GROUP_SIZE).astype(np.uint32),
#     # 'x': np.random.rand(ROW_GROUP_SIZE).astype(np.float64),
#     # 'y': np.random.rand(ROW_GROUP_SIZE).astype(np.float64),
#     "x": np.random.randint(low=0, high=1024, size=ROW_GROUP_SIZE).astype(np.int16),
#     "y": np.random.randint(low=0, high=1024, size=ROW_GROUP_SIZE).astype(np.int16),
# }

# table = pa.table(data)
# pq.write_table(
#     table,
#     "build.parquet",
#     row_group_size=ROW_GROUP_SIZE,
#     data_page_size=None,
#     write_statistics=True,
#     use_dictionary=False,
# )
