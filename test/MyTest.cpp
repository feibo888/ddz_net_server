//
// Created by fb on 2025/6/7.
//

#include "MyTest.h"


void MyTest::test()
{
    fb::Person p;
    p.set_id(10);
    p.set_age(18);
    p.add_name("fb");
    p.add_name("ffb");

    p.set_sex("male");
    p.mutable_addr()->set_addr("hfut");
    p.mutable_addr()->set_num(11000);
    p.set_color(fb::Color::Blue);

    //序列化p到一个字符串中
    std::string output;
    p.SerializeToString(&output);

    std::cout << output << std::endl;

    //反序列化
    fb::Person pp;
    pp.ParseFromString(output);
    std::cout << pp.id() << ", " << pp.age() << ", " << pp.sex() << std::endl;
    std::cout << pp.addr().addr() << ", " << pp.addr().num() << std::endl;
    std::cout << pp.color() << std::endl;

    int size = pp.name_size();
    for (int i = 0; i < size; i++)
    {
        std::cout << pp.name(i) << std::endl;
    }

}
