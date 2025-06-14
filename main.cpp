#include <bits/stdc++.h>
#include "parser.h"

#define all(v) (v).begin(), (v).end()

using namespace std;

const string TRACE = "trace";
const string SHOW_STATS = "stats";
const string SCHED_ALGOS[9] = {"", "FCFS", "RR-", "SPN", "SRT", "HRRN", "FB-1", "FB-2i", "AGING"};

bool compareByService(const tuple<string, int, int> &a, const tuple<string, int, int> &b) {
    return get<2>(a) < get<2>(b);
}

bool compareByArrival(const tuple<string, int, int> &a, const tuple<string, int, int> &b) {
    return get<1>(a) < get<1>(b);
}

bool sortByResponseRatio(const tuple<string, double, int> &a, const tuple<string, double, int> &b) {
    return get<1>(a) > get<1>(b);
}

bool priorityOrder(const tuple<int, int, int> &a, const tuple<int, int, int> &b) {
    return (get<0>(a) == get<0>(b)) ? (get<2>(a) > get<2>(b)) : (get<0>(a) > get<0>(b));
}

void resetTimeline() {
    for (int i = 0; i < last_instant; ++i)
        for (int j = 0; j < process_count; ++j)
            timeline[i][j] = ' ';
}

string extractName(const tuple<string, int, int> &proc) { return get<0>(proc); }
int arrivalOf(const tuple<string, int, int> &proc) { return get<1>(proc); }
int serviceOf(const tuple<string, int, int> &proc) { return get<2>(proc); }
int priorityOf(const tuple<string, int, int> &proc) { return get<2>(proc); }

double computeResponseRatio(int wait, int service) {
    return (wait + service) / static_cast<double>(service);
}

void markWaitTime() {
    for (int i = 0; i < process_count; ++i) {
        int arrival = arrivalOf(processes[i]);
        for (int t = arrival; t < finishTime[i]; ++t) {
            if (timeline[t][i] != '*') timeline[t][i] = '.';
        }
    }
}

void FCFS() {
    int currentTime = arrivalOf(processes[0]);
    for (int i = 0; i < process_count; ++i) {
        int idx = i;
        int at = arrivalOf(processes[i]);
        int st = serviceOf(processes[i]);

        finishTime[idx] = currentTime + st;
        turnAroundTime[idx] = finishTime[idx] - at;
        normTurn[idx] = turnAroundTime[idx] / static_cast<double>(st);

        for (int j = currentTime; j < finishTime[idx]; ++j) timeline[j][idx] = '*';
        for (int j = at; j < currentTime; ++j) timeline[j][idx] = '.';

        currentTime += st;
    }
}

void RoundRobin(int baseQuantum) {
    queue<pair<int, int>> q;
    int j = 0;
    if (arrivalOf(processes[j]) == 0) q.emplace(j++, serviceOf(processes[j - 1]));

    int quantum = baseQuantum;

    for (int t = 0; t < last_instant; ++t) {
        if (!q.empty()) {
            auto &[idx, remTime] = q.front();
            remTime--;
            quantum--;
            timeline[t][idx] = '*';

            if (quantum == 0 && remTime == 0) {
                finishTime[idx] = t + 1;
                turnAroundTime[idx] = finishTime[idx] - arrivalOf(processes[idx]);
                normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
                q.pop(); quantum = baseQuantum;
            } else if (quantum == 0 && remTime > 0) {
                q.push({idx, remTime}); q.pop(); quantum = baseQuantum;
            } else if (quantum > 0 && remTime == 0) {
                finishTime[idx] = t + 1;
                turnAroundTime[idx] = finishTime[idx] - arrivalOf(processes[idx]);
                normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
                q.pop(); quantum = baseQuantum;
            }
        }
        while (j < process_count && arrivalOf(processes[j]) == t + 1)
            q.emplace(j++, serviceOf(processes[j - 1]));
    }
    markWaitTime();
}

void SPN() {
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    int j = 0;
    for (int t = 0; t < last_instant; ++t) {
        while (j < process_count && arrivalOf(processes[j]) <= t) {
            pq.push({serviceOf(processes[j]), j}); j++;
        }
        if (!pq.empty()) {
            int idx = pq.top().second; pq.pop();
            int st = serviceOf(processes[idx]);
            int at = arrivalOf(processes[idx]);

            for (int i = at; i < t; ++i) timeline[i][idx] = '.';
            for (int i = t; i < t + st; ++i) timeline[i][idx] = '*';

            finishTime[idx] = t + st;
            turnAroundTime[idx] = finishTime[idx] - at;
            normTurn[idx] = turnAroundTime[idx] / static_cast<double>(st);
            t += st - 1;
        }
    }
}

void SRT() {
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    int j = 0;
    for (int t = 0; t < last_instant; ++t) {
        while (j < process_count && arrivalOf(processes[j]) == t) {
            pq.push({serviceOf(processes[j]), j}); j++;
        }
        if (!pq.empty()) {
            auto [remTime, idx] = pq.top(); pq.pop();
            timeline[t][idx] = '*';
            if (remTime > 1) pq.push({remTime - 1, idx});
            else {
                finishTime[idx] = t + 1;
                turnAroundTime[idx] = finishTime[idx] - arrivalOf(processes[idx]);
                normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
            }
        }
    }
    markWaitTime();
}

