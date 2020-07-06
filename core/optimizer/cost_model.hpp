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
#include <Eigen/Dense>
#include <vector>

#include "logger2.hpp"
#include "common.hpp"

#define L2U_FACTOR_NUM 2
#define K2U_FACTOR_NUM 4
#define K2L_FACTOR_NUM 5
#define K2K_FACTOR_NUM 5

using namespace Eigen;
using namespace std;

class CostModel{
    private:
     MatrixXd matrix_l2u;
     VectorXd vector_l2u;
     double b_l2u = 0.046;  // explore, const
     double d_l2u;

     MatrixXd matrix_k2u;
     VectorXd vector_k2u;
     double a1_k2u = 0.07;
     double a2_k2u = 0.09;
     double b_k2u = 0.01;
     double d_k2u = 0;  // init-prune, prune, explore, const

     MatrixXd matrix_k2l;
     VectorXd vector_k2l;
     double a1_k2l = 0.013;
     double a2_k2l = 0.015;
     double b_k2l = 0.012;
     double c_k2l = 0;
     double d_k2l = 0;  // init-prune, prune, explore, match, const

     MatrixXd matrix_k2k;
     VectorXd vector_k2k;
     double a1_k2k = 0.44;
     double a2_k2k = 0.01;
     double b_k2k = -0.022;
     double c_k2k = 0;
     double d_k2k = 0;  // init-prune, prune, explore, match, const

    // 2^0, 2^1 ...
     vector<double> network_ltcy;

     double get_read_cost(double init, double explore) {
         int index_size = 8 * sizeof(vertex_t);
         int edge_size = (explore/init + 1) * sizeof(edge_t);
         
         double index_cost = network_ltcy[log(index_size) / log(2)];
         double edge_cost = network_ltcy[log(edge_size) / log(2)];

         return init*(index_cost+edge_cost);
     }

     double get_write_cost(double result_size) {
         int size = result_size*sizeof(sid_t)+128;
         int index = log(size) / log(2);
         index = index < (network_ltcy.size()-1) ? index : (network_ltcy.size() - 2);
         return network_ltcy[index] + ((network_ltcy[index+1] - network_ltcy[index])*(size/pow(2, index)-1));
     }

     double calculate_network_common(CostResult &result){
         if(Global::num_servers==1)
             return 0;

         int path_size = result.path.size();
         ASSERT(path_size >= 8);
         // next hop is local
         if(result.path[path_size-4] == result.path[path_size-8] && result.all_local){
             return 0;
         }
         // from index, next hop must be local
         if ((result.path[1] == TYPE_ID || result.path[1] == PREDICATE_ID)
                && (result.path[path_size - 4] == result.path[3])){
             result.all_local = true;
             return 0;
         }

         double cost = 0;
         result.all_local = false;

         // rdma read
         if(Global::use_rdma && result.init_bind < Global::rdma_threshold){
             double ratio = (Global::num_servers - 1)*1.0 / Global::num_servers;
             cost = get_read_cost(result.init_bind*ratio, result.explore_bind*ratio);
         }else{
             // rdma write | tcp
             cost = get_write_cost(result.init_bind * result.typetable.get_col_num() / Global::num_servers) +
                    get_write_cost(result.result_bind * result.typetable.get_col_num() / Global::num_servers);
             cost *= (Global::num_servers - 1);
         }
         return cost;
     }

    public:
     void print(){
        logstream(LOG_INFO) << "cost model factor: l2u " << b_l2u << "  " << d_l2u << " " << LOG_endl;
        logstream(LOG_INFO)
            << "cost model factor: k2u " << a1_k2u << "  " << a2_k2u << " "
            << b_k2u << "  " << d_k2u << " " << LOG_endl;
        logstream(LOG_INFO)
            << "cost model factor: k2l " << a1_k2l << "  " << a2_k2l << " "
            << b_k2l << "  " << c_k2l << "  " << d_k2l << " " << LOG_endl;
        logstream(LOG_INFO)
            << "cost model factor: k2k " << a1_k2k << "  " << a2_k2k << " "
            << b_k2k << "  " << c_k2k << "  " << d_k2k << " " << LOG_endl;
     }

