#pragma once

#include <math.h>

#include <boost/algorithm/string.hpp>
#include <boost/unordered_map.hpp>
#include <iostream>
#include <string>
#include <vector>

#include "math.hpp"
#include "stats.hpp"
#include "timer.hpp"

using namespace std;

#define COST_THRESHOLD 350

struct plan {
    double cost;            // min cost
    vector<ssid_t> orders;  // best orders
    double result_num;      // intermediate results
};

struct select_record {
    ssid_t p;
    ssid_t d;
    double v;
    bool operator>(const select_record &o) const { return v > o.v; }
    bool operator<(const select_record &o) const { return v < o.v; }
    bool operator==(const select_record &o) const { return v == o.v; }
};

template <class T>
class Minimum_maintenance {
   private:
    T *_stack[2];  // 0 store Descending sequence, 1 store other's
    int _iter[2];  // store iter
    int *_order;   // for push and pop
    int *_fakedeepcopy;

   public:
    Minimum_maintenance() {
        logstream(LOG_ERROR) << "NOT SUPPORT" << LOG_endl;
        ASSERT(0);
    }
    Minimum_maintenance(int s) {
        if (s < 1)  // protect
            s = 1;
        _iter[0] = 0;
        _iter[1] = 0;
        _stack[0] = new T[s];
        _stack[1] = new T[s];
        _order = new int[s];
        // cout<<"NEW    [] +++++++++++++++++++"<<endl;
    }
    Minimum_maintenance(const Minimum_maintenance &copy) {
        _stack[0] = copy._stack[0];
        _stack[1] = copy._stack[1];
        _iter[0] = copy._iter[0];
        _iter[1] = copy._iter[1];
        _order = copy._order;
    }
    ~Minimum_maintenance() {
        delete[] _stack[0];
        delete[] _stack[1];
        delete[] _order;
        // cout<<"DELETE [] -------------------"<<endl;
    }
    void push(const T &ms) {
        int i;
        if (_iter[0] == 0 || _stack[0][_iter[0] - 1] > ms) {
            i = 0;
        } else {
            i = 1;
        }
        _stack[i][_iter[i]] = ms;

        _order[_iter[0] + _iter[1]] = i;
        _iter[i]++;
    }
    void pop() {
        if (_iter[0] + _iter[1]) {
            _iter[_order[_iter[0] + _iter[1] - 1]]--;
        }
    }
    bool top(T &ms) {
        if (!(_iter[0] + _iter[1])) return false;
        int o = _order[_iter[0] + _iter[1] - 1];
        ms = _stack[0][_iter[0] - 1];
        return true;
    }
    bool const empty() { return _iter[0] == 0; }
};

vector<int> empty_ptypes_pos;

class Planner {
    // members
    Stats *statistic;
    vector<ssid_t> triples;
    double min_cost;
    vector<ssid_t> path;
    bool is_empty;  // help identify empty queries

    vector<int> pred_chains;  // the pred_type chains
    // for dfs
    vector<ssid_t> min_path;
    int _chains_size_div_4;
    int *min_select_record;
    unordered_map<int, shared_ptr<Minimum_maintenance<select_record>>>
        *min_select;

    // store all orders for DP
    vector<int> subgraph[11];  // for 2^10 all orders

