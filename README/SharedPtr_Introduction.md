# SharedPtr 自定义实现 README
## 概述
本实现是对 C++ 标准库 `std::shared_ptr` 的轻量级模拟，是一款**共享式智能指针**，核心通过**原子化引用计数**管理动态分配的内存资源。多个 `SharedPtr` 可共享同一资源的所有权，当最后一个持有资源的 `SharedPtr` 析构/重置时，自动释放资源；同时兼容 `WeakPtr`（弱引用指针），可解决循环引用问题，避免内存泄漏。

本实现完整支持拷贝/移动语义、自定义删除器、原子化引用计数（线程安全计数操作），接口设计与标准库 `std::shared_ptr` 对齐，易于理解和扩展，适合学习智能指针核心原理或轻量级项目使用。

## 核心设计
### 1. 控制块（ControlBlock）
`SharedPtr` 的核心是**共享控制块**，所有指向同一资源的 `SharedPtr`/`WeakPtr` 共享同一个控制块，用于管理：
| 成员 | 作用 |
|------|------|
| `std::atomic<size_t> strong_count` | 强引用计数：记录当前持有资源的 `SharedPtr` 数量，原子类型保证线程安全 |
| `std::atomic<size_t> weak_count` | 弱引用计数：记录关联的 `WeakPtr` 数量，原子类型保证线程安全 |
| `T* ptr` | 指向托管资源的裸指针 |
| `Deleter deleter` | 资源删除器：用于自定义资源释放逻辑（默认 `default_delete`） |

### 2. 核心规则
- 强引用计数 `strong_count > 0`：资源存活，禁止释放；
- 强引用计数 `strong_count == 0`：销毁托管资源，若弱引用计数 `weak_count == 0`，则销毁控制块；
- 弱引用计数仅影响控制块的销毁时机，不影响资源本身的生命周期。

## 核心特性
| 特性 | 说明 |
|------|------|
| 共享所有权 | 支持拷贝构造/拷贝赋值，多个 `SharedPtr` 共享同一资源 |
| 原子引用计数 | 强/弱引用计数基于 `std::atomic`，计数操作（增减）线程安全 |
| 移动语义 | 支持移动构造/移动赋值，转移资源所有权时不修改引用计数 |
| 自定义删除器 | 支持自定义删除器（默认 `delete`/`delete[]`），适配不同资源释放逻辑 |
| 弱引用兼容 | 作为 `WeakPtr` 的友元类，支持弱引用关联和 `lock()` 生成有效 `SharedPtr` |
| 自动释放 | 最后一个强引用析构/重置时，自动调用删除器释放资源 |
| 空指针安全 | 构造/重置支持空指针，空指针操作无异常 |
| 数组特化（预留） | 注释中包含数组特化模板，可扩展支持数组类型（禁用 `*`/`->`，重载 `[]`） |

## 接口说明
### 1. 模板参数
```cpp
template <typename T, typename Deleter = default_delete<T>>
class SharedPtr;
```
- `T`：智能指针管理的对象类型（支持普通类型，预留数组特化 `T[]`）；
- `Deleter`：删除器类型，默认使用 `default_delete<T>`（适配 `delete`），数组版默认 `default_delete<T[]>`（适配 `delete[]`）。

### 2. 构造与析构
| 接口 | 说明 |
|------|------|
| `SharedPtr() noexcept` | 默认构造：创建空 `SharedPtr`，持有 `nullptr`，无控制块 |
| `explicit SharedPtr(T* ptr, const Deleter& deleter = Deleter{}) noexcept` | 裸指针构造：接管 `ptr` 的所有权，创建控制块（强计数初始化为 1），可指定自定义删除器 |
| `SharedPtr(const SharedPtr& other) noexcept` | 拷贝构造：共享 `other` 的控制块，强引用计数 +1 |
| `SharedPtr(SharedPtr&& other) noexcept` | 移动构造：转移 `other` 的资源/控制块所有权，`other` 后置为空（不修改计数） |
| `~SharedPtr() noexcept` | 析构函数：调用 `reset()` 减少强计数，必要时释放资源/控制块 |

