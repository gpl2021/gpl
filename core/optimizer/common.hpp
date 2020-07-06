#pragma once

#include <vector>

#include "type.hpp"

#define MINIMUM_COUNT_THRESHOLD \
    0.01  // count below that this value will be abandoned
#define COST_THRESHOLD 1000
#define AA 4
#define BB 2.25
#define CC 1

// for more fine-grained cost model
#define AA_full 0.404
#define AA_early 0.33
#define BB_ifor 0.006
#define CC_const_known 0.123
#define CC_unknown 0.139
#define CACHE_ratio 0  // 0 or 0.537 or 1/1.86

using namespace std;

struct plan {
    double cost;        // min cost
    double result_num;  // intermediate results

    vector<ssid_t> orders;  // best orders
};

struct Cost_result {
    double add_cost;
    double prune_ratio;
    double correprune_boost_results;
    double condprune_results;
    vector<double> updated_result_table;

    void init() {
        add_cost = 0;
        correprune_boost_results = 0;
        condprune_results = 0;
    }

    double cal_first() {
        for (size_t j = 0; j < updated_result_table.size(); j += 2) {
            correprune_boost_results += updated_result_table[j];
        }
        condprune_results = correprune_boost_results;
        add_cost = CC_const_known * condprune_results;
        return add_cost;
    }
};

class Type_table {
    int col_num = 0;
    int row_num = 0;

   public:
    vector<double> tytable;
    // tytable store all the type info during planning
    // struct like
    /* |  0  |  -1  |  -2  |  -3  |
       |count|type..|type..|type..|
       |.....|......|......|......|
    */
    Type_table() {}
    void set_col_num(int n) { col_num = n; }
    int get_col_num() { return col_num; };
    int get_row_num() {
        if (col_num == 0) return 0;
        return tytable.size() / col_num;
    }
    double get_row_col(int r, int c) { return tytable[col_num * r + c]; }
    void set_row_col(int r, int c, double val) {
        tytable[col_num * r + c] = val;
    }
    void append_row_to(int r, vector<double> &updated_result_table) {
        for (int c = 0; c < col_num; c++)
            updated_result_table.push_back(get_row_col(r, c));
    }
    void append_newv_row_to(int r, vector<double> &updated_result_table,
                            double val) {
        updated_result_table.push_back(val);
        for (int c = 1; c < col_num; c++)
            updated_result_table.push_back(get_row_col(r, c));
    }
};
