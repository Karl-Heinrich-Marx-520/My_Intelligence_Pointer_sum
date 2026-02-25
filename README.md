# C++ 智能指针实现
本项目从零实现 C++ 标准库风格的智能指针（`UniquePtr`/`SharedPtr`/`WeakPtr`），深入理解 RAII 机制、内存管理、引用计数及现代 C++ 核心特性（移动语义、类型擦除、原子操作等）。

## 项目简介
智能指针是 RAII（Resource Acquisition Is Initialization）机制的核心应用，本项目通过手动实现智能指针，掌握：
- 独占式/共享式内存所有权管理的核心逻辑
- 引用计数的设计与线程安全实现
- 循环引用问题的解决方案（WeakPtr）
- 现代 C++ 移动语义、类型擦除、异常安全等特性的实践

## 功能特性
### 基础要求（必实现）
| 智能指针 | 核心功能 |
|----------|----------|
| `UniquePtr` | 独占所有权（禁止拷贝、支持移动）<br>核心接口：`release()`/`reset()`/`get()`/`operator*`/`operator->`<br>支持自定义删除器 |
| `SharedPtr` | 共享所有权（支持拷贝/移动）<br>引用计数管理<br>核心接口：`use_count()`/`reset()`/`get()`/`operator*`/`operator->` |

### 进阶要求（推荐实现）
- `WeakPtr`：打破循环引用，支持 `lock()`/`expired()`/`use_count()`
- 实现 `make_unique`/`make_shared` 函数模板（异常安全的对象创建）
- 支持数组类型（`UniquePtr<T[]>`/`SharedPtr<T[]>`）
- 引用计数的线程安全（基于原子操作 `std::atomic`）

### 挑战要求（拓展实现）
- `SharedPtr` 类型擦除的删除器（兼容任意类型的删除器，无需模板参数）
- `enable_shared_from_this`：允许对象获取自身的 `SharedPtr`
- 别名构造（aliasing constructor）：分离指针指向的对象与控制块
- `make_shared` 优化：控制块与对象内存单次分配（减少内存碎片）

## 核心实现细节
### 1. RAII 核心机制
所有智能指针均遵循 RAII 原则：
- **构造时**：获取资源（接管裸指针）
- **析构时**：释放资源（自动调用删除器，无需手动 `delete`）
- 禁止裸指针的手动管理，消除内存泄漏风险

### 2. 控制块设计（SharedPtr/WeakPtr）
`SharedPtr` 的核心是**控制块（Control Block）**，统一管理：
```cpp
struct ControlBlockBase {
    std::atomic<size_t> ref_count;    // 共享引用计数（SharedPtr 数量）
    std::atomic<size_t> weak_count;   // 弱引用计数（WeakPtr 数量）
    virtual ~ControlBlockBase() = default;
    virtual void destroy() = 0;       // 销毁管理的对象
    virtual void deallocate() = 0;    // 释放控制块内存
};

// 普通对象的控制块
template <typename T, typename Deleter>
struct ObjectControlBlock : ControlBlockBase {
    T* ptr;
    Deleter deleter;
    // ... 构造/析构/销毁逻辑
};
```
- `ref_count` 为 0 时，销毁管理的对象；
- `weak_count` 为 0 时，释放控制块内存。

### 3. 线程安全
- 引用计数（`ref_count`/`weak_count`）使用 `std::atomic` 实现原子操作，保证多线程下计数正确；
- 智能指针对象本身**非线程安全**（符合标准库行为），需用户自行保证并发访问的互斥。

### 4. 类型擦除的删除器
`SharedPtr` 不将删除器作为模板参数（与标准库一致），通过控制块的多态实现类型擦除：
- 不同删除器的控制块继承自统一的 `ControlBlockBase`；
- 销毁对象时调用控制块的虚函数 `destroy()`，无需关心删除器具体类型。

## 使用示例
### 1. UniquePtr（独占指针）
```cpp
// 基础使用
auto up1 = make_unique<int>(42);
std::cout << *up1 << std::endl;  // 输出：42

// 移动语义（禁止拷贝）
UniquePtr<int> up2 = std::move(up1);
assert(up1.get() == nullptr);    // up1 已释放所有权

// 自定义删除器（文件句柄）
UniquePtr<FILE, decltype(&fclose)> file_ptr(fopen("test.txt", "r"), &fclose);

// 数组支持
UniquePtr<int[]> arr_ptr = make_unique<int[]>(5);
arr_ptr[0] = 10;
arr_ptr[1] = 20;
```