### 3. 赋值运算符
| 接口 | 说明 |
|------|------|
| `SharedPtr& operator=(const SharedPtr& other) noexcept` | 拷贝赋值：先释放当前资源（`reset()`），再共享 `other` 的控制块，强计数 +1 |
| `SharedPtr& operator=(SharedPtr&& other) noexcept` | 移动赋值：先释放当前资源（`reset()`），再转移 `other` 的所有权，`other` 后置为空 |

### 4. 核心操作
| 接口 | 说明 |
|------|------|
| `T* get() const noexcept` | 获取托管资源的裸指针（仅访问，不释放所有权） |
| `size_t use_count() const noexcept` | 获取当前强引用计数（原子加载，线程安全） |
| `void reset(T* ptr = nullptr, const Deleter& deleter = Deleter{}) noexcept` | 重置资源：<br>1. 减少当前强计数，必要时释放资源/控制块；<br>2. 接管新指针 `ptr`，创建新控制块（若 `ptr` 非空） |
| `void swap(SharedPtr& other) noexcept` | 交换两个 `SharedPtr` 的资源和控制块（无计数修改） |
| `explicit operator bool() const noexcept` | 布尔转换：判断是否持有有效资源（`ptr_ != nullptr` 时返回 `true`） |

### 5. 运算符重载
| 运算符 | 说明 |
|--------|------|
| `T& operator*() const noexcept` | 解引用：返回托管资源的引用（需确保指针非空） |
| `T* operator->() const noexcept` | 成员访问：返回托管资源的裸指针（需确保指针非空） |

### 6. 预留接口（数组特化）
```cpp
// 数组特化模板（注释中预留）
template <typename T, typename Deleter>
class SharedPtr<T[], Deleter> : public SharedPtr<T, Deleter> {
public:
    using SharedPtr<T, Deleter>::SharedPtr;
    T& operator[](std::size_t idx) const; // 数组下标访问
    T& operator*() const = delete;       // 禁用解引用（数组不适用）
    T* operator->() const = delete;      // 禁用成员访问（数组不适用）
};
```

## 使用示例
### 示例 1：基础使用（共享所有权）
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 SharedPtr 及依赖的 default_delete/ControlBlock 定义

int main() {
    // 1. 构造 SharedPtr 管理单个对象
    SharedPtr<int> sp1(new int(100));
    std::cout << "sp1 强计数: " << sp1.use_count() << std::endl; // 输出：1
    std::cout << "sp1 取值: " << *sp1 << std::endl;             // 输出：100

    // 2. 拷贝构造：共享资源，强计数 +1
    SharedPtr<int> sp2(sp1);
    std::cout << "sp1 强计数（拷贝后）: " << sp1.use_count() << std::endl; // 输出：2
    std::cout << "sp2 取值: " << *sp2 << std::endl;                       // 输出：100

    // 3. 重置 sp2：释放 sp2 的所有权，强计数 -1
    sp2.reset();
    std::cout << "sp1 强计数（sp2 重置后）: " << sp1.use_count() << std::endl; // 输出：1

    return 0;
}
```

### 示例 2：移动语义
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 SharedPtr 及依赖定义

int main() {
    SharedPtr<std::string> sp1(new std::string("hello shared_ptr"));
    std::cout << "sp1 强计数: " << sp1.use_count() << std::endl; // 输出：1

    // 移动构造：sp1 所有权转移给 sp2，sp1 后置为空
    SharedPtr<std::string> sp2(std::move(sp1));
    std::cout << "sp2 取值: " << *sp2 << std::endl;             // 输出：hello shared_ptr
    std::cout << "sp1 是否为空: " << std::boolalpha << !sp1 << std::endl; // 输出：true
    std::cout << "sp2 强计数: " << sp2.use_count() << std::endl; // 输出：1

    return 0;
}
```

### 示例 3：自定义删除器
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 SharedPtr 及依赖定义

// 自定义删除器：打印日志并释放内存
struct CustomDeleter {
    void operator()(int* ptr) const noexcept {
        std::cout << "自定义删除器：释放 int 对象，值 = " << *ptr << std::endl;
        delete ptr;
    }
};

