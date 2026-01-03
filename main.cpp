#include <bits/stdc++.h>
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

static inline string trim_copy(string s) {
    auto issp = [](unsigned char c){ return std::isspace(c); };
    while (!s.empty() && issp((unsigned char)s.back())) s.pop_back();
    size_t i = 0;
    while (i < s.size() && issp((unsigned char)s[i])) i++;
    return s.substr(i);
}

static uint32_t parse_hex_u32(const string& s) {
    string t = s;
    if (t.size() >= 2 && (t[0]=='0') && (t[1]=='x' || t[1]=='X')) t = t.substr(2);
    if (t.empty()) return 0;
    return (uint32_t)stoul(t, nullptr, 16);
}

static string bits_of(uint32_t alignedPrefix, uint8_t length) {
    if (length == 0) return "";
    string out;
    out.reserve(length);
    for (int i = 0; i < (int)length; i++) {
        uint32_t bit = (alignedPrefix >> (31 - i)) & 1u;
        out.push_back(bit ? '1' : '0');
    }
    return out;
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

class MultiBitTrie {
public:
    explicit MultiBitTrie(uint8_t s) : stride(s) {
        if (!(stride==1 || stride==2 || stride==4 || stride==8)) throw runtime_error("stride");
        nodes.emplace_back();
    }

    void clear() {
        nodes.clear();
        nodes.emplace_back();
        rules.clear();
    }

    void insert_prefix_bits(uint32_t prefixBitsRightAligned, uint8_t length, uint32_t nextHop) {
        if (length > 32) throw runtime_error("length");
        uint32_t aligned = 0;
        if (length != 0) {
            uint32_t mask = (length == 32) ? 0xFFFFFFFFu : ((1u << length) - 1u);
            uint32_t v = prefixBitsRightAligned & mask;
            aligned = (length == 32) ? v : (v << (32 - length));
        }
        insert_aligned(aligned, length, nextHop);
    }

    optional<uint32_t> lookup(uint32_t ipAligned32) const {
        int cur = 0;
        optional<pair<uint8_t, uint32_t>> best;
        if (nodes[0].hasHop) best = {nodes[0].plen, nodes[0].hop};

        uint8_t pos = 0;
        while (pos + stride <= 32) {
            uint16_t ch = chunk(ipAligned32, pos, stride);
            auto it = nodes[cur].child.find(ch);
            if (it == nodes[cur].child.end()) break;
            cur = it->second;
            if (nodes[cur].hasHop) {
                if (!best.has_value() || nodes[cur].plen > best->first) best = {nodes[cur].plen, nodes[cur].hop};
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

    void tprint_tree() const {
        function<void(int,string)> dfs = [&](int u, string indent){
            if (u == 0) {
                cout << "(root)";
                if (nodes[u].hasHop) cout << " [len=" << (int)nodes[u].plen << ", nh=" << nodes[u].hop << "]";
                cout << "\n";
            }
            vector<pair<uint16_t,int>> kids;
            kids.reserve(nodes[u].child.size());
            for (auto &kv : nodes[u].child) kids.push_back({kv.first, kv.second});
            sort(kids.begin(), kids.end(), [](auto &a, auto &b){ return a.first < b.first; });

            for (auto &kv : kids) {
                uint16_t lab = kv.first;
                int v = kv.second;
                string bits = bits_label(lab);
                cout << indent << bits << " (" << lab << ")";
                if (nodes[v].hasHop) cout << " [len=" << (int)nodes[v].plen << ", nh=" << nodes[v].hop << "]";
                cout << "\n";
                dfs(v, indent + "  ");
            }
        };
        dfs(0, "");
    }

    void tprint_rules() const {
        vector<Rule> r;
        r.reserve(rules.size());
        for (auto &kv : rules) r.push_back(kv.second);
        sort(r.begin(), r.end(), [](const Rule& a, const Rule& b){
            if (a.len != b.len) return a.len < b.len;
            if (a.aligned != b.aligned) return a.aligned < b.aligned;
            return a.hop < b.hop;
        });

        for (auto &x : r) {
            string b = bits_of(x.aligned, x.len);
            if (x.len == 0) cout << "*  " << x.hop << "\n";
            else cout << b << "*  " << x.hop << "\n";
        }
    }

private:
    struct Rule {
        uint32_t aligned = 0;
        uint8_t len = 0;
        uint32_t hop = 0;
    };

    vector<Node> nodes;
    uint8_t stride;

    struct Key {
        uint32_t aligned;
        uint8_t len;
        bool operator==(const Key& o) const { return aligned==o.aligned && len==o.len; }
    };

    struct KeyHash {
        size_t operator()(const Key& k) const {
            return std::hash<uint64_t>()(((uint64_t)k.aligned<<8) | (uint64_t)k.len);
        }
    };

    unordered_map<Key, Rule, KeyHash> rules;

    static uint16_t chunk(uint32_t ip, uint8_t posFromMSB, uint8_t strideBits) {
        uint32_t shift = 32u - (uint32_t)posFromMSB - (uint32_t)strideBits;
        return (uint16_t)((ip >> shift) & ((1u << strideBits) - 1u));
    }

    string bits_label(uint16_t lab) const {
        string s;
        s.reserve(stride);
        for (int i = stride-1; i >= 0; i--) {
            s.push_back(((lab >> i) & 1) ? '1' : '0');
        }
        return s;
    }

    int ensure_child(int cur, uint16_t lab) {
        auto it = nodes[cur].child.find(lab);
        if (it != nodes[cur].child.end()) return it->second;
        int idx = (int)nodes.size();
        nodes.emplace_back();
        nodes[cur].child.emplace(lab, idx);
        return idx;
    }

    void mark_rule(uint32_t aligned, uint8_t len, uint32_t hop) {
        Key k{aligned, len};
        rules[k] = Rule{aligned, len, hop};
    }

    void insert_aligned(uint32_t aligned, uint8_t length, uint32_t nextHop) {
        mark_rule(aligned, length, nextHop);

        if (length == 0) {
            nodes[0].hasHop = true;
            nodes[0].hop = nextHop;
            nodes[0].plen = 0;
            return;
        }

        function<void(int,uint8_t)> rec = [&](int cur, uint8_t pos){
            uint8_t rem = (length >= pos) ? (length - pos) : 0;
            if (rem == 0) {
                nodes[cur].hasHop = true;
                nodes[cur].hop = nextHop;
                nodes[cur].plen = length;
                return;
            }

            if (rem >= stride) {
                uint16_t lab = chunk(aligned, pos, stride);
                int nx = ensure_child(cur, lab);
                rec(nx, pos + stride);
                return;
            }

            uint8_t freeBits = stride - rem;
            uint16_t seg = chunk(aligned, pos, stride);
            uint16_t fixed = (uint16_t)(seg & (~((1u << freeBits) - 1u)));

            for (uint16_t suf = 0; suf < (1u << freeBits); suf++) {
                uint16_t lab = (uint16_t)(fixed | suf);
                int nx = ensure_child(cur, lab);
                nodes[nx].hasHop = true;
                nodes[nx].hop = nextHop;
                nodes[nx].plen = length;
            }
        };

        rec(0, 0);
    }
};

struct PrefixLoadResult { size_t ok=0, skipped=0; };

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

        string a;
        int len, hop;
        {
            stringstream ss(line);
            if (!(ss >> a >> len >> hop)) { res.skipped++; continue; }
        }
        if (len < 0 || len > 32) { res.skipped++; continue; }

        uint32_t bits = parse_hex_u32(a);
        trie.insert_prefix_bits(bits, (uint8_t)len, (uint32_t)hop);
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

static int run_cmd(const string& cmd) {
    int rc = system(cmd.c_str());
    return rc;
}

static void write_gnuplot_files() {
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
}

static void run_gnuplot_if_available() {
    write_gnuplot_files();
    int rc = run_cmd("gnuplot --version > /dev/null 2>&1");
    if (rc != 0) {
        cerr << "[warn] gnuplot not found; plots skipped\n";
        return;
    }
    run_cmd("gnuplot plot_time.gnuplot");
    run_cmd("gnuplot plot_memory.gnuplot");
}

static void write_csv_header(ofstream &out, const vector<string>& cols) {
    for (size_t i = 0; i < cols.size(); i++) {
        if (i) out << ",";
        out << cols[i];
    }
    out << "\n";
}

static int bench_mode(const string& prefixFile, const string& addrFile) {
    vector<string> addrHex;
    try {
        addrHex = load_addresses_hex_strings(addrFile);
    } catch (...) {
        cerr << "Error reading addresses file\n";
        return 2;
    }

    ofstream outRes("lookup_results.csv");
    ofstream outSum("summary.csv");
    if (!outRes || !outSum) {
        cerr << "Error creating CSV files\n";
        return 3;
    }

    write_csv_header(outRes, {"stride","address_hex","next_hop","time_ns"});
    write_csv_header(outSum, {"stride","prefix_count","node_count","edge_count","build_time_ms","lookup_count","min_ns","max_ns","avg_ns","std_ns","est_memory_bytes"});

    vector<int> strides = {1,2,4,8};

    for (int s : strides) {
        MultiBitTrie trie((uint8_t)s);

        auto b1 = chrono::high_resolution_clock::now();
        PrefixLoadResult lr;
        try {
            lr = load_prefix_file_hex3(prefixFile, trie);
        } catch (...) {
            cerr << "Error reading prefix file\n";
            return 4;
        }
        auto b2 = chrono::high_resolution_clock::now();
        long double build_ms = chrono::duration<long double, milli>(b2 - b1).count();

        RunningStats st;

        for (const string& hx : addrHex) {
            uint32_t ip;
            try {
                ip = parse_hex_u32(hx);
            } catch (...) {
                continue;
            }

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

    outRes.flush();
    outSum.flush();
    outRes.close();
    outSum.close();

    run_gnuplot_if_available();
    cerr << "Wrote: lookup_results.csv, summary.csv\n";
    cerr << "Plots: plot_time.png, plot_memory.png (if gnuplot installed)\n";
    return 0;
}

static uint32_t parse_ip_hex32(const string& hx) {
    return parse_hex_u32(hx);
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc == 3) {
        return bench_mode(argv[1], argv[2]);
    }

    cerr << "[aond] started. argc=" << argc << "\n";
    cerr << "=== Interactive Mode ===\n";

    int s;
    cerr << "Stride? (1/2/4/8): ";
    if (!(cin >> s)) return 0;
    if (!(s==1 || s==2 || s==4 || s==8)) {
        cerr << "Invalid stride\n";
        return 1;
    }

    MultiBitTrie trie((uint8_t)s);

    while (true) {
        cerr << "\n--- Menu ---\n";
        cerr << "1) Load prefixes from file\n";
        cerr << "2) Insert prefix (hexPrefix length nextHop)\n";
        cerr << "3) Lookup address (hexAddress)\n";
        cerr << "4) tprint (tree)\n";
        cerr << "5) Memory estimate\n";
        cerr << "6) tprint (rules)\n";
        cerr << "0) Exit\n";
        cerr << "> ";

        int cmd;
        if (!(cin >> cmd)) break;

        if (cmd == 0) break;

        if (cmd == 1) {
            string path;
            cerr << "prefix-file path: ";
            cin >> path;
            try {
                auto r = load_prefix_file_hex3(path, trie);
                cerr << "Loaded: " << r.ok << " (skipped: " << r.skipped << ")\n";
            } catch (...) {
                cerr << "Error reading prefix file\n";
            }
        } else if (cmd == 2) {
            string hx;
            int len, hop;
            cerr << "Enter: hexPrefix length nextHop : ";
            cin >> hx >> len >> hop;
            try {
                trie.insert_prefix_bits(parse_hex_u32(hx), (uint8_t)len, (uint32_t)hop);
                cerr << "OK\n";
            } catch (...) {
                cerr << "Error\n";
            }
        } else if (cmd == 3) {
            string hx;
            cerr << "hexAddress: ";
            cin >> hx;
            uint32_t ip = 0;
            try { ip = parse_ip_hex32(hx); }
            catch (...) { cerr << "Bad hex\n"; continue; }

            auto t1 = chrono::high_resolution_clock::now();
            auto hop = trie.lookup(ip);
            auto t2 = chrono::high_resolution_clock::now();
            long double ns = chrono::duration<long double, nano>(t2 - t1).count();

            if (hop.has_value()) cerr << "Next hop: " << *hop << " | time(ns): " << (double)ns << "\n";
            else cerr << "Next hop: None | time(ns): " << (double)ns << "\n";
        } else if (cmd == 4) {
            trie.tprint_tree();
        } else if (cmd == 5) {
            cerr << "node_count=" << trie.nodeCount()
                 << " edge_count=" << trie.edgeCount()
                 << " est_memory_bytes=" << trie.estimateMemoryBytes() << "\n";
        } else if (cmd == 6) {
            trie.tprint_rules();
        } else {
            cerr << "Unknown\n";
        }
    }

    return 0;
}
