#ifndef SIMPLE_TASK_MANAGER_TASKQUEUE_H
#define SIMPLE_TASK_MANAGER_TASKQUEUE_H
#include <unordered_map>
#include <set>
#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <queue>
#include <mutex>

using Clock = std::chrono::steady_clock;

enum class Status {
    IN_QUEUE,
    IN_PROGRESS,
    COMPLETED,
};

std::string StatusToString(Status s);


class TaskQueue {
public:
    explicit TaskQueue(const std::chrono::seconds timeout);


    // Client space
    size_t addTask(std::string text);

    std::optional<Status> getStatus(const size_t real_id);

    bool cancelTask(size_t real_id);

    std::optional<std::string> getAnswer(const size_t real_id);


    // Worker space
    std::optional<std::pair<size_t, std::string> > getTask();

    bool setAnswer(const size_t id, std::string answer);

    bool releaseTask(const size_t id);

    void updateWorkQueue();

private:
    bool recreateTask(const size_t id);

    bool removeTask(const size_t real_id);


    std::mutex m_;

    const std::chrono::seconds kTimeout;

    size_t real_index_ = 0;
    size_t virtual_index_ = 0;

    std::unordered_map<size_t, size_t> to_real_;
    std::unordered_map<size_t, size_t> to_virtual_;

    std::unordered_map<size_t, std::string> answers_;
    std::unordered_map<size_t, std::string> tasks_;
    std::unordered_map<size_t, Status> status_;
    // queues
    std::set<size_t> main_queue_;
    std::queue<size_t> work_queue_;
    std::unordered_map<size_t, Clock::time_point> start_time_;
};


#endif
