
#include <bits/stdc++.h>
#include <unistd.h>
using namespace std;

struct Node {
    unordered_map<uint16_t, int> child;
    bool hasHop = false;
    uint32_t hop = 0;
    uint8_t plen = 0;
};

struct RunningStats {
    long long n = 0;
    long double mean = 0.0L;
    long double m2 = 0.0L;
    long double minv = 0.0L;
    long double maxv = 0.0L;

    void add(long double x) {
        if (n == 0) minv = maxv = x;
        minv = min(minv, x);
        maxv = max(maxv, x);
        n++;
        long double d = x - mean;
        mean += d / n;
        long double d2 = x - mean;
        m2 += d * d2;
    }

    long double var() const { return n ? (m2 / n) : 0.0L; }
    long double stddev() const { return sqrt(var()); }
};

class MultiBitTrie {
public:
    explicit MultiBitTrie(uint8_t s) : stride(s) {
        if (!(stride==1 || stride==2 || stride==4 || stride==8)) throw runtime_error("invalid stride");
        nodes.emplace_back();
    }

    void insert(uint32_t prefix, uint8_t length, uint32_t nextHop) {
        if (length > 32) throw runtime_error("invalid length");
        if (length == 0) {
            nodes[0].hasHop = true;
            nodes[0].hop = nextHop;
            nodes[0].plen = 0;
            return;
        }

        uint32_t masked = (length == 32) ? prefix : (prefix & (0xFFFFFFFFu << (32 - length)));
        int cur = 0;
        uint8_t pos = 0;

        while (pos + stride <= length) {
            uint16_t ch = chunk(masked, pos);
            auto it = nodes[cur].child.find(ch);
            if (it == nodes[cur].child.end()) {
                int idx = (int)nodes.size();
                nodes.emplace_back();
                nodes[cur].child.emplace(ch, idx);
                cur = idx;
            } else {
                cur = it->second;
            }
            pos += stride;
        }

        nodes[cur].hasHop = true;
        nodes[cur].hop = nextHop;
        nodes[cur].plen = length;
    }

    optional<uint32_t> lookup(uint32_t ip) const {
        int cur = 0;
        optional<pair<uint8_t, uint32_t>> best;
        if (nodes[0].hasHop) best = {nodes[0].plen, nodes[0].hop};

        uint8_t pos = 0;
        while (pos + stride <= 32) {
            uint16_t ch = chunk(ip, pos);
            auto it = nodes[cur].child.find(ch);
            if (it == nodes[cur].child.end()) break;
            cur = it->second;
            if (nodes[cur].hasHop) {
                if (!best.has_value() || nodes[cur].plen > best->first)
                    best = {nodes[cur].plen, nodes[cur].hop};
            }
            pos += stride;
        }
        if (best.has_value()) return best->second;
        return {};
    }

    size_t nodeCount() const { return nodes.size(); }

    size_t edgeCount() const {
        size_t e = 0;
        for (auto &nd : nodes) e += nd.child.size();
        return e;
    }

    size_t estimateMemoryBytes() const {
        size_t e = edgeCount();
        size_t approxNode = sizeof(Node);
        size_t approxEdge = sizeof(pair<const uint16_t, int>) + 24;
        return nodes.size() * approxNode + e * approxEdge;
    }

    void tprint(ostream& os = cerr) const {
        os << "(root)";
        if (nodes[0].hasHop) os << " [len=" << (int)nodes[0].plen << ", nh=" << nodes[0].hop << "]";
        os << "\n";
        tprint_dfs(os, 0, 0);
        os.flush();
    }

private:
    vector<Node> nodes;
    uint8_t stride;

    uint16_t chunk(uint32_t ip, uint8_t posFromMSB) const {
        uint32_t shift = 32u - (uint32_t)posFromMSB - (uint32_t)stride;
        return (uint16_t)((ip >> shift) & ((1u << stride) - 1u));
    }

    string chunk_to_bin(uint16_t v) const {
        string s(stride, '0');
        for (int i = (int)stride - 1; i >= 0; --i) {
            s[i] = (v & 1) ? '1' : '0';
            v >>= 1;
        }
        return s;
    }

