#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/functional/hash.hpp>
#include <boost/mpi.hpp>
#include <boost/serialization/unordered_map.hpp>

#include "comm/tcp_adaptor.hpp"
#include "global.hpp"

using namespace std;

struct four_num {
    int out_out;
    int out_in;
    int in_in;
    int in_out;
    four_num() : out_out(0), out_in(0), in_in(0), in_out(0) {}
    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &out_out;
        ar &out_in;
        ar &in_in;
        ar &in_out;
    }
};

struct direct_p {
    ssid_t dir;
    ssid_t p;
    direct_p() : dir(-1), p(-1) {}
    direct_p(ssid_t x, ssid_t y) : dir(x), p(y) {}
};

class Stats {
   public:
    unordered_map<ssid_t, int> predicate_to_triple;
    unordered_map<ssid_t, int> predicate_to_subject;
    unordered_map<ssid_t, int> predicate_to_object;
    unordered_map<ssid_t, int> type_to_subject;
    unordered_map<pair<ssid_t, ssid_t>, four_num, boost::hash<pair<int, int>>>
        correlation;
    unordered_map<ssid_t, vector<direct_p>> id_to_predicate;

    unordered_map<ssid_t, int> global_ptcount;
    unordered_map<ssid_t, int> global_pscount;
    unordered_map<ssid_t, int> global_pocount;
    unordered_map<ssid_t, int> global_tyscount;
    unordered_map<pair<ssid_t, ssid_t>, four_num, boost::hash<pair<int, int>>>
        global_ppcount;
    int sid;

    Stats() {}

    Stats(int sid) : sid(sid) {}

    void gather_stat(TCP_Adaptor *tcp_adaptor) {
        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);
        oa << (*this);
        tcp_adaptor->send(0, 0, ss.str());

        if (sid == 0) {
            vector<Stats> all_gather;
            for (int i = 0; i < Global::num_servers; i++) {
                std::string str;
                str = tcp_adaptor->recv(0);
                Stats tmp_data;
                std::stringstream s;
                s << str;
                boost::archive::binary_iarchive ia(s);
                ia >> tmp_data;
                all_gather.push_back(tmp_data);
            }

            for (int i = 0; i < all_gather.size(); i++) {
                for (unordered_map<ssid_t, int>::iterator it =
                         all_gather[i].predicate_to_triple.begin();
                     it != all_gather[i].predicate_to_triple.end(); it++) {
                    ssid_t key = it->first;
                    int triple = it->second;
                    if (global_ptcount.find(key) == global_ptcount.end()) {
                        global_ptcount[key] = triple;
                    } else {
                        global_ptcount[key] += triple;
                    }
                }

                for (unordered_map<ssid_t, int>::iterator it =
                         all_gather[i].predicate_to_subject.begin();
                     it != all_gather[i].predicate_to_subject.end(); it++) {
                    ssid_t key = it->first;
                    int subject = it->second;
                    if (global_pscount.find(key) == global_pscount.end()) {
                        global_pscount[key] = subject;
                    } else {
                        global_pscount[key] += subject;
                    }
                }

                for (unordered_map<ssid_t, int>::iterator it =
                         all_gather[i].predicate_to_object.begin();
                     it != all_gather[i].predicate_to_object.end(); it++) {
                    ssid_t key = it->first;
                    int object = it->second;
                    if (global_pocount.find(key) == global_pocount.end()) {
                        global_pocount[key] = object;
                    } else {
                        global_pocount[key] += object;
                    }
                }

                for (unordered_map<pair<ssid_t, ssid_t>, four_num,
                                   boost::hash<pair<int, int>>>::iterator it =
                         all_gather[i].correlation.begin();
                     it != all_gather[i].correlation.end(); it++) {
                    pair<ssid_t, ssid_t> key = it->first;
                    four_num value = it->second;
                    if (global_ppcount.find(it->first) ==
                        global_ppcount.end()) {
                        global_ppcount[key] = value;
                    } else {
                        global_ppcount[key].out_out += value.out_out;
                        global_ppcount[key].out_in += value.out_in;
                        global_ppcount[key].in_in += value.in_in;
                        global_ppcount[key].in_out += value.in_out;
                    }
                }

                // for type predicate
                for (unordered_map<ssid_t, int>::iterator it =
                         all_gather[i].type_to_subject.begin();
                     it != all_gather[i].type_to_subject.end(); it++) {
                    ssid_t key = it->first;
                    int subject = it->second;
                    if (global_tyscount.find(key) == global_tyscount.end()) {
                        global_tyscount[key] = subject;
                    } else {
                        global_tyscount[key] += subject;
                    }
                }
            }

            logstream(LOG_INFO)
                << "global_ptcount size: " << global_ptcount.size() << LOG_endl;
            logstream(LOG_INFO)
                << "global_pscount size: " << global_pscount.size() << LOG_endl;
            logstream(LOG_INFO)
                << "global_pocount size: " << global_pocount.size() << LOG_endl;
            logstream(LOG_INFO)
                << "global_ppcount size: " << global_ppcount.size() << LOG_endl;
            logstream(LOG_INFO)
                << "global_tyscount size: " << global_tyscount.size()
                << LOG_endl;

            // for type predicate
            global_pocount[1] = global_tyscount.size();
            int triple = 0;
            for (unordered_map<ssid_t, int>::iterator it =
                     global_tyscount.begin();
                 it != global_tyscount.end(); it++) {
                triple += it->second;
            }
            global_ptcount[1] = triple;
        }

