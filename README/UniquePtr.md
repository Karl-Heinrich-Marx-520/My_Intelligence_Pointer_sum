# UniquePtr 自定义实现 README
## 概述
本实现是对 C++ 标准库 `std::unique_ptr` 的轻量级模拟，是一款**独占式智能指针**，用于管理动态分配的内存资源。其核心设计目标是**确保资源的独占所有权**，通过自动调用删除器释放内存，避免手动管理裸指针导致的内存泄漏、二次释放等问题。

本实现支持基础的内存管理、移动语义、自定义删除器，且针对数组类型提供了特化版本，接口设计与标准库 `std::unique_ptr` 保持一致，易于理解和使用。

## 核心特性
| 特性 | 说明 |
|------|------|
| 独占所有权 | 禁止拷贝构造/拷贝赋值，确保同一时间只有一个 `UniquePtr` 持有资源 |
| 移动语义 | 支持移动构造/移动赋值，所有权可通过 `std::move` 转移 |
| 自定义删除器 | 支持自定义删除器（默认使用 `delete`/`delete[]`），适配不同资源释放逻辑 |
| 数组特化 | 针对数组类型 `T[]` 提供特化版本，重载 `[]` 运算符，禁用不适用的 `*`/`->` 运算符 |
| 空指针安全 | 构造/重置支持空指针，析构/重置空指针无异常 |
| 断言检查 | 解引用/成员访问时通过 `assert` 检查空指针，避免非法访问 |

## 接口说明
### 1. 模板参数
```cpp
template <typename T, typename Deleter = default_delete<T>>
class UniquePtr;
```
- `T`：智能指针管理的对象类型（支持普通类型/数组类型 `T[]`）；
- `Deleter`：删除器类型，默认使用 `default_delete<T>`（适配 `delete`），数组特化版默认使用 `default_delete<T[]>`（适配 `delete[]`）。

### 2. 构造与析构
| 接口 | 说明 |
|------|------|
| `UniquePtr() noexcept` | 默认构造：创建空 `UniquePtr`，持有 `nullptr`，使用默认删除器 |
| `explicit UniquePtr(T* ptr, Deleter d = Deleter()) noexcept` | 裸指针构造：接管 `ptr` 的所有权，可指定自定义删除器 `d` |
| `UniquePtr(UniquePtr&& other) noexcept` | 移动构造：转移 `other` 的资源所有权，`other` 会后置为空 |
| `~UniquePtr()` | 析构函数：自动调用删除器释放持有的资源 |

### 3. 核心操作
| 接口 | 说明 |
|------|------|
| `T* release() noexcept` | 释放资源所有权，返回裸指针，`UniquePtr` 后置为空（不释放内存） |
| `void reset(T* p = nullptr) noexcept` | 重置资源：释放当前持有的资源，接管新指针 `p`（默认空指针） |
| `T* get() const noexcept` | 获取裸指针（仅访问，不释放所有权） |
| `const Deleter& get_deleter() const noexcept` | 获取删除器的常量引用 |
| `explicit operator bool() const noexcept` | 布尔转换：判断是否持有有效资源（`ptr_ != nullptr` 时返回 `true`） |

### 4. 运算符重载
| 运算符 | 说明 |
|--------|------|
| `T& operator*() const` | 解引用：返回资源的引用（非数组版），空指针时触发 `assert` |
| `T* operator->() const` | 成员访问：返回裸指针（非数组版），空指针时触发 `assert` |
| `T& operator[](std::size_t idx) const` | 数组下标访问（仅数组特化版），返回第 `idx` 个元素的引用 |
| `UniquePtr& operator=(UniquePtr&& other) noexcept` | 移动赋值：转移 `other` 的所有权，先释放当前资源，`other` 后置为空 |

### 5. 禁用接口
| 接口 | 说明 |
|------|------|
| `UniquePtr(const UniquePtr&) = delete` | 禁用拷贝构造，确保独占所有权 |
| `UniquePtr& operator=(const UniquePtr&) = delete` | 禁用拷贝赋值，确保独占所有权 |

## 使用示例
### 示例 1：基础使用（普通类型）
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "headfiles.h" // 包含 UniquePtr 及 default_delete 定义

