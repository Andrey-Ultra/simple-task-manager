#ifndef SIMPLE_TASK_MANAGER_LOG_H
#define SIMPLE_TASK_MANAGER_LOG_H
#include <iostream>
#include <string>

inline void Log(const std::string &msg) {
    std::cout << msg << std::endl;
}

#endif