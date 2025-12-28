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

class MultiBitTrie {
public:
    explicit MultiBitTrie(uint8_t s) : stride(s) {
        if (!(stride==1 || stride==2 || stride==4 || stride==8)) throw runtime_error("stride");
        nodes.emplace_back();
    }

    void insert(uint32_t prefix, uint8_t length, uint32_t nextHop) {
        if (length > 32) throw runtime_error("length");
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

private:
    vector<Node> nodes;
    uint8_t stride;

    uint16_t chunk(uint32_t ip, uint8_t posFromMSB) const {
        uint32_t shift = 32u - (uint32_t)posFromMSB - (uint32_t)stride;
        return (uint16_t)((ip >> shift) & ((1u << stride) - 1u));
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

struct PrefixLoadResult {
    size_t ok = 0;
    size_t skipped = 0;
};

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

static int run_cmd(const string& cmd) {
    int rc = system(cmd.c_str());
    return rc;
}

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

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <prefix-file> <addresses-file>\n";
        return 1;
    }

    string prefixFile = argv[1];
    string addrFile = argv[2];

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

    generate_plots_with_gnuplot();
    return 0;
}
