#pragma once

namespace nebula {

class MySqlConnection;

class MySqlTransaction {
public:
    explicit MySqlTransaction(MySqlConnection& conn);
    ~MySqlTransaction();

    MySqlTransaction(const MySqlTransaction&) = delete;
    MySqlTransaction& operator=(const MySqlTransaction&) = delete;

    bool commit();
    bool rollback();
    bool active() const;

private:
    MySqlConnection& conn_;
    bool active_;
};

}  // namespace nebula
