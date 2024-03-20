#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <atomic>
#include <iomanip> 

// Structure to represent traffic signal data
struct tData {
    std::string timestamp;
    int lights_id;
    int total_cars;
};

class tControlSimu {
private:
    std::queue<tData> t_queue;
    std::mutex mutex;
    std::condition_variable cv_p, cv_c;
    std::vector<tData> top_congested_lights;
    std::atomic<bool> is_producer_done{false}; 

    const int traffic_queue_size = 100; 
    const int lights_congestion = 3; 

public:

    void producer(const std::string& F_name) {
        std::ifstream file(F_name);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << F_name << std::endl;
            return;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            tData data;
            int hour, minute, second;
            char delimiter;
            if (!(iss >> hour >> delimiter >> minute >> delimiter >> second >> data.lights_id >> data.total_cars)) {
                std::cerr << "Error occuring in reading the line: " << line << std::endl;
                continue;
            }
            std::ostringstream oss;
            oss << std::setw(2) << std::setfill('0') << hour << ":" << std::setw(2) << std::setfill('0') << minute << ":" << std::setw(2) << std::setfill('0') << second;
            data.timestamp = oss.str();

            std::unique_lock<std::mutex> lock(mutex);
            cv_p.wait(lock, [this]() { return t_queue.size() < traffic_queue_size; });

            t_queue.push(data);
            std::cout << "Producer: Reading the traffic data - Timestamp: " << data.timestamp <<std::endl;
            std::cout << "Traffic Light ID: " << data.lights_id <<std::endl;
            std::cout<<  "Total cars present: " << data.total_cars << std::endl;

            cv_c.notify_all();
        }
        is_producer_done = true;
        cv_c.notify_all(); 
    }

    void consumer(int total_consumers) {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex);
            cv_c.wait(lock, [this]() { return !t_queue.empty() || is_producer_done; });

            if (t_queue.empty() && is_producer_done) {
                // Exit if queue is empty and producer is done
                break;
            }

            tData data = t_queue.front();
            t_queue.pop();
            lock.unlock();

            processtData(data);

            cv_p.notify_all();
        }
    }

    // Method to process traffic data and update top congested lights list
    void processtData(const tData& data) {
        if (top_congested_lights.size() < lights_congestion) {
            top_congested_lights.push_back(data);
        } else {
            auto min_element = std::min_element(top_congested_lights.begin(), top_congested_lights.end(),
                [](const tData& a, const tData& b) { return a.total_cars < b.total_cars; });
            if (data.total_cars > min_element->total_cars) {
                *min_element = data;
            }
        }

        std::sort(top_congested_lights.begin(), top_congested_lights.end(),
            [](const tData& a, const tData& b) { return a.total_cars > b.total_cars; });

        std::cout << "Consumer: updated list:- ";
        for (const auto& light : top_congested_lights) {
            std::cout << "(Lights ID: " << light.lights_id <<std::endl;
            std::cout<< " Number of Cars: " << light.total_cars << ") ";
        }
        std::cout << std::endl;
    }
    // Method to start the simulation
    void startSimulation(const std::string& F_name, int total_consumers) {
        std::vector<std::thread> p_threads, c_threads;
        // Create producer thread
        p_threads.emplace_back([this, F_name]() {
            this->producer(F_name);
        });
        // Create consumer threads
        for (int i = 0; i < total_consumers; ++i) {
            c_threads.emplace_back([this, total_consumers]() {
                this->consumer(total_consumers);
            });
        }
        // Join producer thread
        for (auto& thread : p_threads) {
            thread.join();
        }
        // Join consumer threads
        for (auto& thread : c_threads) {
            thread.join();
        }
    }
};
int main() {
    tControlSimu simulator;
    std::string F_name = "data_file.txt"; 
    int total_consumers = 3; 
    simulator.startSimulation(F_name, total_consumers);
    return 0;
}
