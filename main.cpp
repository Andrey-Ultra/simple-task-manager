#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <csignal>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "TaskQueue.h"
#include "Log.h"


static std::string EnvStr(const char *name, const std::string &default_value) {
    const char *v = std::getenv(name);
    return v ? v : default_value;
}

static int EnvInt(const char *name, int default_value) {
    const char *v = std::getenv(name);
    if (!v) return default_value;
    try { return std::stoi(v); } catch (...) { return default_value; }
}


using json = nlohmann::json;

static size_t IdFromPath(const httplib::Request &req) {
    return std::stoull(req.matches[1]);
}

static void Text(httplib::Response &res, int code, const std::string &text) {
    res.status = code;
    res.set_content(text, "text/plain");
}

static httplib::Server *g_server = nullptr;

int main() {
    const std::string host = EnvStr("HOST", "0.0.0.0");
    const int port = EnvInt("PORT", 8080);
    const int timeout = EnvInt("TASK_TIMEOUT", 300);

    TaskQueue task_queue(std::chrono::seconds{timeout});
    httplib::Server svr;
    g_server = &svr;
    std::signal(SIGTERM, [](int) { if (g_server) g_server->stop(); });
    std::signal(SIGINT, [](int) { if (g_server) g_server->stop(); });

    svr.set_logger([](const httplib::Request &req, const httplib::Response &res) {
        Log(req.method + " " + req.path + " -> " + std::to_string(res.status));
    });


    // ---------- Client ----------
    svr.Post("/tasks", [&](const httplib::Request &req, httplib::Response &res) {
        if (req.body.empty()) {
            return Text(res, 400, "empty task");
        }
        Text(res, 200, std::to_string(task_queue.addTask(req.body)));
    });

    svr.Get(R"(/tasks/(\d{1,18})/status)", [&](const httplib::Request &req, httplib::Response &res) {
        auto status = task_queue.getStatus(IdFromPath(req));
        if (!status) {
            return Text(res, 404, "not found");
        }
        Text(res, 200, StatusToString(*status));
    });

    svr.Get(R"(/tasks/(\d{1,18})/answer)", [&](const httplib::Request &req, httplib::Response &res) {
        auto answer = task_queue.getAnswer(IdFromPath(req));
        if (!answer) {
            return Text(res, 404, "no answer");
        }
        Text(res, 200, *answer);
    });

    svr.Delete(R"(/tasks/(\d{1,18}))", [&](const httplib::Request &req, httplib::Response &res) {
        if (!task_queue.cancelTask(IdFromPath(req))) {
            return Text(res, 404, "not found");
        }
        Text(res, 200, "ok");
    });

    // ---------- Worker ----------
    svr.Post("/worker/take", [&](const httplib::Request &, httplib::Response &res) {
        auto task = task_queue.getTask();
        if (!task) {
            res.status = 204;
            return;
        }
        json body = {{"id", task->first}, {"task", task->second}};
        res.set_content(body.dump(), "application/json");
    });

    svr.Post(R"(/worker/tasks/(\d{1,18})/answer)", [&](const httplib::Request &req, httplib::Response &res) {
        if (!task_queue.setAnswer(IdFromPath(req), req.body)) {
            return Text(res, 409, "task not in work");
        }
        Text(res, 200, "ok");
    });

    svr.Post(R"(/worker/tasks/(\d{1,18})/release)", [&](const httplib::Request &req, httplib::Response &res) {
        if (!task_queue.releaseTask(IdFromPath(req))) {
            return Text(res, 409, "task not in work");
        }
        Text(res, 200, "ok");
    });

    // ---------- Updater ----------
    std::atomic<bool> running = true;
    std::thread updater([&] {
        while (running) {
            task_queue.updateWorkQueue();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    Log("listening on " + host + ":" + std::to_string(port) +
    ", timeout " + std::to_string(timeout) + "s");

    const bool ok = svr.listen(host, port);

    running = false;
    updater.join();

    if (!ok) {
        Log("cannot listen on " + host + ":" + std::to_string(port));
        return 1;
    }
    Log("stopped");
}
