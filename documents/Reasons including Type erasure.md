挑战要求第一项“实现类型擦除的删除器（SharedPtr）”，核心是让 `SharedPtr` 能够**统一管理任意类型的自定义删除器**，而不需要在模板参数中暴露删除器的具体类型，这也是 C++ 标准库 `std::shared_ptr` 的关键设计之一。

---

### 1. 什么是删除器（Deleter）？
删除器是当 `SharedPtr` 引用计数归零时，用来释放资源的可调用对象（函数、函数对象、lambda 表达式等）。
- 默认删除器：`delete`（释放 `new` 分配的内存）。
- 自定义删除器：比如用 `free` 释放 `malloc` 的内存，或自定义资源释放逻辑。

示例：
```cpp
// 用 free 作为删除器，释放 malloc 分配的内存
SharedPtr<int> p(static_cast<int*>(malloc(sizeof(int))), [](int* ptr) { free(ptr); });
```

---

### 2. 什么是“类型擦除”？
如果没有类型擦除，`SharedPtr` 的模板参数需要包含删除器的类型，例如：
```cpp
// 伪代码：删除器类型作为模板参数
template <typename T, typename Deleter>
class SharedPtr { ... };
```
这会导致：
- 不同删除器的 `SharedPtr<T>` 是**不同类型**（如 `SharedPtr<int, DefaultDeleter>` 和 `SharedPtr<int, MyDeleter>`），无法互相赋值或存入同一容器。
- 代码灵活性大幅降低，不符合标准库的设计目标。

**类型擦除**的作用就是：让 `SharedPtr<T>` 无论使用何种删除器，都保持同一个类型，同时内部能正确调用对应的删除逻辑。

---

### 3. 实现原理（核心思路）
`SharedPtr` 内部通过**控制块（Control Block）**实现类型擦除：
1. **基类抽象**：定义一个删除器基类 `DeleterBase`，包含纯虚函数 `operator()` 用于释放资源。
2. **模板派生**：为每种具体删除器类型 `D`，派生模板类 `DeleterImpl<T, D>`，存储具体的删除器实例，并实现 `operator()`。
3. **控制块存储**：`SharedPtr` 的控制块中存储 `DeleterBase*` 指针，通过虚函数调用删除器，从而擦除删除器的具体类型。
4. **统一接口**：`SharedPtr<T>` 对外只暴露模板参数 `T`，删除器类型被隐藏在控制块内部。

当引用计数归零时，控制块通过 `DeleterBase*` 调用虚函数，自动匹配到对应的 `DeleterImpl` 实现，完成资源释放。

---

### 4. 为什么需要这个特性？
- **类型统一**：不同删除器的 `SharedPtr<T>` 是同一类型，可存入同一容器、互相赋值。
- **灵活性**：支持任意可调用对象作为删除器，无需修改 `SharedPtr` 模板定义。
- **符合标准**：与 `std::shared_ptr` 的行为一致，降低用户学习和迁移成本。

---

### 5. 简单示例（伪代码）
```cpp
// 基类：抽象删除器接口
struct DeleterBase {
    virtual void operator()(void* ptr) const = 0;
    virtual ~DeleterBase() = default;
};

// 模板派生类：存储具体删除器
template <typename T, typename Deleter>
struct DeleterImpl : DeleterBase {
    Deleter deleter;
    DeleterImpl(Deleter d) : deleter(std::move(d)) {}
    void operator()(void* ptr) const override {
        deleter(static_cast<T*>(ptr)); // 调用具体删除器
    }
};

// SharedPtr 控制块
struct ControlBlock {
    std::atomic<int> ref_count;
    DeleterBase* deleter; // 擦除类型的删除器指针
    // ...
};

// SharedPtr 类
template <typename T>
class SharedPtr {
    T* ptr;
    ControlBlock* cb;
public:
    // 构造时创建 DeleterImpl，擦除类型
    template <typename Deleter>
    SharedPtr(T* p, Deleter d) : ptr(p), cb(new ControlBlock{1, new DeleterImpl<T, Deleter>(d)}) {}
    
    // 析构时通过控制块调用删除器
    ~SharedPtr() {
        if (--cb->ref_count == 0) {
            (*cb->deleter)(ptr); // 虚函数调用，自动匹配具体删除器
            delete cb->deleter;
            delete cb;
        }
    }
};
```

---

如果你需要，我可以帮你把这个类型擦除删除器的完整实现代码（包括控制块、虚函数设计和测试用例）整理出来，方便你直接在 Visual Studio 2022 里编译运行。需要我帮你写吗？