int main() {
    // 使用自定义删除器构造 SharedPtr
    SharedPtr<int, CustomDeleter> sp(new int(200), CustomDeleter{});
    std::cout << "sp 取值: " << *sp << std::endl; // 输出：200

    // 析构时自动调用自定义删除器
    return 0;
}
```
**输出**：
```
sp 取值: 200
自定义删除器：释放 int 对象，值 = 200
```

### 示例 4：配合 WeakPtr 使用（解决循环引用）
```cpp
#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
// 包含 SharedPtr/WeakPtr 及依赖定义

// 循环引用的节点类
struct Node {
    std::string name;
    WeakPtr<Node> peer; // 用 WeakPtr 避免循环引用

    Node(const std::string& n) : name(n) {
        std::cout << "构造 Node: " << name << std::endl;
    }
    ~Node() {
        std::cout << "析构 Node: " << name << std::endl;
    }
};

int main() {
    {
        SharedPtr<Node> nodeA(new Node("A"));
        SharedPtr<Node> nodeB(new Node("B"));

        // 互相引用：WeakPtr 不增加强计数
        nodeA->peer = nodeB;
        nodeB->peer = nodeA;

        // lock() 获取有效 SharedPtr
        SharedPtr<Node> peerOfA = nodeA->peer.lock();
        std::cout << "nodeA 的关联节点: " << peerOfA->name << std::endl; // 输出：B
    } // 作用域结束，节点正常析构，无内存泄漏

    return 0;
}
```
**输出**：
```
构造 Node: A
构造 Node: B
nodeA 的关联节点: B
析构 Node: B
析构 Node: A
```

## 注意事项
### 1. 线程安全
- 引用计数操作（`strong_count`/`weak_count` 的增减）基于 `std::atomic`，是线程安全的；
- 对托管资源的读写操作**非线程安全**，需自行加锁保护；
- 同一 `SharedPtr` 对象的多线程修改（如 `reset()`/赋值）**非线程安全**，需避免。

### 2. 循环引用问题
- 两个 `SharedPtr` 互相引用会导致强计数无法归 0，资源泄漏；
- 解决方案：将其中一个引用改为 `WeakPtr`（弱引用不增加强计数）。

### 3. 空指针与断言
- 解引用（`*`）、成员访问（`->`）未做空指针检查，空指针访问会导致未定义行为；
- 建议使用前通过 `if (sp)` 或 `sp.get() != nullptr` 检查指针有效性。

### 4. 裸指针构造风险
- 避免将同一裸指针传递给多个 `SharedPtr` 构造（会导致重复释放）；
- 示例：`SharedPtr<int> sp1(new int); SharedPtr<int> sp2(sp1.get());` → 双重释放！

### 5. 删除器兼容性
- 自定义删除器需满足 `noexcept`（建议）、可调用（重载 `operator()`）；
- 数组资源需使用 `default_delete<T[]>` 或适配 `delete[]` 的自定义删除器。

## 与 std::shared_ptr 的对比
| 特性 | 自定义 SharedPtr | std::shared_ptr |
|------|------------------|-----------------|
| 核心功能 | 共享所有权、原子计数、自动释放 | 完全一致 |
| 弱引用支持 | 兼容自定义 WeakPtr | 原生支持 `std::weak_ptr` |
| 数组特化 | 预留模板（未实现） | C++17 原生支持 `std::shared_ptr<T[]>` |
| 扩展接口 | 仅实现核心接口 | 支持 `make_shared`、`dynamic_pointer_cast` 等工具函数 |
| 性能 | 核心逻辑无冗余，轻量 | 高度优化，支持小对象优化（SSO） |
| 异常安全 | 基础异常安全 | 强异常安全保证 |

## 总结
本 `SharedPtr` 实现完整复刻了 `std::shared_ptr` 的核心原理和核心接口：
1. 通过**共享控制块 + 原子引用计数**实现资源的共享所有权；
2. 支持拷贝/移动语义，兼顾灵活性和性能；
3. 兼容 `WeakPtr` 解决循环引用问题，避免内存泄漏；
4. 支持自定义删除器，适配不同类型的资源释放逻辑。

适合用于学习智能指针的核心原理（控制块、引用计数、线程安全），或在轻量级项目中替代标准库 `std::shared_ptr`（需自行实现 `WeakPtr` 配合使用）。
