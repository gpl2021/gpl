# Gpl Tutorials

## Table of Contents

* [Configuring and running Gpl](#config)
* [Run SPARQL queries on Gpl](#run)
* [Useful commands](#command)
* [Trinity.RDF](#trinity)

<a  name="config"></a>

## Configuring and running Gpl

1) Edit the `config` in `$WUKONG_ROOT/scripts`.

```bash
$cd $WUKONG_ROOT/scripts
$vim config
```

The configuration items related to planner support are:

* `global_enable_planner`: enable generating a plan for each query
* `global_generate_statistics`: generate statistics which planner need by traversing whole dataset (suggested for the first time) / get statistics from file (significantly decrease setup time)
* `global_enable_budget`: enable budget strategy to limit the time for generating the plan.

2) Sync Wukong files to machines listed in `mpd.hosts`.

```bash
$cd ${WUKONG_ROOT}/scripts
$./sync.sh
sending incremental file list
...
```

3) Launch Wukong server on your cluster.

```bash
$cd ${WUKONG_ROOT}/scripts
$./run.sh 2
...
INFO:     Network latency: 2^0  ... 2^20  ...
INFO:     cost model factor: l2u ...
INFO:     cost model factor: k2u ...
INFO:     cost model factor: k2l ...
INFO:     cost model factor: k2k ...
Input 'help' command to get more information
wukong>
```


<a name="run"></a>

## Run SPARQL queries on Gpl

Gpl adds a new argument `-p` to the `sparql` command, which tells the console to use centain plan. If you need Gpl to generate plan, just tell the query without `-p`.

1) Gpl commands.

```bash
wukong> help
These are common Wukong commands: :
...

sparql <args>       run SPARQL queries in single or batch mode:
  -f <fname>             run a [single] SPARQL query from <fname>
  -m <factor> (=1)       set multi-threading <factor> for heavy query
                         processing
  -n <num> (=1)          repeat query processing <num> times
  -p <fname>             adopt user-defined query plan from <fname> for running
                         a single query
  -N <num> (=1)          do query optimization <num> times
  -v <lines> (=0)        print at most <lines> of results
  -o <fname>             output results into <fname>
  -g                     leverage GPU to accelerate heavy query processing
  -b <fname>             run a [batch] of SPARQL queries configured by <fname>
  -h [ --help ]          help message about sparql
  ...
```

2) run a single SPARQL query.

There are query examples in `$WUKONG_ROOT/scripts/sparql_query`. For example, enter `sparql -f sparql_query/lubm/basic/lubm_q1` to run the query `lubm_q1`.
Please note that if you didn't enable the planner, you should specify a query plan manually like `sparql -f sparql_query/lubm/basic/lubm_q1 -p optimal10240_plan`.


```bash
wukong> sparql -N 100 -n 10 -m 10 -f sparql_query/lubm/basic/lubm_q1
INFO:     Parsing a SPARQL query is done.
INFO:     Parsing time: 78 usec
INFO:     Optimization time: 499 usec
INFO:     estimate min cost 3406.5 usec.
INFO:     patterns[6]:
INFO:       31  1 0 -3
INFO:       -3  28  1 -2
INFO:       -2  13  0 -1
INFO:       -1  1 1 23
INFO:       -1  2 1 -3
INFO:       -2  1 1 9
INFO:     unions[0]:
INFO:     optionals[0]:
INFO:     filters[0]:
INFO:       type  valueArg  value
INFO:     real network read cost: 783.353 write cost: 209.961 all cost: 993.315 usec / per server
INFO:     (last) result size: 106
INFO:     (average) latency: 2077 usec


wukong> sparql -n 10 -m 10 -p optimal2560_plan -f sparql_query/lubm/basic/lubm_q1
INFO:     Parsing a SPARQL query is done.
INFO:     Parsing time: 71 usec
INFO:     User-defined query plan is enabled
INFO:     patterns[7]:
INFO:       13  0 1 -2
INFO:       -2  28  0 -3
INFO:       -3  2 0 -1
INFO:       -1  13  1 -2
INFO:       -2  1 1 9
INFO:       -1  1 1 23
INFO:       -3  1 1 31
INFO:     unions[0]:
INFO:     optionals[0]:
INFO:     filters[0]:
INFO:       type  valueArg  value
INFO:     (last) result size: 106
INFO:     (average) latency: 10591 usec
```

<a name="command"></a>

##Useful commands

- Modify configuration while running gpl.
```
wukong> config -s global_enable_budget=0
```

- Change log mode to DEBUG to browse more information.
```
wukong> logger -s 1
```

- After generating statistics by traversing dataset, you can store useful data to file and decrease setup time.
```
wukong> store-stat -f [file_path]
```
In `core/wukong.cpp(196): string fname = Global::input_folder + "/statfile";`, you can change input statistics file path loading in setup phase.

- Load useful data from file.
```
wukong> load-stat -f [file_path]
```

<a name="trinity"></a>

## Trinity.RDF
Trinity.RDF is an exploration-based RDF store with an initial query optimizer for graph exploration. We re-implemented it on Wukong since the source code is not available.

The source code is on the branch `trinity`. Main difference is in the folder `core/optimizer`. The commands used on it are same as that on Gpl.



