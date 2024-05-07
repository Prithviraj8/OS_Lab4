#include <iostream>
#include <fstream>
#include <deque>
#include <string>
#include <getopt.h>
#include<sstream>
#include "globals.h"
#include <algorithm>

using namespace std;

class Operation {
public:
    int time;
    int track;
    Operation(int t, int tr) : time(t), track(tr) {}
};
deque<Operation> operations;

class Request {
public:
    int requestNumber;
    int arrivalTime;
    int startTime;
    int endTime;
    int track;
    Request(int num, int tr) : requestNumber(num), track(tr), arrivalTime(current_time), startTime(0), endTime(0) {}
    // If req_track is private, use a getter
    int getTrack() const { return track; }
};


class IOScheduler
{
public:
//    deque<Request *> IOQueue;
    virtual ~IOScheduler() {} // Ensure we have a virtual destructor

    // Pure virtual functions that must be implemented by derived classes
    virtual Request* strategy(int current_track, int& current_direction) = 0;
    virtual void addRequest(Request* r) = 0;
    virtual void removeRequest() = 0;
    virtual bool isEmpty() = 0;


protected:
    deque<Request *> IOQueue;
};


class FIFO : public IOScheduler
{
private:
    std::deque<std::unique_ptr<Request>> IOQueue;  // Use smart pointers for automatic memory management

public:

    Request* strategy(int current_track, int& current_direction) override{
        if (!IOQueue.empty()) {
            Request* r = IOQueue.front().get();
            // Adjust direction based on the track position relative to the current track
            current_direction = (current_track > r->track) ? -1 : 1;
            return r;
        }
        return nullptr;
    }

    void addRequest(Request* r) override {
        IOQueue.push_back(std::unique_ptr<Request>(r));
    }

    void removeRequest() override {
        if (!IOQueue.empty()) {
            IOQueue.pop_front();
        }
    }

    bool isEmpty() override {
        return IOQueue.empty();
    }
};


class SSTF : public IOScheduler
{
private:
    std::deque<std::unique_ptr<Request>> IOQueue;
    int current_index = -1;

public:
    Request* strategy(int current_track, int& current_direction) override {
        if (IOQueue.empty()) return nullptr;

        auto closest = std::min_element(IOQueue.begin(), IOQueue.end(),
                                        [&](const std::unique_ptr<Request>& a, const std::unique_ptr<Request>& b) {
                                            return std::abs(a->getTrack() - current_track) < std::abs(b->getTrack() - current_track);
                                        });

        if (closest != IOQueue.end()) {
            current_index = std::distance(IOQueue.begin(), closest);
            current_direction = ((*closest)->getTrack() > current_track) ? 1 : -1;
            return closest->get();
        }

        current_index = -1; // Reset current_index if no suitable request is found
        return nullptr;


    }

    void addRequest(Request* r) override {
        IOQueue.push_back(std::unique_ptr<Request>(r));
    }

    void removeRequest() override {
        if (!IOQueue.empty() && current_index >= 0 && current_index < IOQueue.size()) {
            auto it = IOQueue.begin() + current_index;
            IOQueue.erase(it);
        }
    }

    bool isEmpty() override {
        return IOQueue.empty();
    }
};

class Look : public IOScheduler {
private:
    std::deque<Request*> IOQueue;
    int current_index;

public:
    Look() : current_index(0) {}

    Request* strategy(int current_track, int& current_direction) override {
        bool foundRequestInDirection = false;
        int closestIndex = -1;
        int smallestDistance = std::numeric_limits<int>::max();

        for (int i = 0; i < IOQueue.size(); ++i) {
            int distance = IOQueue[i]->getTrack() - current_track;
            int absDistance = std::abs(distance);

            if ((current_direction == 1 && distance >= 0) || (current_direction == -1 && distance <= 0)) {
                if (absDistance < smallestDistance) {
                    smallestDistance = absDistance;
                    closestIndex = i;
                    foundRequestInDirection = true;
                }
            }
        }

        if (!foundRequestInDirection) {
            // Reverse the direction if no suitable request is found in the current direction
            current_direction = -current_direction;
            return this->strategy(current_track, current_direction);  // Recursively find the request in the new direction
        }

        current_index = closestIndex;  // Update current index
        return IOQueue.at(current_index);  // Return the closest request in the current direction
    }

    void addRequest(Request* r) override {
        IOQueue.push_back(r);
    }

    void removeRequest() override {
        if (current_index >= 0 && current_index < IOQueue.size()) {
            IOQueue.erase(IOQueue.begin() + current_index);
        }
    }