    void tprint_dfs(ostream& os, int nodeIdx, int depth) const {
        vector<pair<uint16_t,int>> kids;
        kids.reserve(nodes[nodeIdx].child.size());
        for (auto &kv : nodes[nodeIdx].child) kids.push_back(kv);
        sort(kids.begin(), kids.end(), [](auto &a, auto &b){ return a.first < b.first; });

        for (auto &kv : kids) {
            uint16_t lab = kv.first;
            int childIdx = kv.second;
            for (int i = 0; i < depth + 1; ++i) os << "  ";
            os << chunk_to_bin(lab) << " (" << lab << ")";
            if (nodes[childIdx].hasHop) {
                os << " [len=" << (int)nodes[childIdx].plen << ", nh=" << nodes[childIdx].hop << "]";
            }
            os << "\n";
            tprint_dfs(os, childIdx, depth + 1);
        }
    }
};

static inline string trim_copy(string s) {
    auto issp = [](unsigned char c){ return std::isspace(c); };
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && issp((unsigned char)s[i])) i++;
    return s.substr(i);
}

static uint32_t parse_hex_u32(const string& s) {
    return (uint32_t)stoul(s, nullptr, 16);
}

struct PrefixLoadResult { size_t ok = 0; size_t skipped = 0; };

static PrefixLoadResult load_prefix_file_hex3(const string& path, MultiBitTrie &trie) {
    ifstream in(path);
    if (!in) throw runtime_error("open_prefix");
    PrefixLoadResult res;
    string line;
    while (getline(in, line)) {
        auto h = line.find('#');
        if (h != string::npos) line = line.substr(0, h);
        line = trim_copy(line);
        if (line.empty()) continue;

        string a; int len; long long hop;
        stringstream ss(line);
        if (!(ss >> a >> len >> hop)) { res.skipped++; continue; }
        if (len < 0 || len > 32) { res.skipped++; continue; }

        uint32_t prefix = parse_hex_u32(a);
        trie.insert(prefix, (uint8_t)len, (uint32_t)hop);
        res.ok++;
    }
    return res;
}

