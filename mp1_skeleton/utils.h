#include <string>
using namespace std;

// trim first and last spaces of a string
inline string trimSpaces(string str) {
  cout<< "(Trim:"<< str;
  str.erase(0, str.find_first_not_of(" "));
  str.erase(str.find_last_not_of(" ") + 1);
  cout<< ">>"<< str <<")" << endl;
  return str;
}