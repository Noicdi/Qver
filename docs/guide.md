# 风格规范

本项目参考 Google Style Guide，并有一些自己的改动

## 命名规范

使用描述性命名，少用缩写

- 文件命名：头文件与源文件所有单词小写，单词以`-`做间隔

- 变量：所有单词小写，单词以`_`做间隔

- 函数：首单词小写，后续单词首字母大写

- 类：所有单词首字母大写

- 成员变量：所有单词小写，单词以`_`做间隔，最后以`_`结尾

- 成员函数：首单词小写，后续单词首字母大写

- 宏：全部大写，单词以`_`做间隔


```c++
// MyClass.h
class MyClass {
private:
    std::string name_;
    int name_length_;
public:
    std::string getName();
    int getNameLength();
};

// MyClass.cpp
// 定义相关的成员函数

// main.cpp
int testClass();

int main()
{
    MyClass test_class;
    int name_length;
}
```

