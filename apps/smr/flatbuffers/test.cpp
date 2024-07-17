#include <flatbuffers/flatbuffer_builder.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
  flatbuffers::FlatBufferBuilder builder;
  // auto buff = builder.CreateVector((char*) t);
  auto buff = builder.CreateVector((uint8_t*)t, sizeof(test));
  auto message = smr::CreateMessage(builder,buff);
  builder.Finish(message);
  uint8_t *buf = builder.GetBufferPointer();
  printf("afrer address:%x\n",buf);
  printf("before address:%x\n",t);
  size_t size = builder.GetSize();
  // std::cout << size << std::endl;
  // 打印序列化后的数据（仅用于调试）
  // std::cout << std::endl;
  free(t);
  t->a = 10;
  // 就地
  auto de_message = flatbuffers::GetRoot<smr::Message>(buf);
  // std::cout << de_message->data() << s;
  auto pp = (test*)de_message->data()->Data();
  // memcpy(pp, de_message->data()->Data(), sizeof(test));
  std::cout <<((test*)(pp))->a <<std::endl;
    std::cout <<((test*)(pp))->b <<std::endl;

}