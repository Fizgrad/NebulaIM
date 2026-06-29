#include "common/db/MySqlTransaction.h"

#include "common/db/MySqlConnection.h"

namespace nebula {

MySqlTransaction::MySqlTransaction(MySqlConnection& conn)
    : conn_(conn),
      active_(conn_.beginTransaction()) {}

MySqlTransaction::~MySqlTransaction() {
    if (active_) {
        conn_.rollback();
    }
}

bool MySqlTransaction::commit() {
    if (!active_) {
        return false;
    }
    if (!conn_.commit()) {
        return false;
    }
    active_ = false;
    return true;
}

bool MySqlTransaction::rollback() {
    if (!active_) {
        return false;
    }
    if (!conn_.rollback()) {
        return false;
    }
    active_ = false;
    return true;
}

bool MySqlTransaction::active() const {
    return active_;
}

}  // namespace nebula
