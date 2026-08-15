#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

const int WORD_LEN = 5;

vector<string> loadJsonWords(const string& filename) {
    ifstream file(filename);
    if (!file) return {};

    string content((istreambuf_iterator<char>(file)),
                        istreambuf_iterator<char>());
    file.close();

    size_t start = content.find('[');
    size_t end = content.rfind(']');
    if (start == string::npos || end == string::npos || end <= start)
        return {};

    string array = content.substr(start + 1, end - start - 1);

    vector<string> words;
    size_t pos = 0;
    while ((pos = array.find('"', pos)) != string::npos) {
        size_t close = array.find('"', pos + 1);
        if (close == string::npos) break;
        string word = array.substr(pos + 1, close - pos - 1);
        if (word.size() == WORD_LEN) words.push_back(word);
        pos = close + 1;
    }
    return words;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.json> <output.bin>\n";
        return 1;
    }

    vector<string> words = loadJsonWords(argv[1]);
    if (words.empty()) {
        cerr << "Error: no words found in " << argv[1] << endl;
        return 1;
    }

    ofstream out(argv[2], ios::binary);
    if (!out) {
        cerr << "Error: cannot write " << argv[2] << endl;
        return 1;
    }

    for (const string& w : words)
        out.write(w.c_str(), WORD_LEN);
    out.close();

    cout << "Wrote " << words.size() << " words ("
         << words.size() * WORD_LEN << " bytes) to " << argv[2] << "\n";
    return 0;
}