int main() {
    // 1. 构造并管理单个对象
    UniquePtr<int> up(new int(100));
    std::cout << "值：" << *up << std::endl; // 输出：100
    std::cout << "是否持有资源：" << std::boolalpha << (bool)up << std::endl; // 输出：true

    // 2. 重置资源
    up.reset(new int(200));
    std::cout << "重置后值：" << *up << std::endl; // 输出：200

    // 3. 释放所有权（手动管理）
    int* raw_ptr = up.release();
    std::cout << "释放后是否持有资源：" << (bool)up << std::endl; // 输出：false
    delete raw_ptr; // 释放所有权后需手动删除

    return 0;
}
```

### 示例 2：自定义删除器
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <functional>
#include "headfiles.h"

// 自定义删除器：打印日志并释放内存
struct CustomDeleter {
    void operator()(std::string* ptr) const noexcept {
        std::cout << "自定义删除器：释放 string 对象，值 = " << *ptr << std::endl;
        delete ptr;
    }
};

int main() {
    // 使用自定义删除器构造
    UniquePtr<std::string, CustomDeleter> up(
        new std::string("hello unique_ptr"), 
        CustomDeleter()
    );
    std::cout << "字符串值：" << *up << std::endl; // 输出：hello unique_ptr

    // 析构时自动调用自定义删除器
    return 0;
}
```
**输出**：
```
字符串值：hello unique_ptr
自定义删除器：释放 string 对象，值 = hello unique_ptr
```

### 示例 3：数组特化版使用
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "headfiles.h"

int main() {
    // 数组特化版：管理动态数组
    UniquePtr<int[]> up(new int[5]{1, 2, 3, 4, 5});
    
    // 重载[]运算符访问元素
    for (int i = 0; i < 5; ++i) {
        std::cout << up[i] << " "; // 输出：1 2 3 4 5
    }
    std::cout << std::endl;

    // 数组版禁用*和->运算符（编译报错）
    // std::cout << *up << std::endl; // 编译错误
    // up->some_func(); // 编译错误

    // 重置数组
    up.reset(new int[3]{10, 20, 30});
    std::cout << up[1] << std::endl; // 输出：20

    return 0;
}
```

### 示例 4：移动语义
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include "headfiles.h"

int main() {
    UniquePtr<int> up1(new int(999));
    std::cout << "up1 初始值：" << *up1 << std::endl; // 输出：999

    // 移动构造：up1 所有权转移给 up2，up1 后置为空
    UniquePtr<int> up2(std::move(up1));
    std::cout << "up2 移动后值：" << *up2 << std::endl; // 输出：999
    std::cout << "up1 移动后是否为空：" << std::boolalpha << !up1 << std::endl; // 输出：true

    // 移动赋值：up2 所有权转移给 up3
    UniquePtr<int> up3;
    up3 = std::move(up2);
    std::cout << "up3 赋值后值：" << *up3 << std::endl; // 输出：999

    return 0;
}
```

## 注意事项
1. **禁止拷贝**：`UniquePtr` 禁用了拷贝构造和拷贝赋值，若尝试 `UniquePtr up2 = up1;` 会直接编译失败，需通过 `std::move` 转移所有权。
2. **断言检查**：解引用（`*`）、成员访问（`->`）时会通过 `assert` 检查空指针，Debug 模式下空指针访问会触发断言，Release 模式下 `assert` 失效，需自行保证指针有效性。
3. **删除器要求**：自定义删除器需满足可调用（重载 `operator()`）、 noexcept（建议），且与管理的资源类型匹配（数组资源需适配 `delete[]`）。
4. **数组特化**：数组版 `UniquePtr<T[]>` 仅支持 `[]` 运算符，禁用 `*`/`->`，避免对数组进行非法的解引用操作。
5. **所有权转移**：`release()` 仅释放所有权不释放内存，调用后需手动管理返回的裸指针；`reset()` 会先释放当前资源再接管新资源，是更安全的重置方式。

## 与 std::unique_ptr 的对比
| 特性 | 自定义 UniquePtr | std::unique_ptr |
|------|------------------|-----------------|
| 核心功能 | 独占所有权、移动语义、自动释放 | 完全一致 |
| 数组特化 | 继承基类实现，禁用*/*-> | 原生支持，接口一致 |
| 自定义删除器 | 模板参数传递 | 模板参数传递（C++11+） |
| 空指针安全 | 析构/重置空指针无异常 | 完全一致 |
| 扩展接口 | 支持`swap`，`make_unique`等 | 支持丰富工具函数 |

## 总结
本 `UniquePtr` 实现完整复刻了 `std::unique_ptr` 的核心能力，满足独占式智能指针的核心需求：
1. 确保资源独占，避免拷贝导致的多重释放；
2. 自动管理内存，析构时调用删除器释放资源；
3. 支持移动语义，灵活转移资源所有权；
4. 适配数组/普通类型，提供安全的访问接口。

适合学习智能指针原理、嵌入式/轻量级项目中替代标准库 `std::unique_ptr` 使用（需确保编译环境支持 C++11 及以上的移动语义）。
