#include "tema3.hpp"

void download_thread_func(const ThreadArgs &args) {
    Peer peer_client = *args.peer;

    int max_chunks = 10;

    for (auto &file : peer_client.wanted_files) {
        char filename[MAX_FILENAME];
        strcpy(filename, file.first.c_str());
        bool complete = false;
        int current_chunk = 0;
        while (!complete) {
            if (file.second == -1) {  // request the list of chunks from tracker
                int req = 0;
                MPI_Send(&req, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
                MPI_Send(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
                MPI_Recv(&file.second, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                for (int i = 0; i < file.second; ++i) {
                    char chunk_hash[HASH_SIZE + 1];
                    MPI_Recv(chunk_hash, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    peer_client.wanted_chunks[file.first].push_back(string(chunk_hash));
                }
            } else {  // request the swarm from tracker
                int req = 1;
                MPI_Send(&req, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
                MPI_Send(filename, MAX_FILENAME, MPI_CHAR, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);

                int num_peers;
                MPI_Recv(&num_peers, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                for (int i = 0; i < num_peers; ++i) {
                    int peer_rank;
                    MPI_Recv(&peer_rank, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    int num_chunks;
                    MPI_Recv(&num_chunks, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    for (int j = 0; j < num_chunks; ++j) {
                        char chunk_hash[HASH_SIZE + 1];
                        MPI_Recv(chunk_hash, HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        peer_client.chunk_peers[string(chunk_hash)].insert(peer_rank);
                    }
                }

                // download max 10 chunks, then send refresh to tracker
                while (current_chunk < file.second && max_chunks) {
                    string chunk_hash = peer_client.wanted_chunks[file.first][current_chunk];
                    int num_peers = peer_client.chunk_peers[chunk_hash].size();

                    int chosen_peer_idx = rand() % num_peers;
                    set<int>::iterator it = peer_client.chunk_peers[chunk_hash].begin();
                    advance(it, chosen_peer_idx);

                    int peer_rank = *it;

                    char hash[HASH_SIZE + 1];
                    strcpy(hash, chunk_hash.c_str());
                    MPI_Send(hash, HASH_SIZE + 1, MPI_CHAR, peer_rank, UPLOAD_THREAD_TAG, MPI_COMM_WORLD);
                    
                    int ok;
                    MPI_Recv(&ok, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                    peer_client.owned_files[file.first] = current_chunk + 1;
                    peer_client.owned_chunks[file.first].push_back(chunk_hash);

                    current_chunk++;
                    max_chunks--;
                }

                if (current_chunk == file.second) {
                    complete = true;
                    ofstream fout("client" + to_string(peer_client.rank) + "_" + file.first);

                    for (auto chunk : peer_client.owned_chunks[file.first]) {
                        fout << chunk << '\n';
                    }

                    fout.close();
                } else {
                    max_chunks = 10;

                    int req = 3;
                    MPI_Send(&req, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
                    
                    peer_client.send_refresh();
                }
            }
        }
    }

    int req = 2;
    MPI_Send(&req, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
}

void upload_thread_func(const ThreadArgs &args) {
    while (true) {
        MPI_Status status;

        char chunk_hash[HASH_SIZE + 1];
        MPI_Recv(chunk_hash, HASH_SIZE + 1, MPI_CHAR, MPI_ANY_SOURCE, UPLOAD_THREAD_TAG, MPI_COMM_WORLD, &status);
        int peer_rank = status.MPI_SOURCE;

        string chunk_hash_str(chunk_hash);

        if (status.MPI_SOURCE == TRACKER_RANK) {
            break;
        }

        MPI_Send(&peer_rank, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
    }
}

void tracker(int numtasks, int rank) {
    Tracker tracker_service;

    for (int i = 1; i < numtasks; ++i) {
        tracker_service.recv_refresh(i, true);
    }

    for (int i = 1; i < numtasks; ++i) {
        int ack = 1;
        MPI_Send(&ack, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    int connected_peers = numtasks - 1;
    while (connected_peers) {
        int req;
        MPI_Status status;
        MPI_Recv(&req, 1, MPI_INT, MPI_ANY_SOURCE, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, &status);
        int peer_rank = status.MPI_SOURCE;

        // send chunks list to peer
        if (req == 0) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string filename_str(filename);

            int num_chunks = tracker_service.files[filename_str].size();
            MPI_Send(&num_chunks, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);

            for (auto chunk : tracker_service.files[filename_str]) {
                char chunk_hash[HASH_SIZE + 1];
                strcpy(chunk_hash, chunk.c_str());
                MPI_Send(chunk_hash, HASH_SIZE + 1, MPI_CHAR, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
            }
        }
        
        // send swarm list to peer
        if (req == 1) {
            char filename[MAX_FILENAME];
            MPI_Recv(filename, MAX_FILENAME, MPI_CHAR, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            string filename_str(filename);

            int num_peers = tracker_service.swarm[filename_str].size();
            MPI_Send(&num_peers, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);

            for (auto peer : tracker_service.swarm[filename_str]) {
                MPI_Send(&peer.first, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);

                int num_chunks = peer.second.size();
                MPI_Send(&num_chunks, 1, MPI_INT, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);

                for (auto chunk : peer.second) {
                    char chunk_hash[HASH_SIZE + 1];
                    strcpy(chunk_hash, chunk.c_str());
                    MPI_Send(chunk_hash, HASH_SIZE + 1, MPI_CHAR, peer_rank, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD);
                }
            }
        }

        if (req == 2) {
            connected_peers--;
        }

        if (req == 3) {
            tracker_service.recv_refresh(peer_rank, false);
        }
    }

    for (int i = 1; i < numtasks; ++i) {
        char chunk_hash[HASH_SIZE + 1];
        strcpy(chunk_hash, "!stop!");
        MPI_Send(chunk_hash, HASH_SIZE + 1, MPI_CHAR, i, UPLOAD_THREAD_TAG, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    ifstream fin("in" + to_string(rank) + ".txt");

    Peer peer_client;
    peer_client.rank = rank;

    int num_files;
    fin >> num_files;

    for (int i = 0; i < num_files; ++i) {
        string filename;
        fin >> filename;
        int num_chunks;
        fin >> num_chunks;
        peer_client.owned_files[filename] = num_chunks;
        peer_client.owned_chunks[filename] = vector<string>(num_chunks);

        for (int j = 0; j < num_chunks; ++j) {
            string chunk_hash;
            fin >> chunk_hash;
            peer_client.owned_chunks[filename][j] = chunk_hash;
        }
    }
    
    int num_wanted_files;
    fin >> num_wanted_files;

    for (int i = 0; i < num_wanted_files; ++i) {
        string filename;
        fin >> filename;
        peer_client.wanted_files[filename] = -1;
        peer_client.wanted_chunks[filename] = vector<string>();
    }

    fin.close();

    for (auto &file : peer_client.wanted_files) {
        for (auto &chunk : peer_client.wanted_chunks[file.first]) {
            peer_client.chunk_peers[chunk] = {};
        }
    }

    peer_client.send_refresh();

    int ack;
    MPI_Recv(&ack, 1, MPI_INT, TRACKER_RANK, DOWNLOAD_THREAD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    ThreadArgs args;
    args.peer = &peer_client;

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