    bool isEmpty() override {
        return IOQueue.empty();
    }
};

class CLook : public IOScheduler {
private:
    std::deque<std::unique_ptr<Request>> IOQueue;
    int current_index = -1;

public:
    Request* strategy(int current_track, int& current_direction) override {
        if (IOQueue.empty()) return nullptr;

        current_direction = 1; // CLook always scans in the same direction
        // Find the closest request in the current direction or wrap around
        auto closest = std::min_element(IOQueue.begin(), IOQueue.end(),
                                        [current_track](const std::unique_ptr<Request>& a, const std::unique_ptr<Request>& b) {
                                            return (a->getTrack() >= current_track ? a->getTrack() : INT_MAX) <
                                                   (b->getTrack() >= current_track ? b->getTrack() : INT_MAX);
                                        });

        if (closest == IOQueue.end() || (*closest)->getTrack() < current_track) {
            // Wrap around to the smallest track number greater than currentTrack
            closest = std::min_element(IOQueue.begin(), IOQueue.end(),
                                       [](const std::unique_ptr<Request>& a, const std::unique_ptr<Request>& b) {
                                           return a->getTrack() < b->getTrack();
                                       });
        }

        if (closest != IOQueue.end()) {
            current_index = std::distance(IOQueue.begin(), closest);
            return closest->get();
        }

        current_index = -1;
        return nullptr;
    }

    void addRequest(Request* r) override {
        IOQueue.push_back(std::unique_ptr<Request>(r));
    }

    void removeRequest() override {
        if (!IOQueue.empty() && current_index != -1) {
            auto it = IOQueue.begin() + current_index;
            IOQueue.erase(it);
            current_index = -1; // Reset current_index after removal
        }
    }

    bool isEmpty() override {
        return IOQueue.empty();
    }
};

class FLook : public IOScheduler {
private:
    std::deque<Request*> newQueue;
    std::deque<Request*> *active;
    std::deque<Request*> *add;
    int current_index;

public:
    FLook() {
        active = &IOQueue;
        add = &newQueue;
        current_index = -1;  // Initialized to -1 to indicate no current request is being processed
    }

    Request* strategy(int current_track, int& current_direction) override {
        // Swap queues if the active queue is empty
        if (active->empty()) {
            std::swap(active, add);
        }

        int pos = 0;
        int closest_index = -1;
        int min_distance = std::numeric_limits<int>::max();
        bool direction_fulfilled = false;

        for (auto it = active->begin(); it != active->end(); ++it, ++pos) {
            Request* req = *it;
            int distance = req->getTrack() - current_track;

            // Check if the request is in the current direction of head movement
            if ((current_direction == 1 && distance >= 0) || (current_direction == -1 && distance <= 0)) {
                direction_fulfilled = true;
                if (abs(distance) < min_distance) {
                    closest_index = pos;
                    min_distance = abs(distance);
                }
            }
        }

        // If no request fulfills the current direction, switch directions
        if (!direction_fulfilled) {
            current_direction = -current_direction;
            return this->strategy(current_track, current_direction); // Recursively call with new direction
        }

        // Set the current index to the closest request index
        current_index = closest_index;
        return active->at(current_index);
    }

    void addRequest(Request* r) override {
        add->push_back(r);  // Add new requests to the additional queue

        // Swap queues if active is empty and add is not
        if (active->empty() && !add->empty()) {
            std::swap(active, add);
        }
    }

    void removeRequest() override {
        if (!active->empty() && current_index != -1) {
            auto it = active->begin() + current_index;
            active->erase(it);
        }
    }

    bool isEmpty() override {
        return active->empty() && add->empty();
    }
};

IOScheduler *scheduler;
Request *current_request;

deque<Request> completedRequests;

void addCompleted(Request r) {
    /* Using std::lower_bound to find the correct insertion point
    which maintains the sorted order of requests by request number.*/
    auto it = std::lower_bound(completedRequests.begin(), completedRequests.end(), r,
                               [](const Request& lhs, const Request& rhs) {
                                   return lhs.requestNumber < rhs.requestNumber;
                               });

    completedRequests.insert(it, r);
}


bool checkExitCondition()
{
    return current_request == nullptr && scheduler->isEmpty() && operations.empty();
}

void simulateHeadMovement()
{
    if (current_request != nullptr && current_request->getTrack() != current_track)
    {
        current_track += (current_request->getTrack() > current_track) ? 1 : -1;
        total_movement++;
    }
}

