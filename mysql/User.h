#pragma once
#include "sqlconnpool.h"
#include "sqlConnRAII.h"
#include <string>

class UserDao {
public:
    // 注册：返回true表示成功，false表示失败（用户名已存在或错误）
    static bool Register(const std::string& username, const std::string& password);

    // 登录：返回true表示成功，false表示失败（用户名不存在或密码错误）
    static bool Login(const std::string& username, const std::string& password);
};
