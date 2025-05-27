#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <random>
#include <chrono>
#include <memory>
#include <string>

using namespace std;

// Типы транспортных средств
enum class VehicleType {
    CAR,
    EMERGENCY
};

// Структура ТС
struct Vehicle {
    int id;
    VehicleType type;
    chrono::system_clock::time_point arrivalTime;

    Vehicle(int id, VehicleType type)
        : id(id), type(type), arrivalTime(chrono::system_clock::now()) {}
};

// Класс, моделирующий перекресток
class Intersection {
public:
    Intersection(int intersectionId) : id(intersectionId), stopFlag(false) {
        controlThread = thread(&Intersection::trafficControlLoop, this);
    }

    ~Intersection() {
        stopFlag = true;
        if (controlThread.joinable()) {
            controlThread.join();
        }
    }

    void addVehicle(VehicleType type, const string& direction) {
        static atomic<int> nextVehicleId(1);
        lock_guard<mutex> lock(queueMutex);

        Vehicle vehicle(nextVehicleId++, type);

        if (direction == "north") north.push(vehicle);
        else if (direction == "south") south.push(vehicle);
        else if (direction == "east")  east.push(vehicle);
        else if (direction == "west")  west.push(vehicle);

        cout << "Перекресток " << id << ": ТС " << vehicle.id
             << " (" << (type == VehicleType::EMERGENCY ? "Экстренное" : "Обычное")
             << ") прибыло с направления " << direction << endl;

        if (type == VehicleType::EMERGENCY) emergencyFlag = true;

        cv.notify_one();
    }

    void printStatus() {
        lock_guard<mutex> lock(queueMutex);
        cout << "\nСтатус перекрестка " << id << ":\n"
             << "Север: " << north.size() << " | Юг: " << south.size()
             << " | Восток: " << east.size() << " | Запад: " << west.size()
             << " | Всего: " << totalVehicles()
             << "\nРежим: " << lightStateToString(currentState) << "\n\n";
    }

private:
    int id;
    enum class LightState { NS, EW, EMERGENCY, CONGESTION };

    queue<Vehicle> north, south, east, west;
    LightState currentState = LightState::NS;

    atomic<bool> emergencyFlag{false};
    atomic<bool> congestionFlag{false};
    atomic<bool> stopFlag{false};

    mutex queueMutex;
    condition_variable cv;
    thread controlThread;

    chrono::seconds nsTime{10}, ewTime{10};

    // Основной цикл управления светофором
    void trafficControlLoop() {
        while (!stopFlag) {
            unique_lock<mutex> lock(queueMutex);

            updateCongestion();

            if (emergencyFlag) {
                currentState = LightState::EMERGENCY;
                processEmergencyAll();
                emergencyFlag = false;
                lock.unlock();
                this_thread::sleep_for(chrono::seconds(5));
                continue;
            }

            if (congestionFlag) {
                currentState = LightState::CONGESTION;
                processCongestionMode();
                congestionFlag = false;
                continue;
            }

            if (totalVehicles() > 7) adjustTiming();

            if (currentState == LightState::NS) {
                processDirection(north, south, "Север-Юг");
                currentState = LightState::EW;
            } else {
                processDirection(east, west, "Восток-Запад");
                currentState = LightState::NS;
            }

            lock.unlock();
            this_thread::sleep_for(currentState == LightState::NS ? nsTime : ewTime);
        }
    }

    void processDirection(queue<Vehicle>& q1, queue<Vehicle>& q2, const string& direction) {
        cout << "Перекресток " << id << ": " << direction << " зеленый" << endl;
        processEmergencies(q1);
        processEmergencies(q2);

        if (!q1.empty()) {
            auto v = q1.front(); q1.pop();
            cout << "ТС " << v.id << " проехало по направлению " << direction << endl;
        }

        if (!q2.empty()) {
            auto v = q2.front(); q2.pop();
            cout << "ТС " << v.id << " проехало по направлению " << direction << endl;
        }
    }

    void processEmergencies(queue<Vehicle>& q) {
        queue<Vehicle> temp;
        while (!q.empty()) {
            auto v = q.front(); q.pop();
            if (v.type == VehicleType::EMERGENCY) {
                cout << "ПРИОРИТЕТ: Экстренное ТС " << v.id << " проехало" << endl;
            } else {
                temp.push(v);
            }
        }
        q = move(temp);
    }

    void processEmergencyAll() {
        cout << "Перекресток " << id << ": Режим экстренной службы" << endl;
        processEmergencies(north);
        processEmergencies(south);
        processEmergencies(east);
        processEmergencies(west);
    }

    void processCongestionMode() {
        cout << "Перекресток " << id << ": Аварийный режим из-за затора\n";
        for (int i = 0; i < 2; ++i) {
            processDirection(north, south, "С-Ю (авар.)");
            processDirection(east, west, "В-З (авар.)");
        }
    }

    void updateCongestion() {
        if (totalVehicles() > 10) congestionFlag = true;
    }

    void adjustTiming() {
        size_t ns = north.size() + south.size();
        size_t ew = east.size() + west.size();

        if (ns > ew * 2) {
            nsTime = chrono::seconds(15);
            ewTime = chrono::seconds(5);
        } else if (ew > ns * 2) {
            nsTime = chrono::seconds(5);
            ewTime = chrono::seconds(15);
        } else {
            nsTime = ewTime = chrono::seconds(10);
        }

        cout << "Перекресток " << id << ": Изменены интервалы — С-Ю: " 
             << nsTime.count() << "с, В-З: " << ewTime.count() << "с" << endl;
    }

    size_t totalVehicles() const {
        return north.size() + south.size() + east.size() + west.size();
    }

    string lightStateToString(LightState state) const {
        switch (state) {
            case LightState::NS: return "Север-Юг";
            case LightState::EW: return "Восток-Запад";
            case LightState::EMERGENCY: return "Экстренный";
            case LightState::CONGESTION: return "Затор";
        }
        return "Неизвестно";
    }
};

int main() {
    constexpr int INTERSECTIONS = 10;
    vector<unique_ptr<Intersection>> intersections;

    for (int i = 1; i <= INTERSECTIONS; ++i) {
        intersections.push_back(make_unique<Intersection>(i));
    }

    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<> distIntersection(0, INTERSECTIONS - 1);
    uniform_int_distribution<> distDir(0, 3);
    uniform_int_distribution<> distType(0, 20);

    vector<string> directions = {"north", "south", "east", "west"};

    for (int i = 0; i < 100; ++i) {
        int idx = distIntersection(rng);
        string dir = directions[distDir(rng)];
        VehicleType type = (distType(rng) == 0) ? VehicleType::EMERGENCY : VehicleType::CAR;

        intersections[idx]->addVehicle(type, dir);

        if (i % 20 == 0) {
            for (auto& x : intersections) x->printStatus();
        }

        this_thread::sleep_for(chrono::milliseconds(200 + rand() % 300));
    }

    this_thread::sleep_for(chrono::seconds(5));
    for (auto& x : intersections) x->printStatus();

    return 0;
}
