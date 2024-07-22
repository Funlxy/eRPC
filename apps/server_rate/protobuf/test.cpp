#include <string>
#include "message.pb.h"
#include "iostream"
  std::string s = std::string(1000000000,'a');

int main()
{
  Hello::Req req;
  req.set_data(s);
  // std::cout << req.data() << std::endl;
  std:: cout << req.ByteSizeLong() << std::endl;
  // req.SerializeToArray(ptr, req.ByteSizeLong());
    // std::cout << req.data() << std::endl;

  // std::cout << c << std::endl;
}