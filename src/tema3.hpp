#include <mpi.h>
#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <set>
#include <vector>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

using namespace std;

struct ThreadArgs {
    int rank;
    unordered_map<string, int> *owned_files;
    unordered_map<string, vector<string>> *owned_chunks;
    unordered_map<string, int> *wanted_files;
    unordered_map<string, vector<string>> *wanted_chunks;
    unordered_map<string, set<int>> *chunk_peers;
};

void send_refresh_tracker(const ThreadArgs &args) {
    int num_files = args.owned_files->size();
    MPI_Send(&num_files, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
    
    for (auto file : *args.owned_files) {
        char filename[MAX_FILENAME];
        strcpy(filename, file.first.c_str());
        MPI_Send(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&file.second, 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        for (auto chunk : (*args.owned_chunks)[file.first]) {
            char chunk_hash[HASH_SIZE + 1];
            strcpy(chunk_hash, chunk.c_str());
            MPI_Send(chunk_hash, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        }
    }
}

struct Tracker {
    unordered_map<string, vector<string>> files;
    unordered_map<string, unordered_map<int, vector<string>>> swarm;

    void recv_refresh(int peer, bool init) {
        int num_files;
        MPI_Recv(&num_files, 1, MPI_INT, peer, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        for (int j = 0; j < num_files; j++) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, peer, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            string filename_str(filename);

            int num_chunks;
            MPI_Recv(&num_chunks, 1, MPI_INT, peer, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (int k = 0; k < num_chunks; ++k) {
                char chunk_hash[HASH_SIZE + 1];
                MPI_Recv(chunk_hash, HASH_SIZE + 1, MPI_CHAR, peer, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                string chunk_hash_str(chunk_hash);

                swarm[filename_str][peer].push_back(chunk_hash_str);

                if (!init)
                    continue;
                
                if (files.find(filename_str) != files.end()) {
                    if (files[filename_str].size() < num_chunks) {
                        files[filename_str].push_back(chunk_hash_str);
                    }
                } else {
                    files[filename_str].push_back(chunk_hash_str);
                }
            }
        }
    }
};