     void add_network_ltcy(double latency){
         network_ltcy.push_back(latency);
     }

     void print_network(){
         logstream(LOG_INFO) << "Network latency: ";
         for(int i = 0; i < network_ltcy.size(); ++i){
             logstream(LOG_INFO)
                 << "2^" << i << "  " << network_ltcy[i] << ", ";
         }
         logstream(LOG_INFO) << LOG_endl;
     }

     void end_network_cost(CostResult &result, int mt_factor){
         if(result.path.empty())
            return;
         int real_result = result.result_bind;
         if (result.path[1] == TYPE_ID || result.path[1] == PREDICATE_ID) {
             result.network_cost *= mt_factor;
             real_result *= (mt_factor*Global::num_servers);
         }
         result.network_cost += get_write_cost(real_result*result.typetable.get_col_num());
         return;
     }

     void calculate(CostResult &result){
        if(result.current_model != model_t::L2U && result.init_bind == 0){
            result.add_cost = 0;
            return;
        }
        switch (result.current_model)
         {
         case model_t::I2U:{
             result.network_cost = network_ltcy[8];
             result.add_cost = result.explore_bind * b_l2u + d_l2u;
             break;
         }
         case model_t::L2U:{
             result.network_cost = 0;
             result.add_cost = result.explore_bind * b_l2u + d_l2u;
             break;
        }
         case model_t::K2U:{
             result.network_cost += calculate_network_common(result);
             result.add_cost = (result.init_bind-result.prune_bind) * a1_k2u + result.prune_bind * a2_k2u + 
                        result.explore_bind * b_k2u + d_k2u;
             break;
         }
         case model_t::K2L:{
             result.network_cost += calculate_network_common(result);
             result.add_cost = (result.init_bind - result.prune_bind) * a1_k2l +
                               result.prune_bind * a2_k2l + result.explore_bind * b_k2l + d_k2l;
         }   
             break;
         case model_t::K2K:{
             result.network_cost += calculate_network_common(result);
             result.add_cost = (result.init_bind - result.prune_bind) * a1_k2k +
                               result.prune_bind * a2_k2k + result.explore_bind * b_k2k + d_k2k;
         }   
             break;
         default:
             break;
         }

         if(result.add_cost < 0)
            result.add_cost = 0;
     }

