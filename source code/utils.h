#include <string>
#include <iostream>
using namespace std;

// trim first and last spaces of a string
inline string trimSpaces(string str) {
  cout<< "(Trim:"<< str;
  str.erase(0, str.find_first_not_of(" "));
  str.erase(str.find_last_not_of(" ") + 1);
  cout<< ">>"<< str <<")" << endl;
  return str;
}

// inline vector<string> splitString(string str, char delim) {
//   vector<string> tokens;
//   string token;
//   while (getline(str, token, delim)) {
//     tokens.push_back(token);
//   }
//   return tokens;
// }

inline vector<string> splitString(string &v, char delim){
  vector<string> res;
  string tmp;

  for( size_t i = 0; i < v.size(); i++){
    tmp += v[i];
    if (v[i] == delim){
        res.push_back(tmp.substr(0, tmp.size()-1));
        tmp = "";
    }else if (i == v.size() - 1){
        res.push_back(tmp);
    }
  }
  return res;
}

inline bool createFile(string filePath) {
  ofstream file(filePath);
  if (file.is_open()) {
    file.close();
    return true;
  }
  return false;
}