    // functions
    // dfs traverse , traverse all the valid orders
    bool com_traverse(unsigned int pt_bits, double cost, double pre_results) {
        if (pt_bits == (1 << _chains_size_div_4) - 1) {
            // cout << "estimated cost : " << cost << endl;
            bool ctn = true;
            if (min_cost == std::numeric_limits<double>::max() &&
                cost < COST_THRESHOLD && (path[0] >= (1 << NBITS_IDX))) {
                ctn = false;  // small query
                logstream(LOG_DEBUG)
                    << "small query and use heuristic." << LOG_endl;
            }
            if (cost < min_cost) {
                min_cost = cost;
                min_path = path;
            }
            return ctn;
        }
        for (int pt_pick = 0; pt_pick < _chains_size_div_4; pt_pick++) {
            if (pt_bits & (1 << pt_pick)) continue;
            int i = 4 * pt_pick;
            double add_cost = 0;
            ssid_t o1 = triples[i];
            ssid_t p = triples[i + 1];
            ssid_t d = triples[i + 2];
            ssid_t o2 = triples[i + 3];
            if (path.size() == 0) {
                if (o1 < 0 && o2 < 0) {
                    // continue;
                    // use index vertex
                    path.push_back(p);
                    path.push_back(0);
                    path.push_back(IN);
                    path.push_back(o1);
                    add_cost = double(statistic->global_pscount[p]) /
                               Global::num_servers;  // find subjects
                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    double new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    double select_num_s = (double)statistic->global_pscount[p];
                    select_record srs = {p, d, select_num_s};
                    (*min_select)[o1] =
                        std::unique_ptr<Minimum_maintenance<select_record>>(
                            new Minimum_maintenance<select_record>(
                                2 * _chains_size_div_4));
                    (*min_select)[o1]->push(srs);
                    bool ctn = com_traverse(pt_bits, new_cost, add_cost);
                    (*min_select)[o1]->pop();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();

                    // different direction
                    path.push_back(p);
                    path.push_back(0);
                    path.push_back(OUT);
                    path.push_back(o2);
                    add_cost = double(statistic->global_pocount[p]) /
                               Global::num_servers;  // find objects
                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    double select_num_o = (double)statistic->global_pocount[p];
                    select_record sro = {p, IN, select_num_o};
                    (*min_select)[o2] =
                        std::unique_ptr<Minimum_maintenance<select_record>>(
                            new Minimum_maintenance<select_record>(
                                2 * _chains_size_div_4));
                    (*min_select)[o2]->push(sro);
                    ctn = com_traverse(pt_bits, new_cost, add_cost);
                    (*min_select)[o2]->pop();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                }
                if (o1 > 0) {
                    path.push_back(o1);
                    path.push_back(p);
                    path.push_back(d);
                    path.push_back(o2);
                    add_cost = double(statistic->global_ptcount[p]) /
                               statistic->global_pscount[p];

                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    double new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    new_add_selectivity(o1, p, o2);
                    bool ctn = com_traverse(pt_bits | (1 << pt_pick), new_cost,
                                            add_cost);
                    unadd_selectivity();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();

                } else if (o2 > 0) {
                    path.push_back(o2);
                    path.push_back(p);
                    path.push_back(IN);
                    path.push_back(o1);
                    // for type triples
                    if (p == TYPE_ID) {
                        add_cost = statistic->global_pscount[o2];
                        // cout << "type " << o2 << " : " << add_cost << endl;
                    } else {  // normal triples
                        add_cost = double(statistic->global_ptcount[p]) /
                                   statistic->global_pocount[p];
                    }

                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    double new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    new_add_selectivity(o1, p, o2);
                    bool ctn = com_traverse(pt_bits | (1 << pt_pick), new_cost,
                                            add_cost);
                    unadd_selectivity();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                }
            } else {
                if (min_select->find(o1) != min_select->end() &&
                    !(*min_select)[o1]->empty()) {
                    path.push_back(o1);
                    path.push_back(p);
                    path.push_back(d);
                    path.push_back(o2);
                    double prune_result;
                    select_record sr;
                    if (!(*min_select)[o1]->top(sr))
                        logstream(LOG_ERROR) << "o1 on top" << LOG_endl;
                    int pre_p = sr.p;
                    int pre_d = sr.d;
                    // prune based on correlation and constant
                    if (p == TYPE_ID && o2 > 0) {
                        prune_result =
                            com_prune(pre_results, pre_p, pre_d, o2, OUT);
                        add_cost = prune_result;
                    } else if (o2 >= 0) {
                        prune_result =
                            com_prune(pre_results, pre_p, pre_d, p, OUT);
                        prune_result =
                            double(prune_result) / statistic->global_pocount[p];
                        add_cost = prune_result *
                                   (double(statistic->global_ptcount[p]) /
                                    statistic->global_pscount[p]);
                    } else {
                        prune_result =
                            com_prune(pre_results, pre_p, pre_d, p, OUT);
                        add_cost = prune_result *
                                   (double(statistic->global_ptcount[p]) /
                                    statistic->global_pscount[p]);
                    }

                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    double new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    new_add_selectivity(o1, p, o2);
                    bool ctn = com_traverse(pt_bits | (1 << pt_pick), new_cost,
                                            add_cost);
                    unadd_selectivity();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                }
                if (min_select->find(o2) != min_select->end() &&
                    !(*min_select)[o2]->empty()) {
                    path.push_back(o2);
                    path.push_back(p);
                    path.push_back(IN);
                    path.push_back(o1);
                    double prune_result;
                    select_record sr;
                    if (!(*min_select)[o2]->top(sr))
                        logstream(LOG_ERROR) << "o2 on top\n";
                    int pre_p = sr.p;
                    int pre_d = sr.d;
                    // prune based on correlation and constant
                    prune_result = com_prune(pre_results, pre_p, pre_d, p, IN);
                    if (o1 >= 0) {
                        prune_result =
                            double(prune_result) / statistic->global_pscount[p];
                    }
                    add_cost =
                        prune_result * (double(statistic->global_ptcount[p]) /
                                        statistic->global_pocount[p]);

                    // next iteration
                    // cout << "pick : " << o1 << " " << p << " " << d << " " <<
                    // o2 << endl; cout << "add_cost : " << add_cost << endl;
                    double new_cost = cost + add_cost;
                    if (new_cost > min_cost) {
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        path.pop_back();
                        continue;
                    }
                    new_add_selectivity(o1, p, o2);
                    bool ctn = com_traverse(pt_bits | (1 << pt_pick), new_cost,
                                            add_cost);
                    unadd_selectivity();
                    if (!ctn) return ctn;
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                    path.pop_back();
                }
            }
        }

        return true;
    }