     void generate_factor(model_t model_type=model_t::ALL){
         VectorXd result;
         switch (model_type)
         {
         case model_t::ALL:
             result = matrix_l2u.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_l2u);
             b_l2u = abs(result(0)); d_l2u = result(1);
             result = matrix_k2u.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_k2u);
             a1_k2u = abs(result(0)); a2_k2u = abs(result(1)); b_k2u = abs(result(2));
             d_k2u = result(3);
             result = matrix_k2l.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_k2l);
             a1_k2l = abs(result(0)); a2_k2l = abs(result(1)); b_k2l = abs(result(2));
             c_k2l = abs(result(3)); d_k2l = result(4);
             break;
         case model_t::L2U:
             result = matrix_l2u.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_l2u);
             b_l2u = result(0); d_l2u = result(1);
             break;
         case model_t::K2U:
             result = matrix_k2u.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_k2u);
             a1_k2u = result(0); a2_k2u = result(1); b_k2u = result(2);d_k2u = result(3);
             break;
         case model_t::K2L:
             result = matrix_k2l.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_k2l);
             a1_k2l = result(0); a2_k2l = result(1); b_k2l = result(2);
             b_k2l = result(3); d_k2l = result(4);
             break;
         case model_t::K2K:
             result = matrix_k2k.jacobiSvd(ComputeThinU | ComputeThinV).solve(vector_k2k);
             a1_k2k = result(0); a2_k2k = result(1); b_k2k = result(2);
             c_k2k = result(3); d_k2k = result(4);
             break;
         default:
             break;
         }

         print();
     }

     void add_l2u_sample(int sample_index, int row_explore, int latency){
         matrix_l2u.row(sample_index) << row_explore , 1;
         vector_l2u.row(sample_index) << latency;
     }

     void add_k2u_sample(int sample_index, int row_init, int row_prune, int row_explore, int latency) {
         matrix_k2u.row(sample_index) << (row_init-row_prune), row_prune, row_explore, 1;
         vector_k2u.row(sample_index) << latency;
     }

     void add_k2l_sample(int sample_index, int row_init, int row_prune, int row_explore, 
                                                int row_match, int latency) {
         matrix_k2l.row(sample_index) << (row_init-row_prune), row_prune, row_explore, 0, 1;
         vector_k2l.row(sample_index) << latency;
     }

     void add_k2k_sample(int sample_index, int row_init, int row_prune, int row_explore, 
                                                int row_match, int latency) {
         matrix_k2k.row(sample_index) << (row_init-row_prune), row_prune, row_explore, 0, 1;
         vector_k2k.row(sample_index) << latency;
     }

     void resize(int sample_num, model_t model_type=model_t::K2K){
         switch (model_type)
         {
         case model_t::ALL:
             matrix_l2u = MatrixXd(sample_num, L2U_FACTOR_NUM);
             vector_l2u = VectorXd(sample_num);
             matrix_k2u = MatrixXd(sample_num, K2U_FACTOR_NUM);
             vector_k2u = VectorXd(sample_num);
             matrix_k2l = MatrixXd(sample_num, K2L_FACTOR_NUM);
             vector_k2l = VectorXd(sample_num);
             break;
         case model_t::L2U:
             matrix_l2u = MatrixXd(sample_num, L2U_FACTOR_NUM);
             vector_l2u = VectorXd(sample_num);
             break;
         case model_t::K2U:
             matrix_k2u = MatrixXd(sample_num, K2U_FACTOR_NUM);
             vector_k2u = VectorXd(sample_num);
             break;
         case model_t::K2L:
             matrix_k2l = MatrixXd(sample_num, K2L_FACTOR_NUM);
             vector_k2l = VectorXd(sample_num);
             break;
         case model_t::K2K:
             matrix_k2k.resize(sample_num, K2K_FACTOR_NUM);
             vector_k2k.resize(sample_num);
             break;
         default:
             break;
         }
     }

     void init(int sample_num, model_t model_type=model_t::ALL){
         switch (model_type)
         {
         case model_t::ALL:
             matrix_l2u = MatrixXd(sample_num, L2U_FACTOR_NUM);
             vector_l2u = VectorXd(sample_num);
             matrix_k2u = MatrixXd(sample_num, K2U_FACTOR_NUM);
             vector_k2u = VectorXd(sample_num);
             matrix_k2l = MatrixXd(sample_num, K2L_FACTOR_NUM);
             vector_k2l = VectorXd(sample_num);
             break;
         case model_t::L2U:
             matrix_l2u = MatrixXd(sample_num, L2U_FACTOR_NUM);
             vector_l2u = VectorXd(sample_num);
             break;
         case model_t::K2U:
             matrix_k2u = MatrixXd(sample_num, K2U_FACTOR_NUM);
             vector_k2u = VectorXd(sample_num);
             break;
         case model_t::K2L:
             matrix_k2l = MatrixXd(sample_num, K2L_FACTOR_NUM);
             vector_k2l = VectorXd(sample_num);
             break;
         case model_t::K2K:
             matrix_k2k = MatrixXd(sample_num, K2K_FACTOR_NUM);
             vector_k2k = VectorXd(sample_num);
             break;
         default:
             break;
         }
     }
};