### 2. SharedPtr（共享指针）
```cpp
// 基础使用
auto sp1 = make_shared<std::string>("hello");
SharedPtr<std::string> sp2 = sp1;
assert(sp1.use_count() == 2);    // 引用计数为 2

// 移动语义
SharedPtr<std::string> sp3 = std::move(sp2);
assert(sp2.get() == nullptr);
assert(sp1.use_count() == 2);    // 移动不改变引用计数

// 数组支持
auto arr_sp = make_shared<int[]>(3);
arr_sp[0] = 1;
arr_sp[1] = 2;
```

### 3. WeakPtr（弱引用）
```cpp
// 打破循环引用
struct Node {
    WeakPtr<Node> next;  // 用 WeakPtr 替代 SharedPtr，避免循环引用
};

auto n1 = make_shared<Node>();
auto n2 = make_shared<Node>();
n1->next = n2;
n2->next = n1;

// WeakPtr 核心接口
WeakPtr<std::string> wp = sp1;
assert(!wp.expired());           // 检查引用是否有效
if (auto sp4 = wp.lock()) {      // 升级为 SharedPtr（原子操作）
    std::cout << *sp4 << std::endl;  // 输出：hello
}
assert(wp.use_count() == 2);    // 弱引用不影响共享引用计数
```

### 4. enable_shared_from_this
```cpp
class MyClass : public enable_shared_from_this<MyClass> {
public:
    SharedPtr<MyClass> get_self() {
        return shared_from_this();  // 获取自身的 SharedPtr
    }
};

auto obj = make_shared<MyClass>();
auto self_ptr = obj->get_self();
assert(obj.use_count() == 2);
```

## 测试说明
需覆盖以下核心测试场景，推荐使用 Google Test 或原生 `assert` 编写测试用例：

### 1. 基本生命周期测试
- 智能指针对象析构时自动释放资源，无内存泄漏；
- `release()`/`reset()` 正确释放/转移所有权。

### 2. 引用计数正确性测试
- `SharedPtr` 拷贝/移动/析构时，引用计数正确增减；
- `WeakPtr` 不影响共享引用计数，仅影响弱引用计数。

### 3. 循环引用与 WeakPtr 测试
- 纯 `SharedPtr` 形成的循环引用导致内存泄漏；
- 引入 `WeakPtr` 后，对象能正常析构，无内存泄漏。

### 4. 异常安全测试
- `make_shared`/`make_unique` 在对象构造抛异常时，无内存泄漏；
- 智能指针操作（如 `reset`）过程中抛异常，资源仍能正确释放。

### 5. 移动语义测试
- `UniquePtr` 拷贝构造/赋值被禁用（编译报错）；
- 移动后原指针置空，所有权转移正确；
- `SharedPtr` 移动后引用计数不变（仅转移所有权，不增加计数）。

## 编译与运行
### 编译要求
- C++11 及以上标准（需支持移动语义、原子操作、模板别名等）；
- 编译器：GCC 7+/Clang 6+/MSVC 2017+。

### 编译命令
```bash
# 基础编译（g++）
g++ -std=c++17 -o smart_ptr_test smart_ptr.cpp test.cpp -pthread

# 运行测试
./smart_ptr_test
```

## 核心知识点总结
### 1. 关键机制
- **RAII**：智能指针的核心，通过对象生命周期管理资源，析构时自动释放；
- **移动语义**：`UniquePtr` 实现独占所有权的核心，`std::move` 转移所有权而非拷贝；
- **引用计数**：`SharedPtr` 通过控制块管理计数，计数为 0 时销毁对象，解决内存泄漏。

### 2. 核心问题
- **循环引用**：两个 `SharedPtr` 互相引用导致计数无法归 0，`WeakPtr` 弱引用不增加计数，打破循环；
- **线程安全**：仅引用计数的原子操作保证线程安全，智能指针对象的读写仍需加锁；
- **异常安全**：`make_shared` 优化内存分配，避免对象构造失败导致的内存泄漏。

### 3. 拓展特性
- **类型擦除**：`SharedPtr` 控制块的多态设计，兼容任意类型的删除器，无需暴露删除器类型；
- **enable_shared_from_this**：通过内部 `WeakPtr` 关联控制块，避免裸指针构造 `SharedPtr` 导致的重复计数。

## 注意事项
1. 禁止使用标准库智能指针（`std::unique_ptr`/`std::shared_ptr`），需手动实现所有逻辑；
2. 自定义删除器需兼容任意可调用类型（函数指针、lambda、函数对象）；
3. 数组类型需重载 `operator[]`，并在删除时使用 `delete[]`（而非 `delete`）；
4. 异常安全需覆盖所有边界场景（如对象构造抛异常、删除器抛异常等）。

## 参考资料
- C++ 标准库文档（智能指针部分）
- 《Effective Modern C++》（Scott Meyers）
- 《C++ Concurrency in Action》（Anthony Williams）
- RAII 与智能指针核心原理
