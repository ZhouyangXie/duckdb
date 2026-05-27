import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq


ROW_GROUP_SIZE = 2048 * 3

data = {
    'x': np.arange(ROW_GROUP_SIZE).astype(np.uint32),
    'y': np.random.rand(ROW_GROUP_SIZE).astype(np.float64)
}

table = pa.table(data)
pq.write_table(
    table,
    "sample.parquet",
    row_group_size=ROW_GROUP_SIZE,
    data_page_size=None,
    write_statistics=True,
    use_dictionary=False,
)