    // prune using correlation estimation
    double com_prune(double pre_results, ssid_t pre_p, ssid_t pre_d, ssid_t p,
                     ssid_t d) {
        double result_num;
        double x, y;
        // handle corner case first
        if (pre_p == p) {
            return pre_results;
        }

        // out out correlation
        if (pre_d == OUT && d == OUT) {
            if (pre_p < p) {
                x = double(
                    statistic->global_ppcount[make_pair(pre_p, p)].out_out);
            } else {
                x = double(
                    statistic->global_ppcount[make_pair(p, pre_p)].out_out);
            }
            y = statistic->global_pscount[pre_p];
        }

        // in out correlation
        if (pre_d == IN && d == OUT) {
            if (pre_p < p) {
                x = double(
                    statistic->global_ppcount[make_pair(pre_p, p)].in_out);
            } else {
                x = double(
                    statistic->global_ppcount[make_pair(p, pre_p)].out_in);
            }
            y = statistic->global_pocount[pre_p];
        }

        // in in correlation
        if (pre_d == IN && d == IN) {
            if (pre_p < p) {
                x = double(
                    statistic->global_ppcount[make_pair(pre_p, p)].in_in);
            } else {
                x = double(
                    statistic->global_ppcount[make_pair(p, pre_p)].in_in);
            }
            y = statistic->global_pocount[pre_p];
        }

        // out in correlation
        if (pre_d == OUT && d == IN) {
            if (pre_p < p) {
                x = double(
                    statistic->global_ppcount[make_pair(pre_p, p)].out_in);
            } else {
                x = double(
                    statistic->global_ppcount[make_pair(p, pre_p)].in_out);
            }
            y = statistic->global_pscount[pre_p];
        }

        // cout << "prune ratio (x/y) : " << x << " / " << y  << endl;
        result_num = pre_results * (x / y);
        if (x == 0) is_empty = true;
        return result_num;
    }