        send_stat_to_all_machines(tcp_adaptor);

        logstream(LOG_INFO)
            << "#" << sid << ": load stats of DGraph is finished." << LOG_endl;
    }

    // after the master server get whole statistics, this method is used to send
    // it to all machines.
    void send_stat_to_all_machines(TCP_Adaptor *tcp_adaptor) {
        if (sid == 0) {
            // master server sends statistics
            std::stringstream ss;
            boost::archive::binary_oarchive my_oa(ss);
            my_oa << global_ptcount << global_pscount << global_pocount
                  << global_ppcount << global_tyscount;

            for (int i = 1; i < Global::num_servers; i++)
                tcp_adaptor->send(i, 0, ss.str());
        } else {
            // every slave server recieves statistics
            std::string str;
            str = tcp_adaptor->recv(0);
            std::stringstream ss;
            ss << str;
            boost::archive::binary_iarchive ia(ss);
            ia >> global_ptcount >> global_pscount >> global_pocount >>
                global_ppcount >> global_tyscount;
        }
    }

    void load_stat_from_file(string fname, TCP_Adaptor *tcp_ad) {
        uint64_t t1 = timer::get_usec();

        // master server loads statistics and dispatchs them to all slave
        // servers
        if (sid == 0) {
            ifstream file(fname.c_str());
            if (!file.good()) {
                logstream(LOG_WARNING)
                    << "statistics file " << fname
                    << " does not exsit, pleanse check the fname"
                    << " and use load-stat to mannually set it" << LOG_endl;

                /// FIXME: HANG bug if master return here
                return;
            }

            ifstream ifs(fname);
            boost::archive::binary_iarchive ia(ifs);
            ia >> global_ptcount;
            ia >> global_pscount;
            ia >> global_pocount;
            ia >> global_tyscount;
            ia >> global_ppcount;
            ifs.close();
        }

        send_stat_to_all_machines(tcp_ad);

        uint64_t t2 = timer::get_usec();
        logstream(LOG_INFO) << (t2 - t1) / 1000 << " ms for loading statistics"
                            << " at server " << sid << LOG_endl;
    }

    void store_stat_to_file(string fname) {
        // data only cached on master server
        if (sid != 0) return;

        // avoid saving when it already exsits
        ifstream file(fname.c_str());
        if (!file.good()) {
            ofstream ofs(fname);
            boost::archive::binary_oarchive oa(ofs);
            oa << global_ptcount;
            oa << global_pscount;
            oa << global_pocount;
            oa << global_tyscount;
            oa << global_ppcount;
            ofs.close();

            logstream(LOG_INFO) << "store statistics to file " << fname
                                << " is finished." << LOG_endl;
        }
    }

    // prepare data for planner
    void generate_statistics(GStore *gstore) {
        vertex_t *vertices = gstore->vertices;
        edge_t *edges = gstore->edges;

        for (uint64_t bucket_id = 0;
             bucket_id < gstore->num_buckets + gstore->num_buckets_ext;
             bucket_id++) {
            uint64_t slot_id = bucket_id * gstore->ASSOCIATIVITY;
            for (int i = 0; i < gstore->ASSOCIATIVITY - 1; i++, slot_id++) {
                // skip empty slot
                if (vertices[slot_id].key.is_empty()) continue;

                sid_t vid = vertices[slot_id].key.vid;
                sid_t pid = vertices[slot_id].key.pid;

                uint64_t off = vertices[slot_id].ptr.off;
                if (pid == PREDICATE_ID) continue;  // skip for index vertex

                unordered_map<ssid_t, int> &ptcount = predicate_to_triple;
                unordered_map<ssid_t, int> &pscount = predicate_to_subject;
                unordered_map<ssid_t, int> &pocount = predicate_to_object;
                unordered_map<ssid_t, int> &tyscount = type_to_subject;
                unordered_map<ssid_t, vector<direct_p>> &ipcount =
                    id_to_predicate;

                if (vertices[slot_id].key.dir == IN) {
                    uint64_t sz = vertices[slot_id].ptr.size;

                    // triples only count from one direction
                    if (ptcount.find(pid) == ptcount.end())
                        ptcount[pid] = sz;
                    else
                        ptcount[pid] += sz;

                    // count objects
                    if (pocount.find(pid) == pocount.end())
                        pocount[pid] = 1;
                    else
                        pocount[pid]++;

                    // count in predicates for specific id
                    ipcount[vid].push_back(direct_p(IN, pid));
                } else {
                    // count subjects
                    if (pscount.find(pid) == pscount.end())
                        pscount[pid] = 1;
                    else
                        pscount[pid]++;

                    // count out predicates for specific id
                    ipcount[vid].push_back(direct_p(OUT, pid));

                    // count type predicate
                    if (pid == TYPE_ID) {
                        uint64_t sz = vertices[slot_id].ptr.size;
                        uint64_t off = vertices[slot_id].ptr.off;

                        for (uint64_t j = 0; j < sz; j++) {
                            // src may belongs to multiple types
                            sid_t obid = edges[off + j].val;

                            if (tyscount.find(obid) == tyscount.end())
                                tyscount[obid] = 1;
                            else
                                tyscount[obid]++;

                            if (pscount.find(obid) == pscount.end())
                                pscount[obid] = 1;
                            else
                                pscount[obid]++;

                            ipcount[vid].push_back(direct_p(OUT, obid));
                        }
                    }
                }
            }
        }
        unordered_map<pair<ssid_t, ssid_t>, four_num,
                      boost::hash<pair<int, int>>> &ppcount = correlation;

        // do statistic for correlation
        for (unordered_map<ssid_t, vector<direct_p>>::iterator it =
                 id_to_predicate.begin();
             it != id_to_predicate.end(); it++) {
            ssid_t vid = it->first;
            vector<direct_p> &vec = it->second;

            for (uint64_t i = 0; i < vec.size(); i++) {
                for (uint64_t j = i + 1; j < vec.size(); j++) {
                    ssid_t p1, d1, p2, d2;
                    if (vec[i].p < vec[j].p) {
                        p1 = vec[i].p;
                        d1 = vec[i].dir;
                        p2 = vec[j].p;
                        d2 = vec[j].dir;
                    } else {
                        p1 = vec[j].p;
                        d1 = vec[j].dir;
                        p2 = vec[i].p;
                        d2 = vec[i].dir;
                    }

                    if (d1 == OUT && d2 == OUT)
                        ppcount[make_pair(p1, p2)].out_out++;

                    if (d1 == OUT && d2 == IN)
                        ppcount[make_pair(p1, p2)].out_in++;

                    if (d1 == IN && d2 == IN)
                        ppcount[make_pair(p1, p2)].in_in++;

                    if (d1 == IN && d2 == OUT)
                        ppcount[make_pair(p1, p2)].in_out++;
                }
            }
        }
        // cout << "sizeof correlation = " << stat.correlation.size() << endl;
        logstream(LOG_INFO)
            << "#" << sid << ": generating stats is finished." << LOG_endl;
    }

    template <typename Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar &predicate_to_triple;
        ar &predicate_to_subject;
        ar &predicate_to_object;
        ar &type_to_subject;
        ar &correlation;
    }
};
