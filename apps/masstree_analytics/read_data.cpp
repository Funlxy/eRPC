#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <string>
#include <regex>
#include "protobuf/message.pb.h"
int init_workload(std::string path,std::vector<std::pair<std::string,std::string>>& data){
  std::ifstream infile(path);
  if (!infile.is_open()) {
      std::cerr << "cant open file" << std::endl;
      return -1;
  }

  // 一次性读取整个文件内容
  std::stringstream buffer;
  buffer << infile.rdbuf();
  std::string content = buffer.str();
  infile.close();

  // 按行分割文件内容
  std::istringstream contentStream(content);
  std::string line;
  const std::string prefix = "INSERT usertable ";
  std::regex pattern(R"((user\d+) \[ field0=(.+)\])");
  while (std::getline(contentStream, line)) {
      // 检查行是否以指定的前缀开始和以指定的后缀结束
      if (line.compare(0, prefix.length(), prefix) == 0) {
          // 提取中间的部分
        std::smatch match;
        if (std::regex_search(line, match, pattern) && match.size() > 2) {
            std::string key = match.str(1);
            std::string value = match.str(2);
            value.pop_back();
            data.push_back({key,value});
        }
      }
  }

  printf("work load cnts: %zu\n",data.size());
  return 0;
}
int main() 
{
  std::regex pattern(R"((user\d+) \[ field0=(.+)\])");
  std::string s = "INSERT usertable user4052466453699787802 [ field0=9Da']9<Da>T3)U))5f3?v.B%=:d>3.-34!P=)B%4Ak;=v\"C=*S599 ,U-0?j3#d+ ]";
  std::smatch match;

  if (std::regex_search(s, match, pattern) && match.size() > 2) {
    std::string key = match.str(1);
    std::string value = match.str(2);
    std::cout << key.size() << "\n" << value.size() << std::endl;
    // data.push_back({key,value});
  }
// {
//     // 打开文件
//   std::ifstream infile("ycsb_run.txt");
//   if (!infile.is_open()) {
//       std::cerr << "cant open file" << std::endl;
//       return -1;
//   }

//   // 一次性读取整个文件内容
//   std::stringstream buffer;
//   buffer << infile.rdbuf();
//   std::string content = buffer.str();
//   infile.close();

//   // 按行分割文件内容
//   std::istringstream contentStream(content);
//   std::string line;
//   const std::string prefix = "READ usertable ";
//   const std::string suffix = " [ field0 ]";
//   int cnt = 0;
//   std::vector<std::string> cur; 
//   while (std::getline(contentStream, line)) {
//       // 检查行是否以指定的前缀开始和以指定的后缀结束
//       if (line.compare(0, prefix.length(), prefix) == 0 && 
//           line.compare(line.length() - suffix.length(), suffix.length(), suffix) == 0) {
//           // 提取中间的部分
//           std::string user_field = line.substr(prefix.length(), 
//                                                 line.length() - prefix.length() - suffix.length());
//           cur.push_back(user_field);
//       }
//   }
//   std::cout << cur.size() << std::endl;
//   std::vector<std::vector<std::string>>work_load;
//   int minSize = cur.size() / 10;
//   int extra = cur.size() % 10;
//   work_load.reserve(10);
//   auto it = cur.begin();
//   for (int i = 0; i < 10; ++i) {
//       int currentSize = minSize + (i < extra ? 1 : 0);
//       work_load[i].insert(work_load[i].end(), it, it + currentSize);
//       it += currentSize;
//   }
//   for(int i = 0 ; i < 10 ; i++) std::cout << work_load[i].size() << std::endl;
//     return 0;
// }
} 