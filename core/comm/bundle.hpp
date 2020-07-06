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
#include <boost/serialization/set.hpp>
#include <boost/serialization/split_free.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/vector.hpp>

#include <query.hpp>
#include <optimizer/benchmark.hpp>

using namespace std;
using namespace boost::archive;

enum req_type {
    SPARQL_QUERY = 0,
    DYNAMIC_LOAD = 1,
    GSTORE_CHECK = 2,
    SPARQL_HISTORY = 3,
    COST_BENCHMK = 4
};

namespace boost {
namespace serialization {
char occupied = 0;
char empty = 1;

template <class Archive>
void save(Archive &ar, const SPARQLQuery::Pattern &t, unsigned int version) {
    ar << t.subject;
    ar << t.predicate;
    ar << t.object;
    ar << t.direction;
    ar << t.pred_type;
}

template <class Archive>
void load(Archive &ar, SPARQLQuery::Pattern &t, unsigned int version) {
    ar >> t.subject;
    ar >> t.predicate;
    ar >> t.object;
    ar >> t.direction;
    ar >> t.pred_type;
}

template <class Archive>
void save(Archive &ar, const SPARQLQuery::PatternGroup &t,
          unsigned int version) {
    ar << t.patterns;
    ar << t.optional_new_vars;  // it should not be put into the "if
                                // (t.optional.size() > 0)" block. The PG itself
                                // is from optional
    if (t.filters.size() > 0) {
        ar << occupied;
        ar << t.filters;
    } else
        ar << empty;

    if (t.optional.size() > 0) {
        ar << occupied;
        ar << t.optional;
    } else
        ar << empty;

    if (t.unions.size() > 0) {
        ar << occupied;
        ar << t.unions;
    } else
        ar << empty;
}

template <class Archive>
void load(Archive &ar, SPARQLQuery::PatternGroup &t, unsigned int version) {
    char temp = 2;
    ar >> t.patterns;
    ar >> t.optional_new_vars;
    ar >> temp;
    if (temp == occupied) {
        ar >> t.filters;
        temp = 2;
    }
    ar >> temp;
    if (temp == occupied) {
        ar >> t.optional;
        temp = 2;
    }
    ar >> temp;
    if (temp == occupied) {
        ar >> t.unions;
        temp = 2;
    }
}

template <class Archive>
void save(Archive &ar, const SPARQLQuery::Result &t, unsigned int version) {
    ar << t.col_num;
    ar << t.row_num;
    ar << t.attr_col_num;
    ar << t.status_code;
    ar << t.blind;
    ar << t.nvars;
    ar << t.required_vars;
    ar << t.v2c_map;
    ar << t.optional_matched_rows;
    if (t.row_num > 0) {
        ar << occupied;
        ar << t.result_table;
        ar << t.attr_res_table;
    } else {
        ar << empty;
    }
#ifdef USE_GPU
    ar << t.gpu;
#endif
}

template <class Archive>
void load(Archive &ar, SPARQLQuery::Result &t, unsigned int version) {
    char temp = 2;
    ar >> t.col_num;
    ar >> t.row_num;
    ar >> t.attr_col_num;
    ar >> t.status_code;
    ar >> t.blind;
    ar >> t.nvars;
    ar >> t.required_vars;
    ar >> t.v2c_map;
    ar >> t.optional_matched_rows;
    ar >> temp;
    if (temp == occupied) {
        ar >> t.result_table;
        ar >> t.attr_res_table;
    }
#ifdef USE_GPU
    ar >> t.gpu;
#endif
}


template <class Archive>
void save(Archive &ar, const SPARQLQuery &t, unsigned int version) {
    ar << t.qid;
    ar << t.pqid;
    ar << t.pg_type;
    ar << t.state;
    ar << t.dev_type;
    ar << t.job_type;
    ar << t.priority;
    ar << t.mt_factor;
    ar << t.mt_tid;
    ar << t.pattern_step;
    ar << t.local_var;
    ar << t.corun_enabled;
    ar << t.corun_step;
    ar << t.fetch_step;
    ar << t.union_done;
    ar << t.optional_step;
    ar << t.limit;
    ar << t.offset;
    ar << t.distinct;
    ar << t.pattern_group;
    if (t.orders.size() > 0) {
        ar << occupied;
        ar << t.orders;
    } else {
        ar << empty;
    }
    ar << t.result;
}

template <class Archive>
void load(Archive &ar, SPARQLQuery &t, unsigned int version) {
    char temp = 2;
    ar >> t.qid;
    ar >> t.pqid;
    ar >> t.pg_type;
    ar >> t.state;
    ar >> t.dev_type;
    ar >> t.job_type;
    ar >> t.priority;
    ar >> t.mt_factor;
    ar >> t.mt_tid;
    ar >> t.pattern_step;
    ar >> t.local_var;
    ar >> t.corun_enabled;
    ar >> t.corun_step;
    ar >> t.fetch_step;
    ar >> t.union_done;
    ar >> t.optional_step;
    ar >> t.limit;
    ar >> t.offset;
    ar >> t.distinct;
    ar >> t.pattern_group;
    ar >> temp;
    if (temp == occupied) ar >> t.orders;
    ar >> t.result;
}

}  // namespace serialization
}  // namespace boost

/**
 * GStore consistency checker
 */