    void new_add_selectivity(ssid_t o1, ssid_t p, ssid_t o2) {
        if (o1 < 0 && o2 > 0) {
            double select_num;
            if (p == TYPE_ID) {
                select_num = (double)statistic->global_pscount[o2];
                p = o2;
            } else {
                select_num = double(statistic->global_pscount[p]) /
                             statistic->global_pocount[p];
            }
            select_record sr = {p, OUT, select_num};
            if (min_select->find(o1) == min_select->end())
                (*min_select)[o1] =
                    std::unique_ptr<Minimum_maintenance<select_record>>(
                        new Minimum_maintenance<select_record>(
                            2 * _chains_size_div_4));
            (*min_select)[o1]->push(sr);
            min_select_record[min_select_record[0] + 1] = o1;
            min_select_record[min_select_record[0] + 2] = 1;
            min_select_record[0] += 2;
        } else if (o2 < 0 && o1 > 0) {
            double select_num = double(statistic->global_pocount[p]) /
                                statistic->global_pscount[p];
            select_record sr = {p, IN, select_num};
            if (min_select->find(o2) == min_select->end())
                (*min_select)[o2] =
                    std::unique_ptr<Minimum_maintenance<select_record>>(
                        new Minimum_maintenance<select_record>(
                            2 * _chains_size_div_4));
            (*min_select)[o2]->push(sr);

            min_select_record[min_select_record[0] + 1] = o2;
            min_select_record[min_select_record[0] + 2] = 1;
            min_select_record[0] += 2;
        } else if (o1 < 0 && o2 < 0) {
            double select_num_s = statistic->global_pscount[p];
            double select_num_o = statistic->global_pocount[p];
            select_record sro1 = {p, OUT, select_num_s};
            if (min_select->find(o1) == min_select->end())
                (*min_select)[o1] =
                    std::unique_ptr<Minimum_maintenance<select_record>>(
                        new Minimum_maintenance<select_record>(
                            2 * _chains_size_div_4));
            (*min_select)[o1]->push(sro1);

            select_record sro2 = {p, IN, select_num_o};
            if (min_select->find(o2) == min_select->end())
                (*min_select)[o2] =
                    std::unique_ptr<Minimum_maintenance<select_record>>(
                        new Minimum_maintenance<select_record>(
                            2 * _chains_size_div_4));
            (*min_select)[o2]->push(sro2);

            min_select_record[min_select_record[0] + 1] = o1;
            min_select_record[min_select_record[0] + 2] = o2;
            min_select_record[min_select_record[0] + 3] = 2;
            min_select_record[0] += 3;
        } else {
            min_select_record[min_select_record[0] + 1] = 0;
            min_select_record[0] += 1;
        }
    }

    void unadd_selectivity() {
        int lastpos = min_select_record[0];
        if (lastpos == 0) return;
        int lastnum = min_select_record[lastpos];
        ASSERT(lastpos > lastnum);
        int i;
        for (i = 1; i <= lastnum; i++) {
            (*min_select)[min_select_record[lastpos - i]]->pop();
        }
        min_select_record[0] -= lastnum + 1;
    }

    // remove the attr pattern query before doing the planner and transfer
    // pattern to cmd_chains
    void transfer_to_cmd_chains(vector<SPARQLQuery::Pattern> &p,
                                vector<ssid_t> &attr_pattern,
                                vector<int> &attr_pred_chains,
                                vector<ssid_t> &temp_cmd_chains) {
        for (int i = 0; i < p.size(); i++) {
            SPARQLQuery::Pattern pattern = p[i];
            if (pattern.pred_type == 0) {
                temp_cmd_chains.push_back(pattern.subject);
                temp_cmd_chains.push_back(pattern.predicate);
                temp_cmd_chains.push_back((ssid_t)pattern.direction);
                temp_cmd_chains.push_back(pattern.object);
            } else {
                attr_pattern.push_back(pattern.subject);
                attr_pattern.push_back(pattern.predicate);
                attr_pattern.push_back((ssid_t)pattern.direction);
                attr_pattern.push_back(pattern.object);

                attr_pred_chains.push_back(pattern.pred_type);
            }
        }
    }

