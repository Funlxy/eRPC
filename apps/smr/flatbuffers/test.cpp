#include <flatbuffers/flatbuffer_builder.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include "message_generated.h"
struct test{
  int a;
  size_t b;
  float c;
};
int main()
{
  // auto ptr = new test();
  auto t = new test();
  t->a = 1;
  t->b = 2;
  t->c = 3.1;
  // test c = {1,2,3};
  std::cout << sizeof(test) << std::endl;
  std::cout << sizeof(*t) << std::endl;
  flatbuffers::FlatBufferBuilder builder;
  // auto buff = builder.CreateVector((char*) t);
  auto buff = builder.CreateVector((uint8_t*)t, sizeof(test));
  auto message = smr::CreateMessage(builder,buff);
  builder.Finish(message);
  uint8_t *buf = builder.GetBufferPointer();
  printf("afrer address:%x\n",buf);
  printf("before address:%x\n",t);
  size_t size = builder.GetSize();
  std::cout << size << std::endl;
  // 打印序列化后的数据（仅用于调试）
  std::cout << "Serialized data: ";
  for (size_t i = 0; i < size; ++i) {
      std::cout << static_cast<int>(buf[i]) << " ";
  }
  std::cout << std::endl;
  t->a = 10;
  // 就地
  auto de_message = flatbuffers::GetRoot<smr::Message>(buf);
  auto de_t = (test*)de_message->data()->Data();
  std::cout << de_t->a << " " << de_t->b << " " << de_t->c << std::endl;
  // 这里是24与sizeof(test)相同
  std::cout << de_message->data()->size() << std::endl;

}