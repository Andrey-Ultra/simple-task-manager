#include "TaskQueue.h"

std::string StatusToString(Status s) {
    switch (s) {
        case Status::IN_QUEUE: return "in queue";
        case Status::IN_PROGRESS: return "in work";
        case Status::COMPLETED: return "complete";
    }
    return "";
}

TaskQueue::TaskQueue(const std::chrono::seconds timeout) : kTimeout(timeout) {
}

size_t TaskQueue::addTask(std::string text) {
    std::lock_guard lock(m_);
    const size_t real_id = real_index_++;
    const size_t id = virtual_index_++;

    to_real_[id] = real_id;
    to_virtual_[real_id] = id;

    tasks_[id] = std::move(text);
    status_[id] = Status::IN_QUEUE;
    main_queue_.insert(id);
    return real_id;
}

std::optional<Status> TaskQueue::getStatus(const size_t real_id) {
    std::lock_guard lock(m_);
    if (!to_virtual_.contains(real_id)) {
        return std::nullopt;
    }
    return status_[to_virtual_[real_id]];
}

bool TaskQueue::cancelTask(size_t real_id) {
    std::lock_guard lock(m_);
    return removeTask(real_id);
}

std::optional<std::string> TaskQueue::getAnswer(const size_t real_id) {
    std::lock_guard lock(m_);
    if (!to_virtual_.contains(real_id)) {
        return std::nullopt;
    }

    const size_t id = to_virtual_[real_id];
    if (status_[id] != Status::COMPLETED) {
        return std::nullopt;
    }

    std::string answer = std::move(answers_[id]);
    removeTask(real_id);
    return answer;
}

std::optional<std::pair<size_t, std::string> > TaskQueue::getTask() {
    std::lock_guard lock(m_);
    if (main_queue_.empty()) {
        return std::nullopt;
    }

    size_t id = *main_queue_.begin();
    main_queue_.erase(id);

    status_[id] = Status::IN_PROGRESS;
    work_queue_.push(id);
    start_time_[id] = Clock::now();

    return std::pair(id, tasks_[id]);
}

bool TaskQueue::setAnswer(const size_t id, std::string answer) {
    std::lock_guard lock(m_);
    if (!to_real_.contains(id) || status_[id] != Status::IN_PROGRESS) {
        return false;
    }

    start_time_.erase(id);
    status_[id] = Status::COMPLETED;
    answers_[id] = std::move(answer);

    return true;
}

bool TaskQueue::releaseTask(const size_t id) {
    std::lock_guard lock(m_);
    if (!to_real_.contains(id) || status_[id] != Status::IN_PROGRESS) {
        return false;
    }
    recreateTask(id);
    return true;
}

void TaskQueue::updateWorkQueue() {
    std::lock_guard lock(m_);
    while (!work_queue_.empty()) {
        size_t id = work_queue_.front();

        if (!start_time_.contains(id)) {
            work_queue_.pop();
            continue;
        }
        if (Clock::now() - start_time_[id] < kTimeout) {
            break;
        }
        work_queue_.pop();
        recreateTask(id);
    }
}

bool TaskQueue::recreateTask(const size_t id) {
    if (!to_real_.contains(id)) {
        return false;
    }

    std::string text = tasks_[id];
    const size_t real_id = to_real_[id];

    start_time_.erase(id);
    status_.erase(id);
    tasks_.erase(id);
    to_real_.erase(id);


    const size_t new_id = virtual_index_++;

    to_real_[new_id] = real_id;
    to_virtual_[real_id] = new_id;

    tasks_[new_id] = std::move(text);
    status_[new_id] = Status::IN_QUEUE;
    main_queue_.insert(new_id);

    return true;
}

bool TaskQueue::removeTask(const size_t real_id) {
    if (!to_virtual_.contains(real_id)) {
        return false;
    }

    size_t id = to_virtual_[real_id];

    switch (status_[id]) {
        case Status::IN_QUEUE:
            main_queue_.erase(id);
            break;
        case Status::IN_PROGRESS:
            start_time_.erase(id);
            break;
        case Status::COMPLETED:
            answers_.erase(id);
            break;
    }
    status_.erase(id);
    tasks_.erase(id);
    to_real_.erase(id);
    to_virtual_.erase(real_id);

    return true;
}