    // used by set plan
    void set_direction(SPARQLQuery::PatternGroup &group,
                       const vector<int> &orders, const vector<string> &dirs,
                       vector<int> &ptypes_pos = empty_ptypes_pos) {
        vector<SPARQLQuery::Pattern> patterns;
        for (int i = 0; i < orders.size(); i++) {
            // number of orders starts from 1
            SPARQLQuery::Pattern pattern = group.patterns[orders[i] - 1];

            if (dirs[i] == "<") {
                pattern.direction = IN;
                ssid_t t = pattern.subject;
                pattern.subject = pattern.object;
                pattern.object = t;
            } else if (dirs[i] == ">") {
                pattern.direction = OUT;
            } else if (dirs[i] == "<<") {
                pattern.direction = IN;
                pattern.object = pattern.subject;
                pattern.subject = pattern.predicate;
                pattern.predicate = PREDICATE_ID;
            } else if (dirs[i] == ">>") {
                pattern.direction = OUT;
                pattern.subject = pattern.predicate;
                pattern.predicate = PREDICATE_ID;
            }

            if (ptypes_pos.size() != 0)
                set_ptypes_pos(ptypes_pos, dirs[i], patterns.size(),
                               orders[i] - 1);
            patterns.push_back(pattern);
        }
        group.patterns = patterns;
    }

    // used by set direction
    void set_ptypes_pos(vector<int> &ptypes_pos, const string &dir,
                        int current_order, int raw_order) {
        for (int i = 0; i < ptypes_pos.size(); i++) {
            // check if any pos need to be changed
            if (ptypes_pos[i] / 4 == raw_order) {
                if (dir == "<") {
                    switch (ptypes_pos[i] % 4) {
                        case 0:
                            ptypes_pos[i] = current_order * 4 + 3;
                            break;
                        case 1:
                            ptypes_pos[i] = current_order * 4 + 1;
                            break;
                        case 3:
                            ptypes_pos[i] = current_order * 4 + 0;
                            break;
                        default:
                            ptypes_pos[i] =
                                current_order * 4 + ptypes_pos[i] % 4;
                    }
                } else if (dir == ">") {
                    ptypes_pos[i] = current_order * 4 + ptypes_pos[i] % 4;
                } else if (dir == "<<") {
                    switch (ptypes_pos[i] % 4) {
                        case 0:
                            ptypes_pos[i] = current_order * 4 + 3;
                            break;
                        case 1:
                            ptypes_pos[i] = current_order * 4 + 0;
                            break;
                        default:
                            ptypes_pos[i] =
                                current_order * 4 + ptypes_pos[i] % 4;
                    }
                } else if (dir == ">>") {
                    switch (ptypes_pos[i] % 4) {
                        case 1:
                            ptypes_pos[i] = current_order * 4 + 0;
                            break;
                        default:
                            ptypes_pos[i] =
                                current_order * 4 + ptypes_pos[i] % 4;
                    }
                }
            }
        }
    }

   public:
    Planner() {}

    Planner(int tid, DGraph *graph, Stats *statistic) : statistic(statistic) {}

