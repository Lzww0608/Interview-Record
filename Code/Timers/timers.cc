#include <set>
#include <functional>
#include <chrono>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <memory>
struct TimerNodeBase {
    int64_t id;
    time_t expire;
};

struct TimerNode: public TimerNodeBase {
    using Callback = std::function<void(const TimerNode&)>;
    Callback func;
    TimerNode(time_t expire, int64_t id, Callback func): func(func) {
        this->id = id;
        this->expire = expire;
    }
};

bool operator<(const TimerNodeBase& lhs, const TimerNodeBase& rhs) {
    if (lhs.expire == rhs.expire) {
        return lhs.id < rhs.id;
    }
    return lhs.expire < rhs.expire;
}

class Timer {
public:
    static time_t GetTick() {
        auto sc = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now());
        auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(sc.time_since_epoch());
        return tmp.count();
    }

    TimerNode AddTimer(time_t msec, TimerNode::Callback&& func) {
        auto it = timers.emplace(msec + GetTick(), GetId(), std::move(func));
        return static_cast<TimerNode>(*it.first);
    }

    bool DelTimer(TimerNode& node) {
        auto it = timers.find(node);
        if (it == timers.end()) {
            return false;
        }
        timers.erase(it);
        return true;
    }

    void HandleTimer(time_t now) {
        auto it = timers.begin();
        while (it != timers.end() && it->expire <= now) {
            it->func(*it);
            it = timers.erase(it);
        }
    }

    time_t TimeToSleep() {
        auto it = timers.begin();
        if (it == timers.end()) {
            return -1;
        }
        auto t = it->expire - GetTick();
        if (t <= 0) {
            return 0;
        }
        return t;
    }

private:
    std::set<TimerNode, std::less<>> timers;
    static int64_t tid;
    static int64_t GetId() {
        return tid++;
    }
};

int64_t Timer::tid = 0;


int main() {
    int epfd = epoll_create(1);
    std::unique_ptr<Timer> timer = std::make_unique<Timer>();

    timer->AddTimer(1000, [](const TimerNode& node) {
        std::cout << "evoke time is: " << Timer::GetTick() << " and id is: " << node.id << std::endl;
    });

    timer->AddTimer(2000, [](const TimerNode& node) {
        std::cout << "evoke time is: " << Timer::GetTick() << " and id is: " << node.id << std::endl;
    });

    timer->AddTimer(3000, [](const TimerNode& node) {
        std::cout << "evoke time is: " << Timer::GetTick() << " and id is: " << node.id << std::endl;
    });

    auto it = timer->AddTimer(2100, [](const TimerNode& node) {
        std::cout << "evoke time is: " << Timer::GetTick() << " and id is: " << node.id << std::endl;
    });

    timer->DelTimer(it);
    epoll_event ev[64] = {0};

    while (true) {
        int n = epoll_wait(epfd, ev, 64, timer->TimeToSleep());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        timer->HandleTimer(Timer::GetTick());
    }
    //epoll_close(epfd);
    return 0;
}