#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>

struct Row { std::vector<std::string> values; std::string operator[](size_t i) const {return i<values.size()?values[i]:"";} };
class Database {
    sqlite3* db=nullptr;
public:
    explicit Database(const std::string& path);
    ~Database();
    Database(const Database&)=delete;
    Database& operator=(const Database&)=delete;
    void exec(const std::string& sql);
    std::vector<Row> query(const std::string& sql, const std::vector<std::string>& args={});
    int change(const std::string& sql, const std::vector<std::string>& args={});
    int id() const;
    void begin(); void commit(); void rollback();
    void schema();
};
std::string passwordHash(const std::string& password,const std::string& saltHex);
std::string newSalt();
bool passwordMatches(const std::string& password,const std::string& salt,const std::string& hash);
