#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <optional>
#include <string>

using namespace std;
namespace fs = std::filesystem;


// -------- BINARY READ/WRITE HELPERS -------- 

static void write_uint32(ofstream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void write_uint64(ofstream& out, uint64_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static uint32_t read_uint32(ifstream& in) {
    uint32_t v;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

static uint64_t read_uint64(ifstream& in) {
    uint64_t v;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

// ---------------------------------------------
//  FileKey: Composite key (file_size + hash)
// ---------------------------------------------
struct FileKey {
    uint64_t size;
    uint64_t hash;

    bool operator==(const FileKey& o) const {
        return size == o.size && hash == o.hash;
    }
};

struct FileKeyHash {
    size_t operator()(const FileKey& k) const noexcept {
        // 64-bit mix (similar to boost::hash_combine)
        uint64_t h = k.hash;
        h ^= k.size + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
        return static_cast<size_t>(h);
    }
};

// ---------------------------------------------
//  File hashing: buffered + DJB2 + xxhash-style mix
// ---------------------------------------------
class FileHash {
public:
    static uint64_t fast_hash(const string& path)
    {
        ifstream file(path, ios::binary);
        if (!file.is_open())
            return 0;

        uint64_t hash = 5381;
        char buffer[4096];

        while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
            size_t n = file.gcount();
            for (size_t i = 0; i < n; i++) {
                hash = ((hash << 5) + hash) + static_cast<unsigned char>(buffer[i]); // DJB2
            }
        }

        // Mix to reduce collisions
        hash ^= (hash >> 33);
        hash *= 0xff51afd7ed558ccdULL;
        hash ^= (hash >> 33);
        hash *= 0xc4ceb9fe1a85ec53ULL;
        hash ^= (hash >> 33);

        return hash;
    }
};

//USED to KEEP PROGRESS

struct LogEntry {
    uint64_t size;
    uint64_t hash;
};

// ---------------------------------------------
//  File discovery
// ---------------------------------------------
class FileDiscovery {
public:
    static vector<string> find(const vector<string>& paths)
    {
        vector<string> out;
        out.reserve(4096);

        for (const auto& p : paths) {
            try {
                for (const auto& entry : fs::recursive_directory_iterator(p)) {
                    if (entry.is_regular_file())
                        out.push_back(entry.path().string());
                }
            }
            catch (const exception& e) {
                cerr << "Error scanning " << p << ": " << e.what() << endl;
            }
        }
        return out;
    }
};

// ---------------------------------------------
//  ht: A clean wrapper over unordered_map
//      ht<Key> : vector<string>
// ---------------------------------------------
template <typename Key, typename Hash = std::hash<Key>>
class ht : public unordered_map<Key, vector<string>, Hash>
{
public:

    // Insert file path under the key
    void insert_value(const Key& k, const string& v)
    {
        (*this)[k].push_back(v);
    }

    
};

// ---------------------------------------------
//  Byte-by-byte exact comparison
// ---------------------------------------------
class FileMatcher {
public:
    static void exact_compare(const vector<string>& paths,
                              vector<pair<string, string>>& matches)
    {
        for (size_t i = 0; i < paths.size(); i++) {
            for (size_t j = i + 1; j < paths.size(); j++) {

                if (fs::file_size(paths[i]) != fs::file_size(paths[j]))
                    continue;

                ifstream f1(paths[i], ios::binary);
                ifstream f2(paths[j], ios::binary);
                if (!f1.is_open() || !f2.is_open())
                    continue;

                istreambuf_iterator<char> b1(f1), e1;
                istreambuf_iterator<char> b2(f2), e2;

                if (equal(b1, e1, b2)) {
                    matches.emplace_back(paths[i], paths[j]);
                }
            }
        }
    }

    static vector<pair<string, string>> find_matches(const vector<string>& paths)
    {

        unordered_map<string, LogEntry> savedLog;

        ifstream logIn(".dupscan.bin", ios::binary);
        if (logIn.is_open()) {
    
            while (true) {
        
                if (logIn.peek() == EOF)
                    break;
        
                uint32_t len = read_uint32(logIn);
                if (!logIn) break;

                string path(len, '\0');
                logIn.read(&path[0], len);
                if (!logIn) break;

                uint64_t size = read_uint64(logIn);
                if (!logIn) break;

                uint64_t hash = read_uint64(logIn);
                if (!logIn) break;
        
                savedLog[path] = LogEntry{ size, hash };
            }
        }       
        
        // Discover files
        auto files = FileDiscovery::find(paths);

        ht<FileKey, FileKeyHash> table;

        
        ofstream logOut(".dupscan.bin", ios::binary | ios::app);

        // Hash each file, but reuse from log when possible
        for (const auto& f : files) {
            uint64_t sz = fs::file_size(f);
            uint64_t h = 0;

        // Look for this file in the saved log
        auto it = savedLog.find(f);
        if (it != savedLog.end() && it->second.size == sz) {
            h = it->second.hash;
            cout << "[REUSE HASH] " << f << " → size=" << sz << ", hash=" << h << "\n";

        } else {
            h = FileHash::fast_hash(f);
            cout << "[HASH] " << f << " → size=" << sz << ", hash=" << h << "\n";

            uint32_t len = static_cast<uint32_t>(f.size());

            write_uint32(logOut, len);

            logOut.write(f.data(), len);
            write_uint64(logOut, sz);
            write_uint64(logOut, h);

            //Update map in memory
            savedLog[f] = LogEntry{ sz, h };

        }

        FileKey key{ sz, h };
        table.insert_value(key, f);
    }
    
        // Compare files inside each bucket
        vector<pair<string, string>> matches;

        for (auto& [key, vec] : table) {
            if (vec.size() > 1)
                exact_compare(vec, matches);
        }

        return matches;
    }
};

// ---------------------------------------------
//  Main
// ---------------------------------------------
int main()
{
    vector<string> paths = { "." };

    auto matches = FileMatcher::find_matches(paths);

    cout << "\n--- EXACT MATCHES ---\n";
    for (auto& m : matches)
        cout << m.first << " <==> " << m.second << "\n";
}
