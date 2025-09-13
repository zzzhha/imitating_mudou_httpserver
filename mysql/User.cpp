// user_dao.cpp
#include "User.h"
#include <mysql/mysql.h>
#include <cstring>

// 注册逻辑：检查用户名是否存在，不存在则插入
bool UserDao::Register(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        LOGWARNING("Register failed: username or password is empty");
        return false;
    }

    MYSQL* sql = nullptr;
    // 利用RAII自动获取和释放连接
    SqlConnRAII raii(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOGERROR("Register failed: get connection from pool failed");
        return false;
    }

    // 转义特殊字符，防止SQL注入
    char esc_username[256] = {0};
    mysql_real_escape_string(sql, esc_username, username.c_str(), username.size());
    char esc_password[256] = {0};
    mysql_real_escape_string(sql, esc_password, password.c_str(), password.size());

    // 1. 检查用户名是否已存在
    std::string check_sql = "SELECT username FROM user WHERE username = '" 
        + std::string(esc_username) + "';";
    if (mysql_query(sql, check_sql.c_str()) != 0) {
      char buf[256];
      sprintf(buf,"Register check failed: %s",mysql_error(sql));
      LOGERROR(buf);
      return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (res && mysql_num_rows(res) > 0) {
      // 用户名已存在
      mysql_free_result(res);
      char buf[256];
      sprintf(buf,"Register failed: username %s already exists", username.c_str());
      LOGWARNING(buf);
      return false;
    }
    if (res) mysql_free_result(res); // 释放结果集

    // 2. 插入新用户（实际项目中应加密密码，如用SHA256）
    std::string insert_sql = "INSERT INTO user(username, password) VALUES('"
        + std::string(esc_username) + "', '" + std::string(esc_password) + "');";
    if (mysql_query(sql, insert_sql.c_str()) != 0) {
      char buf[256];
      sprintf(buf,"Register insert failed: %s", mysql_error(sql));
      LOGERROR(buf);
        return false;
    }
    char buf[256];
    sprintf(buf,"Register success: username = %s", username.c_str());
    LOGINFO(buf);
    return true;
}

// 登录逻辑：查询用户名并验证密码
bool UserDao::Login(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        LOGWARNING("Login failed: username or password is empty");
        return false;
    }

    MYSQL* sql = nullptr;
    SqlConnRAII raii(&sql, SqlConnPool::Instance());
    if (!sql) {
        LOGERROR("Login failed: get connection from pool failed");
        return false;
    }

    // 转义特殊字符
    char esc_username[256] = {0};
    mysql_real_escape_string(sql, esc_username, username.c_str(), username.size());

    // 查询用户密码
    std::string query_sql = "SELECT password FROM user WHERE username = '"+ std::string(esc_username) + "';";
    if (mysql_query(sql, query_sql.c_str()) != 0) {
      char buf[256];
      sprintf(buf,"Login query failed: %s", mysql_error(sql));
      LOGERROR(buf);
      return false;
    }

    MYSQL_RES* res = mysql_store_result(sql);
    if (!res) {
      char buf[256];
      sprintf(buf,"Login get result failed: %s", mysql_error(sql));
      LOGERROR(buf);
      return false;
    }

    // 验证结果
    bool success = false;
    if (mysql_num_rows(res) == 1) { // 用户名存在
      MYSQL_ROW row = mysql_fetch_row(res);
      if (row && row[0] && std::string(row[0]) == password) { // 密码匹配
        success = true;
        char buf[256];
        sprintf(buf,"Login success: username = %s", username.c_str());
        LOGINFO(buf);
      } else {
        char buf[256];
        sprintf(buf,"Login failed: password incorrect for %s", username.c_str());
        LOGWARNING(buf);
      }
    } else { // 用户名不存在
      char buf[256];
      sprintf(buf,"Login failed: username %s not exists", username.c_str());
      LOGWARNING(buf);
    }

    mysql_free_result(res); // 释放结果集
    return success;
}