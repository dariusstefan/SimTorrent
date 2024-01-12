#include <mpi.h>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

using namespace std;

struct ThreadArgs {
    int rank;
    unordered_map<string, int> *owned_files;
    unordered_map<string, vector<string>> *owned_chunks;
    unordered_map<string, int> *wanted_files;
    unordered_map<string, vector<string>> *wanted_chunks;
    unordered_map<string, set<int>> *chunk_peers;
};