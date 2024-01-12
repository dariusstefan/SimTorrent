#include "tema3.hpp"

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

void download_thread_func(const ThreadArgs &args) {
}

void upload_thread_func(const ThreadArgs &args) {
}

void tracker(int numtasks, int rank) {
    unordered_map<string, vector<string>> files;
    unordered_map<string, unordered_map<string, vector<int>>> swarm;
}

void peer(int numtasks, int rank) {
    ifstream fin("in" + to_string(rank) + ".txt");
    
    unordered_map<string, int> owned_files;
    unordered_map<string, vector<string>> owned_chunks;

    int num_files;
    fin >> num_files;

    for (int i = 0; i < num_files; ++i) {
        string filename;
        fin >> filename;
        int num_chunks;
        fin >> num_chunks;
        owned_files[filename] = num_chunks;
        owned_chunks[filename] = vector<string>(num_chunks);

        for (int j = 0; j < num_chunks; ++j) {
            string chunk_hash;
            fin >> chunk_hash;
            owned_chunks[filename][j] = chunk_hash;
        }
    }

    unordered_map<string, int> wanted_files;
    unordered_map<string, vector<string>> wanted_chunks;
    
    int num_wanted_files;
    fin >> num_wanted_files;

    for (int i = 0; i < num_wanted_files; ++i) {
        string filename;
        fin >> filename;
        wanted_files[filename] = -1;
        wanted_chunks[filename] = vector<string>();
    }

    fin.close();

    ofstream fout("client" + to_string(rank) + ".txt");
    fout << "Peer " << rank << ":\n";
    fout << "Owned files:\n";

    for (auto &file : owned_files) {
        fout << file.first << " " << file.second << "\n";
    }

    fout << "Owned chunks:\n";

    for (auto &file : owned_chunks) {
        fout << file.first << "\n";

        for (auto &chunk : file.second) {
            fout << chunk << "\n";
        }

        fout << "\n";
    }

    fout << "Wanted files:\n";

    for (auto &file : wanted_files) {
        fout << file.first << "\n";

        for (auto &chunk : wanted_chunks[file.first]) {
            fout << chunk << "\n";
        }

        fout << "\n";
    }

    fout.close();

    unordered_map<string, set<int>> chunk_peers;

    for (auto &file : wanted_files) {
        for (auto &chunk : wanted_chunks[file.first]) {
            chunk_peers[chunk] = {};
        }
    }

    ThreadArgs args;
    args.rank = rank;
    args.owned_files = &owned_files;
    args.owned_chunks = &owned_chunks;
    args.wanted_files = &wanted_files;
    args.wanted_chunks = &wanted_chunks;
    args.chunk_peers = &chunk_peers;

    thread download_thread(download_thread_func, args);
    thread upload_thread(upload_thread_func, args);

    download_thread.join();
    upload_thread.join();
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
 
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }

    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
