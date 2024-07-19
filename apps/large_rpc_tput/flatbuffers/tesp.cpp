#include <flatbuffers/buffer.h>
#include <flatbuffers/flatbuffer_builder.h>
#include <cstdint>
// #include "./meessage_generated.h"
#include <iostream>
#include <string>
#include "./hello_generated.h"

int main() {
  // int a = 1;
  std::string s = std::string(0,'a');
  flatbuffers::FlatBufferBuilder builder;
  auto offset = builder.CreateString(s);
  auto name = Hello::CreateRequest(builder,offset);
  builder.Finish(name);
  std::cout << builder.GetSize()-s.size() << std::endl;
  // 16 + 4 + 4
  // auto int_offset = builder.c
  // auto offset = builder.CreateString(s);
  // auto t = Hello::CreateRequest(builder, offset);
  // builder.Finish(t);

  // auto size = builder.GetSize();
  // const uint8_t *buf = builder.GetBufferPointer();

  // std::cout << "Buffer size: " << size << std::endl;
  // // Print buffer content
  // print_buffer(buf, size);

  // // Manually parse the buffer (example)
  // auto root_offset = flatbuffers::GetRoot<flatbuffers::Table>(buf)->GetField<uint32_t>(4, 0);
  // auto string_offset = flatbuffers::Getfi<uint32_t>(buf, root_offset);
  // auto string_ptr = flatbuffers::GetPointer<const char*>(buf + string_offset);
  // std::cout << "Parsed message: " << string_ptr << std::endl  ;
}