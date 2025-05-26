#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <random>
#include <memory>

using namespace std;

// Задача с приоритетом (чем меньше значение — тем выше приоритет)
struct Task {
    int id;
    int priority;

    Task(int id, int priority) : id(id), priority(priority) {}

    bool operator<(const Task& other) const {
        return priority > other.priority;  // обратный порядок: меньший приоритет — выше
    }
};

// Класс сервера, обрабатывающего задачи
class Server {
public:
    Server(int serverId) : id(serverId), stopFlag(false), currentLoad(0) {
        workerThread = thread(&Server::run, this);
    }

    ~Server() {
        stopFlag = true;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void addTask(const Task& task) {
        lock_guard<mutex> lock(mtx);
        taskQueue.push(task);
        ++currentLoad;
    }

    int getLoad() const {
        return currentLoad;
    }

    int getId() const {
        return id;
    }

private:
    int id;
    priority_queue<Task> taskQueue;
    atomic<int> currentLoad;
    atomic<bool> stopFlag;
    thread workerThread;
    mutable mutex mtx;

    void run() {
        while (!stopFlag) {
            Task task(0, 0);
            bool hasTask = false;

            {
                lock_guard<mutex> lock(mtx);
                if (!taskQueue.empty()) {
                    task = taskQueue.top();
                    taskQueue.pop();
                    --currentLoad;
                    hasTask = true;
                }
            }

            if (hasTask) {
                cout << "Сервер " << id << " обрабатывает задачу " << task.id
                     << " с приоритетом " << task.priority << endl;
                this_thread::sleep_for(chrono::milliseconds(100 + rand() % 200));
            } else {
                this_thread::sleep_for(chrono::milliseconds(100));
            }
        }
    }
};

// Кластер серверов
class Cluster {
public:
    Cluster(int initial = 5) : initialServerCount(initial), loadThreshold(0.8) {
        for (int i = 1; i <= initialServerCount; ++i) {
            servers.emplace_back(make_unique<Server>(i));
        }
    }

    void addTask(const Task& task) {
        Server* leastLoaded = nullptr;
        int minLoad = numeric_limits<int>::max();

        for (const auto& server : servers) {
            int load = server->getLoad();
            if (load < minLoad) {
                minLoad = load;
                leastLoaded = server.get();
            }
        }

        if (leastLoaded) {
            leastLoaded->addTask(task);

            if (servers.size() == initialServerCount &&
                static_cast<double>(minLoad + 1) > loadThreshold * initialServerCount) {
                evaluateClusterLoad();
            }
        }
    }

    void printStatus() const {
        cout << "\nСостояние кластера:\n";
        for (const auto& server : servers) {
            cout << "Сервер " << server->getId() << ": " << server->getLoad() << " задач\n";
        }
        cout << endl;
    }

private:
    vector<unique_ptr<Server>> servers;
    const int initialServerCount;
    const double loadThreshold;
    mutable mutex mtx;

    void evaluateClusterLoad() {
        lock_guard<mutex> lock(mtx);

        int totalLoad = 0;
        for (const auto& server : servers) {
            totalLoad += server->getLoad();
        }

        double averageLoad = static_cast<double>(totalLoad) / servers.size();

        if (averageLoad > loadThreshold * initialServerCount) {
            int newId = servers.size() + 1;
            servers.emplace_back(make_unique<Server>(newId));
            cout << "Добавлен новый сервер " << newId << " из-за высокой нагрузки\n";
        }
    }
};

int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    Cluster cluster;

    for (int i = 1; i <= 30; ++i) {
        int priority = 1 + rand() % 5;
        cluster.addTask(Task(i, priority));

        if (i % 5 == 0) {
            cluster.printStatus();
        }

        this_thread::sleep_for(chrono::milliseconds(200 + rand() % 300));
    }

    this_thread::sleep_for(chrono::seconds(2));
    cluster.printStatus();

    return 0;
}