void HRRN() {
    vector<tuple<string, double, int>> active;
    int j = 0;
    for (int t = 0; t < last_instant; ++t) {
        while (j < process_count && arrivalOf(processes[j]) <= t) {
            active.emplace_back(extractName(processes[j]), 1.0, 0); j++;
        }
        for (auto &proc : active) {
            int idx = processToIndex[get<0>(proc)];
            get<1>(proc) = computeResponseRatio(t - arrivalOf(processes[idx]), serviceOf(processes[idx]));
        }
        sort(all(active), sortByResponseRatio);
        if (!active.empty()) {
            int idx = processToIndex[get<0>(active[0])];
            int dur = serviceOf(processes[idx]);
            for (int k = 0; k < dur && t < last_instant; ++k, ++t)
                timeline[t][idx] = '*';
            finishTime[idx] = t;
            turnAroundTime[idx] = t - arrivalOf(processes[idx]);
            normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
            t--; active.erase(active.begin());
        }
    }
    markWaitTime();
}

void FB1() {
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    unordered_map<int, int> remTime;
    int j = 0;
    if (arrivalOf(processes[0]) == 0) {
        pq.emplace(0, 0); remTime[0] = serviceOf(processes[0]); j++;
    }
    for (int t = 0; t < last_instant; ++t) {
        if (!pq.empty()) {
            auto [priority, idx] = pq.top(); pq.pop();
            remTime[idx]--;
            timeline[t][idx] = '*';
            if (remTime[idx] == 0) {
                finishTime[idx] = t + 1;
                turnAroundTime[idx] = finishTime[idx] - arrivalOf(processes[idx]);
                normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
            } else {
                if (pq.size() >= 1) pq.emplace(priority + 1, idx);
                else pq.emplace(priority, idx);
            }
        }
        while (j < process_count && arrivalOf(processes[j]) == t + 1) {
            pq.emplace(0, j); remTime[j] = serviceOf(processes[j]); j++;
        }
    }
    markWaitTime();
}

void FB2i() {
    priority_queue<pair<int, int>, vector<pair<int, int>>, greater<>> pq;
    unordered_map<int, int> remTime;
    int j = 0;
    if (arrivalOf(processes[0]) == 0) {
        pq.emplace(0, 0); remTime[0] = serviceOf(processes[0]); j++;
    }
    for (int t = 0; t < last_instant; ++t) {
        if (!pq.empty()) {
            auto [priority, idx] = pq.top(); pq.pop();
            int q = pow(2, priority);
            int tmp = t;
            while (q-- && remTime[idx] > 0 && tmp < last_instant) {
                timeline[tmp++][idx] = '*';
                remTime[idx]--;
            }
            if (remTime[idx] > 0) pq.emplace(priority + 1, idx);
            else {
                finishTime[idx] = tmp;
                turnAroundTime[idx] = tmp - arrivalOf(processes[idx]);
                normTurn[idx] = turnAroundTime[idx] / static_cast<double>(serviceOf(processes[idx]));
            }
            t = tmp - 1;
        }
        while (j < process_count && arrivalOf(processes[j]) <= t + 1) {
            pq.emplace(0, j); remTime[j] = serviceOf(processes[j]); j++;
        }
    }
    markWaitTime();
}

void Aging(int q) {
    vector<tuple<int, int, int>> qvec;
    int j = 0, currIdx = -1;
    for (int t = 0; t < last_instant; ++t) {
        while (j < process_count && arrivalOf(processes[j]) <= t)
            qvec.emplace_back(priorityOf(processes[j]), j++, 0);
        for (auto &entry : qvec) {
            if (get<1>(entry) == currIdx) {
                get<2>(entry) = 0; get<0>(entry) = priorityOf(processes[currIdx]);
            } else {
                get<0>(entry)++; get<2>(entry)++;
            }
        }
        sort(all(qvec), priorityOrder);
        currIdx = get<1>(qvec[0]);
        for (int i = 0; i < q && t < last_instant; ++i, ++t)
            timeline[t][currIdx] = '*';
        t--;
    }
    markWaitTime();
}

void runAlgorithm(char algo, int quantum, const string &op) {
    if (op == TRACE) cout << SCHED_ALGOS[algo - '0'] << (algo == '2' ? to_string(quantum) : "") << " ";
    switch (algo) {
        case '1': FCFS(); break;
        case '2': RoundRobin(quantum); break;
        case '3': SPN(); break;
        case '4': SRT(); break;
        case '5': HRRN(); break;
        case '6': FB1(); break;
        case '7': FB2i(); break;
        case '8': Aging(quantum); break;
    }
}

int main() {
    parse();
    for (int i = 0; i < (int)algorithms.size(); ++i) {
        resetTimeline();
        runAlgorithm(algorithms[i].first, algorithms[i].second, operation);
        if (operation == TRACE) printTimeline(i);
        else if (operation == SHOW_STATS) printStats(i);
        cout << "\n";
    }
    return 0;
}