void changeDirection()
{
    // Reverse the current direction.
    current_direction *= -1;

    // Move the head in the new direction.
    current_track += current_direction;
    total_movement++;

    // Adjust for boundaries of the track (e.g., not moving to negative track numbers).
    if (current_track < 0)
    {
        current_track = 0;  // Reset to start if it goes negative.
        current_direction = 1;  // Switch direction to forward since it cannot go further back.
    }
}

void fetchNextRequest()
{
    // If there is no current request and the scheduler is not empty, fetch the next request.
    if (current_request == nullptr && !scheduler->isEmpty())
    {
        current_request = scheduler->strategy(current_track, current_direction);
        if (current_request != nullptr)
        {
            // Initialize start time for the request.
            current_request->startTime = current_time;
            // Update wait time and track the maximum wait time encountered.
            total_wait_time += current_time - current_request->arrivalTime;
            max_wait_time = std::max(max_wait_time, current_time - current_request->arrivalTime);

            // If the request can be immediately processed, adjust the simulation time to account for this.
            if (current_request->getTrack() == current_track)
            {
                current_time--;  // This decrements the time as the request is instantly processed.
            }
        }
        else
        {
            // If no request is suitable, change direction and adjust the track accordingly.
            changeDirection();
        }
    }
}

void processCompletedRequests()
{
    if (current_request != nullptr && current_request->getTrack() == current_track)
    {
        current_request->endTime = current_time;
        total_turnaround += current_time - current_request->arrivalTime;

        if (OPTION_V)
            printf("%5d: %d finish %d\n", current_time, current_request->requestNumber, current_time - current_request->arrivalTime);

        addCompleted(*current_request);
        scheduler->removeRequest();
        current_request = nullptr;
    }
}

void processArrivals(int& request_number) {
    // Ensure there are pending operations and the current operation is due to start.
    if (!operations.empty() && operations.front().time == current_time) {
        // Construct a new Request object and add it to the scheduler.
        Request* newRequest = new Request(request_number, operations.front().track);
        scheduler->addRequest(newRequest);

        // Verbose output if enabled.
        if (OPTION_V) {
            printf("%5d: %d add %d\n", current_time, request_number, operations.front().track);
        }

        // Remove the processed operation and increment the request number.
        operations.pop_front();
        request_number++;
    }
}


void printDetails(int request_number)
{
    for(auto & completedRequest : completedRequests)
    {
        printf("%5d: %5d %5d %5d\n",completedRequest.requestNumber, completedRequest.arrivalTime, completedRequest.startTime, completedRequest.endTime);
    }
    printf("SUM: %d %d %.4lf %.2lf %.2lf %d\n",
           current_time,                    // Total simulation time
           total_movement,               // Total head movements
           double(total_movement) / current_time,  // Average movements per unit time
           double(total_turnaround) / request_number, // Average turnaround time
           double(total_wait_time) / request_number,  // Average wait time
           max_wait_time);               // Maximum wait time
}


void simulate()
{
    int request_number = 0;

    while(true)
    {
        processArrivals(request_number);
        processCompletedRequests();
        fetchNextRequest();

        simulateHeadMovement();

        if (checkExitCondition())
            break;

        current_time++;  // Increment time at the end of the loop
    }

    printDetails(request_number);
}

void readFile(const string& fileName) {
    std::ifstream file(fileName);
    string line;
    while (getline(file, line)) {
        if (line[0] != '#') {
            std::istringstream iss(line);
            int time, track;
            if (iss >> time >> track) {
                operations.emplace_back(time, track);
            }
        }
    }
}


void parseOptions(int argc, char* argv[]) {
    int option;
    while ((option = getopt(argc, argv, "vqfs:")) != -1) {
        switch (option) {
            case 'v': OPTION_V = true; break;
            case 's':
                if (optarg != nullptr) {
                    switch (optarg[0]) {
                        case 'N': scheduler = new FIFO(); break;
                        case 'S': scheduler = new SSTF(); break;
                        case 'L': scheduler = new Look(); break;
                        case 'C': scheduler = new CLook(); break;
                        case 'F': scheduler = new FLook(); break;
                        default: break; // Handle unexpected options
                    }
                }
                break;
            default:
                std::cerr << "Unsupported option provided." << std::endl;
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    current_request = nullptr;
    OPTION_V = false;
    total_time = total_movement = total_turnaround = total_wait_time = max_wait_time = 0;
    scheduler = nullptr;

    parseOptions(argc, argv);
    string fileName = argv[optind];
    readFile(fileName);
    simulate();
}
