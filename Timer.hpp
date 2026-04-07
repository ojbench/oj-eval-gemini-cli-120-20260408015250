#pragma once

#include "Task.hpp"
#include <vector>

class TimingWheel;

class TaskNode {
    friend class TimingWheel;
    friend class Timer;
public:
    TaskNode(Task* task, size_t time) : task(task), time(time), next(nullptr), prev(nullptr), wheel(nullptr), slot(0) {}
    ~TaskNode() {}

private:
    Task* task;
    TaskNode* next, *prev;
    size_t time;
    TimingWheel* wheel;
    size_t slot;
};

class TimingWheel {
    friend class Timer;
public:
    TimingWheel(size_t size, size_t interval) : size(size), interval(interval), current_slot(0) {
        slots = new TaskNode*[size];
        for (size_t i = 0; i < size; ++i) {
            slots[i] = nullptr;
        }
    }
    ~TimingWheel() {
        for (size_t i = 0; i < size; ++i) {
            TaskNode* curr = slots[i];
            while (curr) {
                TaskNode* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        delete[] slots;
    }

    void insert(size_t slot, TaskNode* node) {
        node->next = slots[slot];
        node->prev = nullptr;
        if (slots[slot]) {
            slots[slot]->prev = node;
        }
        slots[slot] = node;
        node->wheel = this;
        node->slot = slot;
    }

    void remove(TaskNode* node) {
        if (node->prev) {
            node->prev->next = node->next;
        } else {
            slots[node->slot] = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        node->wheel = nullptr;
    }

    TaskNode* take_all_tasks(size_t slot) {
        TaskNode* head = slots[slot];
        slots[slot] = nullptr;
        TaskNode* curr = head;
        while (curr) {
            curr->wheel = nullptr;
            curr = curr->next;
        }
        return head;
    }

private:
    const size_t size, interval;
    size_t current_slot = 0;
    TaskNode** slots;
};

class Timer {
public:    
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&) = delete;
    Timer& operator=(Timer&&) = delete;

    Timer() {
        second_wheel = new TimingWheel(60, 1);
        minute_wheel = new TimingWheel(60, 60);
        hour_wheel = new TimingWheel(24, 3600);
    }

    ~Timer() {
        delete second_wheel;
        delete minute_wheel;
        delete hour_wheel;
    }

    TaskNode* addTask(Task* task) {
        TaskNode* node = new TaskNode(task, task->getFirstInterval());
        if (addTaskNode(node, 0)) {
            return node;
        }
        return nullptr;
    }

    void cancelTask(TaskNode *p) {
        if (p) {
            if (p->wheel) {
                p->wheel->remove(p);
            }
            delete p;
        }
    }

    std::vector<Task*> tick() {
        std::vector<Task*> triggered_tasks;

        second_wheel->current_slot = (second_wheel->current_slot + 1) % 60;
        bool minute_tick = (second_wheel->current_slot == 0);

        if (minute_tick) {
            minute_wheel->current_slot = (minute_wheel->current_slot + 1) % 60;
            bool hour_tick = (minute_wheel->current_slot == 0);

            if (hour_tick) {
                hour_wheel->current_slot = (hour_wheel->current_slot + 1) % 24;
                TaskNode* curr = hour_wheel->take_all_tasks(hour_wheel->current_slot);
                while (curr) {
                    TaskNode* next = curr->next;
                    curr->time %= hour_wheel->interval;
                    addTaskNode(curr, 1);
                    curr = next;
                }
            }

            TaskNode* curr = minute_wheel->take_all_tasks(minute_wheel->current_slot);
            while (curr) {
                TaskNode* next = curr->next;
                curr->time %= minute_wheel->interval;
                addTaskNode(curr, 0);
                curr = next;
            }
        }

        TaskNode* curr = second_wheel->take_all_tasks(second_wheel->current_slot);
        while (curr) {
            TaskNode* next = curr->next;
            triggered_tasks.push_back(curr->task);
            
            if (curr->task->getPeriod() > 0) {
                curr->time = curr->task->getPeriod();
                addTaskNode(curr, 0);
            } else {
                delete curr;
            }
            curr = next;
        }

        return triggered_tasks;
    }

private:
    TimingWheel* second_wheel;
    TimingWheel* minute_wheel;
    TimingWheel* hour_wheel;

    bool addTaskNode(TaskNode* node, int start_level) {
        size_t time = node->time;
        
        if (start_level <= 0) {
            if (time / 1 <= 60) {
                size_t target_slot = (second_wheel->current_slot + time / 1) % 60;
                second_wheel->insert(target_slot, node);
                node->time = time;
                return true;
            } else {
                time = time + second_wheel->current_slot * 1;
                start_level = 1;
            }
        }
        
        if (start_level <= 1) {
            if (time / 60 <= 60) {
                size_t target_slot = (minute_wheel->current_slot + time / 60) % 60;
                minute_wheel->insert(target_slot, node);
                node->time = time;
                return true;
            } else {
                time = time + minute_wheel->current_slot * 60;
                start_level = 2;
            }
        }
        
        if (start_level <= 2) {
            if (time / 3600 <= 24) {
                size_t target_slot = (hour_wheel->current_slot + time / 3600) % 24;
                hour_wheel->insert(target_slot, node);
                node->time = time;
                return true;
            } else {
                delete node;
                return false;
            }
        }
        return false;
    }
};