static vector<string> load_addresses_hex_strings(const string& path) {
    ifstream in(path);
    if (!in) throw runtime_error("open_addr");
    vector<string> out;
    string line;
    while (getline(in, line)) {
        line = trim_copy(line);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}

static void write_csv_header(ofstream &out, const vector<string>& cols) {
    for (size_t i = 0; i < cols.size(); i++) {
        if (i) out << ",";
        out << cols[i];
    }
    out << "\n";
}

static string csv_escape(const string& s) {
    bool need = false;
    for (char c : s) if (c==',' || c=='"' || c=='\n' || c=='\r') { need = true; break; }
    if (!need) return s;
    string t = "\"";
    for (char c : s) t += (c=='"') ? string("\"\"") : string(1, c);
    t += "\"";
    return t;
}

static int run_cmd(const string& cmd) { return system(cmd.c_str()); }

static void generate_plots_with_gnuplot() {
    {
        ofstream gp("plot_time.gnuplot");
        gp <<
        "set datafile separator ','\n"
        "set terminal png size 900,600\n"
        "set output 'plot_time.png'\n"
        "set title 'Average Lookup Time vs Stride'\n"
        "set xlabel 'Stride (bits)'\n"
        "set ylabel 'Average Lookup Time (ns)'\n"
        "set grid\n"
        "set style data histograms\n"
        "set style fill solid 1.0 border -1\n"
        "set boxwidth 0.7\n"
        "plot 'summary.csv' using 1:9 every ::1 title 'Avg(ns)' with boxes\n";
    }
    {
        ofstream gp("plot_memory.gnuplot");
        gp <<
        "set datafile separator ','\n"
        "set terminal png size 900,600\n"
        "set output 'plot_memory.png'\n"
        "set title 'Estimated Memory Usage vs Stride'\n"
        "set xlabel 'Stride (bits)'\n"
        "set ylabel 'Estimated Memory (bytes)'\n"
        "set grid\n"
        "set style data histograms\n"
        "set style fill solid 1.0 border -1\n"
        "set boxwidth 0.7\n"
        "plot 'summary.csv' using 1:11 every ::1 title 'Mem(bytes)' with boxes\n";
    }
    run_cmd("gnuplot plot_time.gnuplot");
    run_cmd("gnuplot plot_memory.gnuplot");
}

static void interactive_mode() {
    cerr << "=== Interactive Mode ===\n" << flush;
    cerr << "Stride? (1/2/4/8): " << flush;

    int s;
    if (!(cin >> s)) {
        cerr << "\nInput error (cin failed).\n";
        return;
    }
    if (!(s==1 || s==2 || s==4 || s==8)) {
        cerr << "Invalid stride.\n";
        return;
    }

    MultiBitTrie trie((uint8_t)s);

    while (true) {
        cerr << "\n--- Menu ---\n"
             << "1) Load prefixes from file\n"
             << "2) Insert prefix (hexPrefix length nextHop)\n"
             << "3) Lookup address (hexAddress)\n"
             << "4) tprint\n"
             << "5) Memory estimate\n"
             << "0) Exit\n"
             << "> " << flush;

        int cmd;
        if (!(cin >> cmd)) {
            cerr << "\nEOF/invalid input. Exiting.\n";
            return;
        }
        if (cmd == 0) return;

        if (cmd == 1) {
            string path;
            cerr << "prefix-file path: " << flush;
            cin >> path;
            try {
                auto lr = load_prefix_file_hex3(path, trie);
                cerr << "Loaded: " << lr.ok << " (skipped: " << lr.skipped << ")\n";
            } catch (const exception& e) {
                cerr << "Error: " << e.what() << "\n";
            }
        } else if (cmd == 2) {
            string hx; int len; long long hop;
            cerr << "Enter: hexPrefix length nextHop : " << flush;
            cin >> hx >> len >> hop;
            try {
                trie.insert(parse_hex_u32(hx), (uint8_t)len, (uint32_t)hop);
                cerr << "OK\n";
            } catch (const exception& e) {
                cerr << "Insert failed: " << e.what() << "\n";
            }
        } else if (cmd == 3) {
            string hx;
            cerr << "hexAddress: " << flush;
            cin >> hx;
            try {
                uint32_t ip = parse_hex_u32(hx);
                auto t1 = chrono::high_resolution_clock::now();
                auto nh = trie.lookup(ip);
                auto t2 = chrono::high_resolution_clock::now();
                long double ns = chrono::duration<long double, nano>(t2 - t1).count();
                cerr << "Next hop: " << (nh ? to_string(*nh) : string("None"))
                     << " | time(ns): " << (double)ns << "\n";
            } catch (const exception& e) {
                cerr << "Lookup failed: " << e.what() << "\n";
            }
        } else if (cmd == 4) {
            trie.tprint(cerr);
        } else if (cmd == 5) {
            cerr << "node_count=" << trie.nodeCount()
                 << " edge_count=" << trie.edgeCount()
                 << " est_memory_bytes=" << trie.estimateMemoryBytes()
                 << "\n";
        } else {
            cerr << "Unknown command.\n";
        }
    }
}

static int batch_mode(const string& prefixFile, const string& addrFile) {
    vector<string> addrHex = load_addresses_hex_strings(addrFile);

    ofstream outRes("lookup_results.csv");
    ofstream outSum("summary.csv");
    if (!outRes || !outSum) throw runtime_error("cannot create csv");

    write_csv_header(outRes, {"stride","address_hex","next_hop","time_ns"});
    write_csv_header(outSum, {"stride","prefix_count","node_count","edge_count","build_time_ms",
                              "lookup_count","min_ns","max_ns","avg_ns","std_ns","est_memory_bytes"});

    vector<int> strides = {1,2,4,8};

    for (int s : strides) {
        MultiBitTrie trie((uint8_t)s);

        auto b1 = chrono::high_resolution_clock::now();
        auto lr = load_prefix_file_hex3(prefixFile, trie);
        auto b2 = chrono::high_resolution_clock::now();
        long double build_ms = chrono::duration<long double, milli>(b2 - b1).count();

        RunningStats st;

        for (const string& hx : addrHex) {
            uint32_t ip = parse_hex_u32(hx);

            auto t1 = chrono::high_resolution_clock::now();
            auto hop = trie.lookup(ip);
            auto t2 = chrono::high_resolution_clock::now();

            long double ns = chrono::duration<long double, nano>(t2 - t1).count();
            st.add(ns);

            outRes << s << ","
                   << csv_escape(hx) << ","
                   << (hop.has_value() ? to_string(*hop) : string("None")) << ","
                   << (double)ns << "\n";
        }

        outSum << s << ","
               << lr.ok << ","
               << trie.nodeCount() << ","
               << trie.edgeCount() << ","
               << (double)build_ms << ","
               << addrHex.size() << ","
               << (double)st.minv << ","
               << (double)st.maxv << ","
               << (double)st.mean << ","
               << (double)st.stddev() << ","
               << trie.estimateMemoryBytes() << "\n";
    }

    outRes.close();
    outSum.close();
    generate_plots_with_gnuplot();
    return 0;
}

int main(int argc, char** argv) {
    // پیام شروع را روی stderr چاپ می‌کنیم تا همیشه دیده شود
    cerr << "[aond] started. argc=" << argc << "\n" << flush;

    try {
        if (argc < 3) {
            interactive_mode();
            return 0;
        }
        return batch_mode(argv[1], argv[2]);
    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
