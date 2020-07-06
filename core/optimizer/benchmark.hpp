/*
 * Copyright (c) 2016 Shanghai Jiao Tong University.
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://ipads.se.sjtu.edu.cn/projects/wukong
 *
 */
#pragma once

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "logger2.hpp"

#define SUBSET_SIZE 1000

using namespace std;

enum class cost_model_t { NO_CIRCLE = 0, CLOCKWISE, NETWORK, REAL_COST, OTHER };
/**
 * Benchmark request for cost model
 */
class CostBenchmk {
   private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &qid;
        ar &pqid;
        ar &run_times;
        ar &type;

        switch (type) {
            case cost_model_t::NO_CIRCLE:
                ar &stype;
                ar &subject;
                ar &p;
                ar &object;
                ar &otype;
                ar &latency_l2u;
                ar &latency_k2u;
                ar &latency_k2l;

                ar &row_l2u;
                ar &row_k2u_init;
                ar &row_k2u_prune;
                ar &row_k2u;
                ar &row_k2l;
                break;
            case cost_model_t::NETWORK:
                ar &data;
                break;
            case cost_model_t::REAL_COST:
                ar &read_cycle;
                ar &write_cycle;
                break;
            default: //clockwise, other
                ar &t1;
                ar &t2;
                ar &t3;
                ar &p1;
                ar &p2;
                ar &p3;
                ar &latency_k2k;
                ar &row_k2k_bind;
                ar &row_k2k_init;
                ar &row_k2k_prune;
                ar &row_k2k_result;
                break;
        }
    }

   public:
    int qid = -1;   // unused
    int pqid = -1;  // parent qid

    cost_model_t type = cost_model_t::NO_CIRCLE;
    int run_times;

    // no circle, for l2u / k2u / k2l
    ssid_t stype, subject, p, object, otype;
    double latency_l2u, latency_k2u, latency_k2l, latency_k2k;
    int row_l2u, row_k2u_init, row_k2u_prune, row_k2u, row_k2l;

    // a circle, for k2k
    ssid_t t1, t2, t3, p1, p2, p3;
    int row_k2k_init, row_k2k_bind, row_k2k_prune, row_k2k_result;

    // network
    string data;

    // real network cost on other server
    uint64_t read_cycle, write_cycle;

    CostBenchmk() {}
    CostBenchmk(ssid_t t1, ssid_t t2, ssid_t t3, ssid_t p1, ssid_t p2,
                ssid_t p3)
        : t1(t1), t2(t2), t3(t3), p1(p1), p2(p2), p3(p3) {}
    CostBenchmk(ssid_t stype, ssid_t subject, ssid_t p, ssid_t object,
                ssid_t otype)
        : stype(stype), subject(subject), p(p), object(object), otype(otype) {}
    CostBenchmk(string &data) : data(data) {}

    void generate_l2u_query(SPARQLQuery &q) {
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(subject, p, OUT, -1));
        q.result.nvars = 1;
        q.result.required_vars.push_back(-1);
    }

    void generate_ntwk_query(SPARQLQuery &q){
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(stype, TYPE_ID, IN, -1));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-1, p, OUT, -2));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-2, TYPE_ID, OUT, -3));
        q.result.nvars = 3;
        q.result.required_vars.push_back(-1);
        q.result.required_vars.push_back(-2);
        q.result.required_vars.push_back(-3);
    }

    void generate_k2u_query(SPARQLQuery &q) {
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(stype, TYPE_ID, IN, -1));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-1, p, OUT, -2));
        q.result.nvars = 2;
        q.result.required_vars.push_back(-1);
    }

    void generate_k2l_query(SPARQLQuery &q) {
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(stype, TYPE_ID, IN, -1));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-1, p, OUT, object));
        q.result.nvars = 1;
        q.result.required_vars.push_back(-1);
    }

    void generate_k2k_query(SPARQLQuery &q) {
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(t1, TYPE_ID, IN, -1));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-1, p1, OUT, -2));
        q.pattern_group.patterns.push_back(
            SPARQLQuery::Pattern(-2, TYPE_ID, OUT, t2));
        if (type == cost_model_t::CLOCKWISE) {
            q.pattern_group.patterns.push_back(
                SPARQLQuery::Pattern(-2, p2, OUT, -3));
        } else {
            q.pattern_group.patterns.push_back(
                SPARQLQuery::Pattern(-2, p2, IN, -3));
        }

        q.result.nvars = 4;
        q.result.required_vars.push_back(-1);
        q.result.required_vars.push_back(-2);
        q.result.required_vars.push_back(-3);
    }
};