#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <set>
#include <cctype>
#include <algorithm>
#include <random>

using namespace std;

const int MAX_GUESSES = 6;
const int WORD_LEN = 5;

const string GREEN = "\033[48;5;28m\033[97m";
const string YELLOW = "\033[48;5;220m\033[30m";
const string GRAY = "\033[48;5;59m\033[97m";
const string RESET = "\033[0m";

vector<string> loadWords(const string& filename) {
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

string findWordsDir(const string& exePath) {
    size_t slash = exePath.find_last_of("/\\");
    if (slash != string::npos) {
        string dir = exePath.substr(0, slash);
        if (!dir.empty()) {
            ifstream test(dir + "/words/possible answers.json");
            if (test) return dir + "/words/";
            test.close();
            ifstream test2(dir + "/../words/possible answers.json");
            if (test2) return dir + "/../words/";
        }
    }
    return "words/";
}

void prune(vector<string>& answers,vector<char> gray,vector<pair<char,int>> yellow,vector<pair<char,int>> green){

    for(int i=answers.size()-1;i>=0;i--){
        bool isvalid = true;

        //  for gray 
        for(int j=0;j<5 && isvalid;j++){
            for(int k =0 ;k<gray.size() && isvalid;k++){
                if(gray[k] == answers[i][j])
                    isvalid = false;
            }
        }

        // for yellow
        for(int k =0 ;k<yellow.size() && isvalid;k++){
            bool isfound = false;
            for(int j=0;j<5 && isvalid;j++){
                if((yellow[k].first == answers[i][j]) && yellow[k].second == j)
                    isvalid = false;
                if(yellow[k].first == answers[i][j])
                    isfound = true;
            }
            if(!isfound)
                isvalid = false;
        }

        // for green
        for(int k =0 ;k<green.size() && isvalid;k++){
            bool isfound = false;
            for(int j=0;j<5 && isvalid;j++){
                if(green[k].first == answers[i][j] && green[k].second == j)
                    isfound = true;
            }
            if(!isfound)
                isvalid = false;
        }
        if(!isvalid)
            answers.erase(answers.begin() + i);
    }


}


int main(int argc, char* argv[]) {
    string wordsDir = findWordsDir(argc > 0 ? argv[0] : "");
    vector<string> answers = loadWords(wordsDir + "possible answers.json");
    vector<string> allWords = loadWords(wordsDir + "possible answers + valid words.json");

    vector<char> gray;
    vector<pair<char,int>> yellow;
    vector<pair<char,int>> green;

    gray.push_back('c');
    gray.push_back('r');
    gray.push_back('a');
    

    yellow.push_back({'n',3});
    yellow.push_back({'e',4});

    prune(allWords,gray,yellow,green);

    cout<<allWords.size();


}