    bool generate_for_patterns(vector<SPARQLQuery::Pattern> &patterns,
                               bool test) {
        // transfer from patterns to temp_cmd_chains, may cause performance
        // decrease
        vector<ssid_t> temp_cmd_chains;
        vector<ssid_t> attr_pattern;
        vector<int> attr_pred_chains;
        transfer_to_cmd_chains(patterns, attr_pattern, attr_pred_chains,
                               temp_cmd_chains);
        min_path.clear();
        path.clear();
        is_empty = false;
        double cost = 0;
        min_cost = std::numeric_limits<double>::max();
        if (min_cost != std::numeric_limits<double>::max()) ASSERT(false);

        uint64_t t_prepare1 = timer::get_usec();
        // prepare for heuristic
        for (int i = 0, ilimit = temp_cmd_chains.size(); i < ilimit;
             i = i + 4) {
            if (temp_cmd_chains[i] >= (1 << NBITS_IDX) ||
                temp_cmd_chains[i + 3] >= (1 << NBITS_IDX)) {
                if (i == 0) break;
                int ta, tb, tc, td;
                ta = temp_cmd_chains[i];
                tb = temp_cmd_chains[i + 1];
                tc = temp_cmd_chains[i + 2];
                td = temp_cmd_chains[i + 3];
                temp_cmd_chains[i] = temp_cmd_chains[0];
                temp_cmd_chains[i + 1] = temp_cmd_chains[1];
                temp_cmd_chains[i + 2] = temp_cmd_chains[2];
                temp_cmd_chains[i + 3] = temp_cmd_chains[3];
                temp_cmd_chains[0] = ta;
                temp_cmd_chains[1] = tb;
                temp_cmd_chains[2] = tc;
                temp_cmd_chains[3] = td;
                break;
            }
        }
        uint64_t t_prepare2 = timer::get_usec();
        // cout << "prepare time : " << t_prepare2 - t_prepare1 << " us" <<
        // endl;

        uint64_t t_traverse1 = timer::get_usec();
        this->triples = temp_cmd_chains;
        this->min_select =
            new unordered_map<int,
                              shared_ptr<Minimum_maintenance<select_record>>>;
        _chains_size_div_4 = temp_cmd_chains.size() / 4;
        min_select_record = new int[1 + 6 * _chains_size_div_4];
        min_select_record[0] = 0;
        com_traverse(0, cost, 0);
        delete[] min_select_record;
        delete this->min_select;
        uint64_t t_traverse2 = timer::get_usec();
        // cout << "traverse time : " << t_traverse2 - t_traverse1 << " us" <<
        // endl;

        if (is_empty == true) {
            logstream(LOG_INFO) << "Identified empty result query." << LOG_endl;
            logstream(LOG_INFO) << "Query planning is finished." << LOG_endl;
            return false;
        }

        logstream(LOG_DEBUG)
            << "Query planning for one part is finished." << LOG_endl;
        logstream(LOG_DEBUG) << "Estimated cost: " << min_cost << LOG_endl;

        if (test) return true;
        // transfer from min_path to patterns
        patterns.clear();
        for (int i = 0; i < min_path.size() / 4; i++) {
            SPARQLQuery::Pattern pattern(min_path[4 * i], min_path[4 * i + 1],
                                         min_path[4 * i + 2],
                                         min_path[4 * i + 3]);
            pattern.pred_type = 0;
            patterns.push_back(pattern);
        }
        // add_attr_pattern to the end of patterns
        for (int i = 0; i < attr_pred_chains.size(); i++) {
            SPARQLQuery::Pattern pattern(
                attr_pattern[4 * i], attr_pattern[4 * i + 1],
                attr_pattern[4 * i + 2], attr_pattern[4 * i + 3]);
            pattern.pred_type = attr_pred_chains[i];
            patterns.push_back(pattern);
        }
        return true;
    }

    bool generate_for_group(SPARQLQuery::PatternGroup &group, bool test) {
        bool success = true;
        if (group.patterns.size() > 0)
            success = generate_for_patterns(group.patterns, test);
        for (auto &g : group.unions) success = generate_for_group(g, test);
        return success;
    }

    bool generate_plan(SPARQLQuery &r) {
        return generate_for_group(r.pattern_group, false);
    }

    bool test_plan(SPARQLQuery &r) {
        return generate_for_group(r.pattern_group, true);
    }

    // set user-defuned query plan
    // @return: false if no plan is set
    bool set_plan(SPARQLQuery::PatternGroup &group, istream &fmt_stream,
                  vector<int> &ptypes_pos = empty_ptypes_pos) {
        if (!fmt_stream.good()) {
            logstream(LOG_WARNING) << "Failed to read format file!" << LOG_endl;
            return false;
        }

        // read user-defined query plan file
        vector<int> orders;
        vector<string> dirs;

        int order;
        string dir = ">";
        int nunions = 0, noptionals = 0;
        string line;
        while (std::getline(fmt_stream, line)) {
            boost::trim(line);
            if (boost::starts_with(line, "#") || line.empty()) {
                continue;  // skip comments and blank lines
            } else if (line == "{") {
                continue;
            } else if (line == "}") {
                break;
            } else if (boost::starts_with(boost::to_lower_copy(line),
                                          "union")) {
                set_plan(group.unions[nunions], fmt_stream);
                nunions++;
                continue;
            } else if (boost::starts_with(boost::to_lower_copy(line),
                                          "optional")) {
                set_plan(group.optional[noptionals], fmt_stream);
                noptionals++;
                continue;
            }

            istringstream iss(line);
            iss >> order >> dir;
            dirs.push_back(dir);
            orders.push_back(order);
        }

        // correctness check
        if (orders.size() < group.patterns.size()) {
            logstream(LOG_ERROR) << "wrong format file content!" << LOG_endl;
            return false;
        }

        set_direction(group, orders, dirs, ptypes_pos);
        return true;
    }
};
