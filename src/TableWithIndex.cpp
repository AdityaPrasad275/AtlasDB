#include "TableWithIndex.h"

#include <algorithm>
#include <cstring>

TableWithIndex::TableWithIndex(
    BufferPoolManager* bpm,
    page_id_t table_first_page_id,
    page_id_t index_root_page_id
) : _table(bpm, table_first_page_id), _index(bpm, index_root_page_id) {}

std::vector<char> TableWithIndex::_serializeRow(int key, const char* payload, int payload_size) {
    std::vector<char> row(sizeof(int) + payload_size);
    std::memcpy(row.data(), &key, sizeof(int));
    if (payload_size > 0) {
        std::memcpy(row.data() + sizeof(int), payload, payload_size);
    }
    return row;
}

bool TableWithIndex::_deserializeRow(const std::vector<char>& stored_row, IndexedRow& row, const RID& rid) {
    if (stored_row.size() < sizeof(int)) {
        return false;
    }

    std::memcpy(&row.key, stored_row.data(), sizeof(int));
    row.payload.assign(stored_row.begin() + sizeof(int), stored_row.end());
    row.rid = rid;
    return true;
}

bool TableWithIndex::_fetchRowByRid(const RID& rid, IndexedRow& row) {
    std::vector<char> stored_row;
    if (!_table.getRecord(rid, stored_row)) {
        return false;
    }
    return _deserializeRow(stored_row, row, rid);
}

bool TableWithIndex::insert(int key, const char* payload, int payload_size, RID* inserted_rid) {
    std::vector<RID> existing;
    if (_index.getValue(key, existing)) {
        return false;
    }

    std::vector<char> row = _serializeRow(key, payload, payload_size);
    RID rid;
    if (!_table.insertRecord(row.data(), static_cast<int>(row.size()), rid)) {
        return false;
    }

    if (!_index.insert(key, rid)) {
        return false;
    }

    if (inserted_rid != nullptr) {
        *inserted_rid = rid;
    }
    return true;
}

bool TableWithIndex::getByKeyScan(int key, IndexedRow& row) {
    RID rid;
    std::vector<char> stored_row;
    if (!_table.getFirstRecord(rid, stored_row)) {
        return false;
    }

    do {
        IndexedRow candidate;
        if (_deserializeRow(stored_row, candidate, rid) && candidate.key == key) {
            row = std::move(candidate);
            return true;
        }

        RID next_rid;
        std::vector<char> next_row;
        if (!_table.getNextRecord(rid, next_rid, next_row)) {
            break;
        }
        rid = next_rid;
        stored_row = std::move(next_row);
    } while (true);

    return false;
}

bool TableWithIndex::getByKeyIndex(int key, IndexedRow& row) {
    std::vector<RID> result;
    if (!_index.getValue(key, result) || result.empty()) {
        return false;
    }

    if (!_fetchRowByRid(result[0], row)) {
        return false;
    }

    return row.key == key;
}

bool TableWithIndex::getByKey(int key, IndexedRow& row) {
    return getByKeyIndex(key, row);
}

bool TableWithIndex::deleteByKey(int key) {
    std::vector<RID> result;
    if (!_index.getValue(key, result) || result.empty()) {
        return false;
    }

    if (!_table.deleteRecord(result[0])) {
        return false;
    }

    return _index.remove(key);
}

bool TableWithIndex::updateByKey(int key, const char* payload, int payload_size, RID* updated_rid) {
    return updateByKey(key, key, payload, payload_size, updated_rid);
}

bool TableWithIndex::updateByKey(int current_key, int new_key, const char* payload, int payload_size, RID* updated_rid) {
    std::vector<RID> current_result;
    if (!_index.getValue(current_key, current_result) || current_result.empty()) {
        return false;
    }

    if (current_key != new_key) {
        std::vector<RID> target_result;
        if (_index.getValue(new_key, target_result) && !target_result.empty()) {
            return false;
        }
    }

    RID rid = current_result[0];
    const RID old_rid = rid;
    std::vector<char> row = _serializeRow(new_key, payload, payload_size);
    if (!_table.updateRecord(row.data(), static_cast<int>(row.size()), rid)) {
        return false;
    }

    const bool rid_changed = !(rid == old_rid);
    const bool key_changed = current_key != new_key;
    if (rid_changed || key_changed) {
        if (!_index.remove(current_key)) {
            return false;
        }
        if (!_index.insert(new_key, rid)) {
            return false;
        }
    }

    if (updated_rid != nullptr) {
        *updated_rid = rid;
    }
    return true;
}

bool TableWithIndex::rangeScanScan(int low_key, int high_key, std::vector<IndexedRow>& rows) {
    rows.clear();

    RID rid;
    std::vector<char> stored_row;
    if (!_table.getFirstRecord(rid, stored_row)) {
        return false;
    }

    do {
        IndexedRow candidate;
        if (_deserializeRow(stored_row, candidate, rid) &&
            candidate.key >= low_key &&
            candidate.key <= high_key) {
            rows.push_back(std::move(candidate));
        }

        RID next_rid;
        std::vector<char> next_row;
        if (!_table.getNextRecord(rid, next_rid, next_row)) {
            break;
        }
        rid = next_rid;
        stored_row = std::move(next_row);
    } while (true);

    std::sort(rows.begin(), rows.end(), [](const IndexedRow& lhs, const IndexedRow& rhs) {
        return lhs.key < rhs.key;
    });

    return !rows.empty();
}

bool TableWithIndex::rangeScanIndex(int low_key, int high_key, std::vector<IndexedRow>& rows) {
    rows.clear();

    BPlusTreeCursor cursor;
    if (!_index.lowerBound(low_key, cursor)) {
        return false;
    }

    int key;
    RID rid;
    do {
        if (!_index.getCursorValue(cursor, key, rid)) {
            break;
        }
        if (key > high_key) {
            break;
        }

        IndexedRow row;
        if (!_fetchRowByRid(rid, row)) {
            return false;
        }
        rows.push_back(std::move(row));
    } while (_index.next(cursor));

    return !rows.empty();
}