class GStoreCheck {
   private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &qid;
        ar &pqid;
        ar &check_ret;
        ar &index_check;
        ar &normal_check;
    }

   public:
    int qid = -1;   // unused
    int pqid = -1;  // parent qid

    int check_ret = 0;
    bool index_check = false;
    bool normal_check = false;

    GStoreCheck() {}

    GStoreCheck(bool i, bool n) : index_check(i), normal_check(n) {}
};

/**
 * RDF data loader
 */
class RDFLoad {
   private:
    friend class boost::serialization::access;

    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &qid;
        ar &pqid;
        ar &load_dname;
        ar &load_ret;
        ar &check_dup;
    }

   public:
    int qid = -1;   // unused
    int pqid = -1;  // parent query id

    string load_dname = "";  // the file name used to be inserted
    int load_ret = 0;
    bool check_dup = false;

    RDFLoad() {}

    RDFLoad(string s, bool b) : load_dname(s), check_dup(b) {}
};

/**
 * Bundle to be sent by network, with data type labeled
 * Note this class does not use boost serialization
 */
class Bundle {
   private:
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &type;
        ar &data;
    }

   public:
    req_type type;
    string data;

    Bundle() {}

    Bundle(const req_type &t, const string &d) : type(t), data(d) {}

    Bundle(const Bundle &b) : type(b.type), data(b.data) {}

    Bundle(const SPARQLQuery &r) : type(SPARQL_QUERY) {
        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);

        oa << r;
        data = ss.str();
    }

    Bundle(const RDFLoad &r) : type(DYNAMIC_LOAD) {
        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);

        oa << r;
        data = ss.str();
    }

    Bundle(const GStoreCheck &r) : type(GSTORE_CHECK) {
        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);

        oa << r;
        data = ss.str();
    }

    Bundle(const CostBenchmk &c) : type(COST_BENCHMK) {
        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);

        oa << c;
        data = ss.str();
    }

    Bundle(const string str) { init(str); }

    void init(const string str) {
        memcpy(&type, str.c_str(), sizeof(req_type));
        string d(str, sizeof(req_type), str.length() - sizeof(req_type));
        data = d;
    }

    // SPARQLQuery command
    SPARQLQuery get_sparql_query() const {
        ASSERT(type == SPARQL_QUERY);

        std::stringstream ss;
        ss << data;

        boost::archive::binary_iarchive ia(ss);
        SPARQLQuery result;
        ia >> result;
        return result;
    }

    // RDFLoad command
    RDFLoad get_rdf_load() const {
        ASSERT(type == DYNAMIC_LOAD);

        std::stringstream ss;
        ss << data;

        boost::archive::binary_iarchive ia(ss);
        RDFLoad result;
        ia >> result;
        return result;
    }

    // GStoreCheck command
    GStoreCheck get_gstore_check() const {
        ASSERT(type == GSTORE_CHECK);

        std::stringstream ss;
        ss << data;

        boost::archive::binary_iarchive ia(ss);
        GStoreCheck result;
        ia >> result;
        return result;
    }

    // GStoreCheck command
    CostBenchmk get_cost_benchmk() const {
        ASSERT(type == COST_BENCHMK);

        std::stringstream ss;
        ss << data;

        boost::archive::binary_iarchive ia(ss);
        CostBenchmk result;
        ia >> result;
        return result;
    }

    string to_str() const {
#if 1  // FIXME
        char *c_str = new char[sizeof(req_type) + data.length()];
        memcpy(c_str, &type, sizeof(req_type));
        memcpy(c_str + sizeof(req_type), data.c_str(), data.length());
        string str(c_str, sizeof(req_type) + data.length());
        delete[] c_str;
        return str;
#else
        // FIXME: why not work? (Rong)
        return string(std::to_string((uint64_t)type) + data);
#endif
    }
};

BOOST_SERIALIZATION_SPLIT_FREE(SPARQLQuery::Pattern);
BOOST_SERIALIZATION_SPLIT_FREE(SPARQLQuery::PatternGroup);
BOOST_SERIALIZATION_SPLIT_FREE(SPARQLQuery::Result);
BOOST_SERIALIZATION_SPLIT_FREE(SPARQLQuery);

// remove class information at the cost of losing auto versioning,
// which is useless currently because wukong use boost serialization to transmit
// data between endpoints running the same code.
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery::Pattern,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery::PatternGroup,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery::Filter,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery::Order,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery::Result,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(SPARQLQuery,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(GStoreCheck,
                           boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(RDFLoad, boost::serialization::object_serializable);
BOOST_CLASS_IMPLEMENTATION(CostBenchmk,
                           boost::serialization::object_serializable);

// remove object tracking information at the cost of that multiple identical
// objects may be created when an archive is loaded. current query data
// structure does not contain two identical object reference with the same
// pointer
BOOST_CLASS_TRACKING(SPARQLQuery::Pattern, boost::serialization::track_never);
BOOST_CLASS_TRACKING(SPARQLQuery::Filter, boost::serialization::track_never);
BOOST_CLASS_TRACKING(SPARQLQuery::PatternGroup,
                     boost::serialization::track_never);
BOOST_CLASS_TRACKING(SPARQLQuery::Order, boost::serialization::track_never);
BOOST_CLASS_TRACKING(SPARQLQuery::Result, boost::serialization::track_never);
BOOST_CLASS_TRACKING(SPARQLQuery, boost::serialization::track_never);
BOOST_CLASS_TRACKING(GStoreCheck, boost::serialization::track_never);
BOOST_CLASS_TRACKING(RDFLoad, boost::serialization::track_never);
BOOST_CLASS_TRACKING(CostBenchmk, boost::serialization::track_never);
